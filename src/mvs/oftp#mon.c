/* oftp#mon.c - the monitor: sessions, listener, scheduler, control port
** and console commands (MVS only)
**
** Every session runs in a thread of its own. Its file callbacks go to
** the spool (one record per transfer) and to the data set layer; the
** session engine does the protocol. No writable static data: OFTPD is
** AC(1) and reentrant, everything lives in the OFTP_MON block.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mvs/lock.h>
#include <mvs/rfile.h>
#include <mvs/wto.h>

#include "oftpdsf.h"
#include "oftpmon.h"
#include "oftpnet.h"
#include "oftpsess.h"

void
oftp_mon_msg(const char *fmt, ...)
{
    char line[124];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    wto(line);
}

/* ---- helpers ----------------------------------------------------- */

static int
ptn_index(OFTP_MON *m, const OFTP_PARTNER *p)
{
    return (int)(p - m->cfg.ptn);
}

/* A data set name: 1..44 characters, qualifiers of 1..8 starting with a
** letter or national character
*/
static int
valid_dsn(const char *d)
{
    int q = 0, len = 0;

    for (; *d; d++, len++) {
        char c = *d;

        if (c == '.') {
            if (q == 0)
                return 0;
            q = 0;
            continue;
        }
        if (q == 0 && !((c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R')
                        || (c >= 'S' && c <= 'Z') || c == '@' || c == '#'
                        || c == '$'))
            return 0;
        if (!((c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R')
              || (c >= 'S' && c <= 'Z') || (c >= '0' && c <= '9')
              || c == '@' || c == '#' || c == '$' || c == '-'))
            return 0;
        if (++q > 8)
            return 0;
    }
    return q > 0 && len <= 44;
}

static int
exists(const char *dsn)
{
    RFILE *rf;
    char q[60];

    snprintf(q, sizeof(q), "'%s'", dsn);
    if (ropen(q, 0, &rf) != 0)
        return 0;
    rclose(rf);
    return 1;
}

static void
settext(OFTP_SPREC *r, const char *text)
{
    snprintf(r->text, sizeof(r->text), "%s", text);
}

/* ---- one session ------------------------------------------------- */

typedef struct sesctx {
    OFTP_MON   *m;
    OFTP_PARTNER *pt;
    char        ip[16];
    int         sock;
    int         initiator;
    OFTP_SPREC  out;                    /* the file being sent           */
    OFTP_DSF   *odsf;
    OFTP_SPREC  in;                     /* the file being received       */
    OFTP_DSF   *idsf;
    char        tmp[45];
    char        erpkey[8];
    OFTP_SESS   s;
} SESCTX;

static int
f_next_out(void *ctx, const OFTP_PEER *peer, OFTP_SFIDCMD *f, OFTP_FH **fh)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    char last[8], name[56];
    long now = (long)time(NULL);

    (void)peer;
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_SEND || strcmp(r.partner, c->pt->peer.name) != 0)
            continue;
        if (r.state != SPL_QUEUED && !(r.state == SPL_WAITING && r.tnext <= now))
            continue;
        if (r.member[0])
            snprintf(name, sizeof(name), "%s(%s)", r.local, r.member);
        else
            snprintf(name, sizeof(name), "%s", r.local);
        if (oftp_dsf_open_out(name, r.fmt == 'T' ? 'T' : 0, r.xlate == 'Y',
                              f, &c->odsf) != 0) {
            r.state = SPL_FAILED;
            r.reasby = 'L';
            r.reas = 12;
            settext(&r, "data set cannot be opened");
            oftp_spl_put(c->m->spool, &r);
            oftp_mon_msg("OFTP210E %.8s %s: %s CANNOT BE OPENED", r.key,
                         r.partner, name);
            continue;
        }
        r.fmt = f->fmt;
        r.lrecl = f->lrecl;
        strcpy(f->dsn, r.dsn);
        strcpy(f->date, r.date);
        strcpy(f->time, r.time);
        strcpy(f->user, r.user);
        strcpy(f->dest, c->pt->peer.code);
        strcpy(f->orig, c->pt->peer.ourcode);
        r.state = SPL_ACTIVE;
        r.attempts++;
        r.tlast = now;
        oftp_spl_put(c->m->spool, &r);
        c->out = r;
        *fh = c;
        return 1;
    }
    return 0;
}

static int
f_read(void *ctx, OFTP_FH *fh, unsigned char *b, unsigned cap, unsigned *n,
       int *last)
{
    SESCTX *c = ctx;

    (void)fh;
    return oftp_dsf_read(c->odsf, b, cap, n, last);
}

static void
f_out_done(void *ctx, OFTP_FH *fh, int reas, char retry)
{
    SESCTX *c = ctx;
    OFTP_SPREC *r = &c->out;
    char t[80];

    (void)fh;
    r->recs = oftp_dsf_records(c->odsf);
    oftp_dsf_close(c->odsf, 1);
    c->odsf = NULL;
    r->tlast = (long)time(NULL);
    if (reas == 0) {
        r->state = SPL_SENT;
        r->reas = 0;
        settext(r, "accepted by the partner, EERP outstanding");
        oftp_mon_msg("OFTP211I %.8s %s: %s SENT, %lu RECORDS", r->key,
                     r->partner, r->dsn, r->recs);
    } else {
        r->reas = reas;
        r->reasby = 'P';
        if (retry == 'Y') {
            r->state = SPL_WAITING;
            r->tnext = r->tlast + c->m->cfg.retry;
        } else
            r->state = SPL_REJECTED;
        snprintf(t, sizeof(t), "refused by the partner, reason %02d", reas);
        settext(r, t);
        oftp_mon_msg("OFTP212W %.8s %s: %s REFUSED, REASON %02d RETRY %c",
                     r->key, r->partner, r->dsn, reas, retry);
    }
    oftp_spl_put(c->m->spool, r);
    r->key[0] = '\0';
}

static int
f_open_in(void *ctx, const OFTP_PEER *peer, const OFTP_SFIDCMD *f,
          OFTP_FH **fh, char *retry)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    OFTP_DSOPT opt;
    char final[45], last[8];
    int rc;

    (void)peer;
    *retry = 'N';
    if (strlen(c->m->cfg.inprefix) + 1 + strlen(f->dsn) > 44)
        return 1;
    snprintf(final, sizeof(final), "%s.%s", c->m->cfg.inprefix, f->dsn);
    if (!valid_dsn(final))
        return 1;

    /* the same virtual file again: answer its EERP once more */
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir == SPL_RECV && strcmp(r.orig, f->orig) == 0
            && strcmp(r.dsn, f->dsn) == 0 && strcmp(r.date, f->date) == 0
            && strcmp(r.time, f->time) == 0) {
            if (r.state == SPL_CONFIRMED) {
                r.state = SPL_RECEIVED;
                oftp_spl_put(c->m->spool, &r);
            }
            return 13;
        }
    }
    if (exists(final)) {
        *retry = 'Y';
        return 13;
    }

    memset(&r, 0, sizeof(r));
    r.dir = SPL_RECV;
    r.state = SPL_ACTIVE;
    r.fmt = f->fmt;
    r.lrecl = f->lrecl;
    r.xlate = c->pt->xlate ? 'Y' : 'N';
    strcpy(r.partner, c->pt->peer.name);
    strcpy(r.dsn, f->dsn);
    strcpy(r.date, f->date);
    strcpy(r.time, f->time);
    strcpy(r.user, f->user);
    strcpy(r.dest, f->dest);
    strcpy(r.orig, f->orig);
    strcpy(r.local, final);
    r.tqueued = r.tlast = (long)time(NULL);
    if (oftp_spl_add(c->m->spool, &r) != 0) {
        *retry = 'Y';
        return 99;
    }
    snprintf(c->tmp, sizeof(c->tmp), "%s.%.8s", c->m->cfg.tmpprefix, r.key);
    memset(&opt, 0, sizeof(opt));
    opt.tlrecl = c->m->cfg.tlrecl;
    opt.ublksize = c->m->cfg.ublksize;
    strcpy(opt.unit, c->m->cfg.unit);
    strcpy(opt.volser, c->m->cfg.volser);
    rc = oftp_dsf_open_in(c->tmp, f, r.xlate == 'Y', &opt, &c->idsf);
    if (rc != 0) {
        oftp_spl_del(c->m->spool, r.key);
        oftp_mon_msg("OFTP220E %s: %s NOT RECEIVED, %s NOT ALLOCATED (%02d)",
                     r.partner, f->dsn, c->tmp, rc);
        *retry = 'Y';
        return rc;
    }
    c->in = r;
    *fh = c;
    return 0;
}

static int
f_write(void *ctx, OFTP_FH *fh, const unsigned char *b, unsigned n, int eor)
{
    SESCTX *c = ctx;

    (void)fh;
    return oftp_dsf_write(c->idsf, b, n, eor);
}

static int
f_close_in(void *ctx, OFTP_FH *fh, int ok)
{
    SESCTX *c = ctx;
    OFTP_SPREC *r = &c->in;
    int rc;

    (void)fh;
    r->recs = oftp_dsf_records(c->idsf);
    rc = oftp_dsf_close(c->idsf, ok);
    c->idsf = NULL;
    if (!ok || rc != 0) {
        oftp_spl_del(c->m->spool, r->key);
        r->key[0] = '\0';
        return rc;
    }
    if (rename(c->tmp, r->local) != 0) {
        remove(c->tmp);
        oftp_spl_del(c->m->spool, r->key);
        oftp_mon_msg("OFTP221E %s: %s NOT RENAMED TO %s", r->partner,
                     c->tmp, r->local);
        r->key[0] = '\0';
        return 12;
    }
    r->state = SPL_RECEIVED;
    r->tdone = (long)time(NULL);
    settext(r, "received, EERP owed");
    oftp_spl_put(c->m->spool, r);
    oftp_mon_msg("OFTP222I %.8s %s: %s RECEIVED AS %s, %lu RECORDS", r->key,
                 r->partner, r->dsn, r->local, r->recs);
    r->key[0] = '\0';
    return 0;
}

static int
f_next_erp(void *ctx, const OFTP_PEER *peer, char *code, OFTP_ERPCMD *e,
           OFTP_FH **h)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    char last[8];

    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_RECV || r.state != SPL_RECEIVED
            || strcmp(r.partner, c->pt->peer.name) != 0)
            continue;
        memset(e, 0, sizeof(*e));
        *code = OFTP_EERP;
        strcpy(e->dsn, r.dsn);
        strcpy(e->date, r.date);
        strcpy(e->time, r.time);
        strcpy(e->user, r.user);
        strcpy(e->dest, r.orig);
        strcpy(e->orig, peer->ourcode);
        memcpy(c->erpkey, r.key, 8);
        *h = c;
        return 1;
    }
    return 0;
}

static void
f_erp_sent(void *ctx, OFTP_FH *h)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;

    (void)h;
    if (oftp_spl_get(c->m->spool, c->erpkey, &r) == 0) {
        r.state = SPL_CONFIRMED;
        settext(&r, "received, EERP sent");
        oftp_spl_put(c->m->spool, &r);
    }
}

static void
f_erp_recv(void *ctx, const OFTP_PEER *peer, char code, const OFTP_ERPCMD *e)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    char last[8], t[80];

    (void)peer;
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_SEND || strcmp(r.partner, c->pt->peer.name) != 0
            || strcmp(r.dsn, e->dsn) != 0 || strcmp(r.date, e->date) != 0
            || strcmp(r.time, e->time) != 0)
            continue;
        r.erp = code;
        r.tdone = (long)time(NULL);
        if (code == OFTP_EERP) {
            r.state = SPL_DELIVERED;
            settext(&r, "delivered, EERP received");
        } else {
            r.state = SPL_REJECTED;
            r.reas = e->reas;
            r.reasby = 'P';
            snprintf(t, sizeof(t), "NERP received, reason %02d", e->reas);
            settext(&r, t);
        }
        oftp_spl_put(c->m->spool, &r);
        oftp_mon_msg("OFTP213I %.8s %s: %s %s", r.key, r.partner, r.dsn,
                     code == OFTP_EERP ? "DELIVERED" : "NOT DELIVERED (NERP)");
        return;
    }
    oftp_mon_msg("OFTP214W %s: %s FOR UNKNOWN FILE %s", c->pt->peer.name,
                 code == OFTP_EERP ? "EERP" : "NERP", e->dsn);
}

static int
p_lookup(void *ctx, const char *code, OFTP_PEER *peer)
{
    SESCTX *c = ctx;
    OFTP_PARTNER *p = oftp_cfg_byid(&c->m->cfg, code);
    int i;

    if (p == NULL)
        return -1;
    if (p->address[0] && strcmp(p->address, c->ip) != 0) {
        oftp_mon_msg("OFTP201W %s CALLED FROM %s, NOT %s", p->peer.name,
                     c->ip, p->address);
        return -1;
    }
    i = ptn_index(c->m, p);
    lock(c->m, 0);
    if (c->m->busy[i]) {
        unlock(c->m, 0);
        oftp_mon_msg("OFTP202W %s ALREADY IN A SESSION", p->peer.name);
        return -1;
    }
    c->m->busy[i] = 1;
    unlock(c->m, 0);
    c->pt = p;
    *peer = p->peer;
    return 0;
}

static void
s_log(void *ctx, const char *line)
{
    SESCTX *c = ctx;

    oftp_mon_msg("OFTP290I %s: %s", c->pt ? c->pt->peer.name : c->ip, line);
}

static int
session_thread(void *arg1, void *arg2)
{
    SESCTX *c = arg1;
    OFTP_NETX nx;
    OFTP_MON *m = c->m;
    int rc;

    (void)arg2;
    nx.sock = c->sock;
    nx.stop = &m->stop;
    oftp_net_xport(&nx, &c->s.x);
    c->s.f.ctx = c;
    c->s.f.next_out = f_next_out;
    c->s.f.read = f_read;
    c->s.f.out_done = f_out_done;
    c->s.f.open_in = f_open_in;
    c->s.f.write = f_write;
    c->s.f.close_in = f_close_in;
    c->s.f.next_erp = f_next_erp;
    c->s.f.erp_sent = f_erp_sent;
    c->s.f.erp_recv = f_erp_recv;
    c->s.p.ctx = c;
    c->s.p.lookup = p_lookup;
    c->s.log = s_log;
    c->s.logctx = c;
    c->s.initiator = c->initiator;
    c->s.timeout = m->cfg.timeout;
    strcpy(c->s.localcode, m->cfg.odetteid);
    if (c->initiator)
        c->s.peer = c->pt->peer;

    rc = oftp_sess_run(&c->s);

    /* a file cut off in the middle: back to waiting, or away */
    if (c->odsf != NULL) {
        oftp_dsf_close(c->odsf, 1);
        c->out.state = SPL_WAITING;
        c->out.tnext = (long)time(NULL) + m->cfg.retry;
        settext(&c->out, "session ended during the transfer");
        oftp_spl_put(m->spool, &c->out);
    }
    if (c->idsf != NULL) {
        oftp_dsf_close(c->idsf, 0);
        oftp_spl_del(m->spool, c->in.key);
    }
    oftp_net_close(c->sock);
    oftp_mon_msg("OFTP203I %s: SESSION ENDED, ESID %02d %s, FILES %u OUT %u IN",
                 c->pt ? c->pt->peer.name : c->ip, rc,
                 c->s.esid_local ? "(OURS)" : "(PARTNER)",
                 c->s.files_out, c->s.files_in);
    lock(m, 0);
    if (c->pt)
        m->busy[ptn_index(m, c->pt)] = 0;
    m->nsess--;
    unlock(m, 0);
    free(c);
    return 0;
}

static int
start_session(OFTP_MON *m, int sock, const char *ip, OFTP_PARTNER *pt)
{
    SESCTX *c;
    CTHDTASK *t;

    if ((c = calloc(1, sizeof(*c))) == NULL) {
        oftp_net_close(sock);
        return -1;
    }
    c->m = m;
    c->sock = sock;
    c->pt = pt;
    c->initiator = pt != NULL;
    snprintf(c->ip, sizeof(c->ip), "%s", ip);
    lock(m, 0);
    m->nsess++;
    unlock(m, 0);
    t = cthread_create_ex(session_thread, c, NULL, 64 * 1024);
    if (t == NULL) {
        lock(m, 0);
        m->nsess--;
        if (pt)
            m->busy[ptn_index(m, pt)] = 0;
        unlock(m, 0);
        oftp_net_close(sock);
        free(c);
        return -1;
    }
    cthread_detach(t);
    return 0;
}

/* ---- listener and scheduler -------------------------------------- */

static int
listen_thread(void *arg1, void *arg2)
{
    OFTP_MON *m = arg1;
    char ip[16];

    (void)arg2;
    while (!m->stop) {
        int s;
        ECB e = 0;

        ecb_timed_wait(&e, 100, 0);
        if (m->stop)
            break;
        if ((s = oftp_net_accept(m->lsock, ip, sizeof(ip))) < 0)
            continue;
        if (m->nsess >= m->cfg.maxsess) {
            oftp_mon_msg("OFTP204W CALL FROM %s REFUSED, %d SESSIONS",
                         ip, m->nsess);
            oftp_net_close(s);
            continue;
        }
        oftp_mon_msg("OFTP205I CALL FROM %s", ip);
        start_session(m, s, ip, NULL);
    }
    return 0;
}

/* Marks each partner that has work: a send due or an EERP owed */
static void
due(OFTP_MON *m, char *work)
{
    OFTP_SPREC r;
    char last[8];
    long now = (long)time(NULL);
    int i;

    memset(work, 0, OFTP_MAXPTN);
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if ((r.dir == SPL_SEND && (r.state == SPL_QUEUED
                                   || (r.state == SPL_WAITING && r.tnext <= now)))
            || (r.dir == SPL_RECV && r.state == SPL_RECEIVED))
            for (i = 0; i < m->cfg.nptn; i++)
                if (strcmp(m->cfg.ptn[i].peer.name, r.partner) == 0)
                    work[i] = 1;
    }
}

static int
sched_thread(void *arg1, void *arg2)
{
    OFTP_MON *m = arg1;
    char work[OFTP_MAXPTN];
    int i;

    (void)arg2;
    while (!m->stop) {
        m->wake = 0;
        ecb_timed_wait(&m->wake, 1000, 0);     /* 10 s or when woken */
        if (m->stop)
            break;
        due(m, work);
        for (i = 0; i < m->cfg.nptn && !m->stop; i++) {
            OFTP_PARTNER *p = &m->cfg.ptn[i];
            int s;

            if (!(work[i] || m->callreq[i]) || !p->enabled || !p->call)
                continue;
            lock(m, 0);
            if (m->busy[i] || m->nsess >= m->cfg.maxsess) {
                unlock(m, 0);
                continue;
            }
            m->busy[i] = 1;
            unlock(m, 0);
            m->callreq[i] = 0;
            oftp_mon_msg("OFTP206I CALLING %s AT %s PORT %d", p->peer.name,
                         p->host, p->port);
            if ((s = oftp_net_connect(p->host, p->port, 5)) < 0) {
                oftp_mon_msg("OFTP207W %s NOT REACHED", p->peer.name);
                lock(m, 0);
                m->busy[i] = 0;
                unlock(m, 0);
                continue;
            }
            start_session(m, s, p->host, p);
        }
    }
    return 0;
}

/* ---- control port ------------------------------------------------ */

/* KEYWORD=VALUE from a request line, or "" */
static void
kw(const char *line, const char *key, char *out, size_t cap)
{
    size_t kl = strlen(key);
    const char *p = line;

    out[0] = '\0';
    while ((p = strstr(p, key)) != NULL) {
        if ((p == line || p[-1] == ' ') && p[kl] == '=') {
            size_t n = 0;

            p += kl + 1;
            while (*p && *p != ' ' && n < cap - 1)
                out[n++] = *p++;
            out[n] = '\0';
            return;
        }
        p += kl;
    }
}

static void
ctl_send(OFTP_MON *m, int s, const char *line)
{
    OFTP_SPREC r;
    OFTP_PARTNER *p;
    char partner[16], dsn[60], vfn[40], fmt[4], xl[4], user[12], reply[80];
    char *lp;
    struct tm *tm;
    time_t now = time(NULL);

    kw(line, "PARTNER", partner, sizeof(partner));
    kw(line, "DSN", dsn, sizeof(dsn));
    kw(line, "VFN", vfn, sizeof(vfn));
    kw(line, "FMT", fmt, sizeof(fmt));
    kw(line, "XLATE", xl, sizeof(xl));
    kw(line, "USER", user, sizeof(user));
    if ((p = oftp_cfg_find(&m->cfg, partner)) == NULL) {
        oftp_net_puts(s, "501 unknown or disabled partner");
        return;
    }
    memset(&r, 0, sizeof(r));
    if ((lp = strchr(dsn, '(')) != NULL) {
        char *e = strchr(lp, ')');

        *lp = '\0';
        if (e)
            *e = '\0';
        snprintf(r.member, sizeof(r.member), "%s", lp + 1);
    }
    if (!valid_dsn(dsn) || !exists(dsn)) {
        oftp_net_puts(s, "502 data set not found");
        return;
    }
    if (vfn[0] == '\0') {
        if (strlen(r.member[0] ? r.member : dsn) > OFTP_DSNLEN) {
            oftp_net_puts(s, "503 name longer than 26, give VFN=");
            return;
        }
        strcpy(vfn, r.member[0] ? r.member : dsn);
    }
    if (strlen(vfn) > OFTP_DSNLEN) {
        oftp_net_puts(s, "503 VFN longer than 26");
        return;
    }
    r.dir = SPL_SEND;
    r.state = SPL_QUEUED;
    r.fmt = fmt[0] == 'T' ? 'T' : 0;
    r.xlate = xl[0] == 'Y' ? 'Y' : 'N';
    strcpy(r.partner, p->peer.name);
    snprintf(r.owner, sizeof(r.owner), "%s", user);
    snprintf(r.user, sizeof(r.user), "%s", user);
    strcpy(r.dsn, vfn);
    strcpy(r.local, dsn);
    strcpy(r.dest, p->peer.code);
    strcpy(r.orig, p->peer.ourcode);
    tm = localtime(&now);
    lock(m, 0);
    m->counter = m->counter % 9999 + 1;
    snprintf(r.date, sizeof(r.date), "%04d%02d%02d", tm->tm_year + 1900,
             tm->tm_mon + 1, tm->tm_mday);
    snprintf(r.time, sizeof(r.time), "%02d%02d%02d%04u", tm->tm_hour,
             tm->tm_min, tm->tm_sec, m->counter);
    unlock(m, 0);
    r.tqueued = (long)now;
    settext(&r, "queued");
    if (oftp_spl_add(m->spool, &r) != 0) {
        oftp_net_puts(s, "451 spool not available");
        return;
    }
    snprintf(reply, sizeof(reply), "200 %.8s queued for %s as %s", r.key,
             r.partner, r.dsn);
    oftp_net_puts(s, reply);
    oftp_mon_msg("OFTP230I %.8s QUEUED FOR %s: %s AS %s", r.key, r.partner,
                 dsn, r.dsn);
    ecb_post(&m->wake, 0);
}

static void
ctl_list(OFTP_MON *m, int s, const char *line)
{
    OFTP_SPREC r;
    char last[8], partner[16], out[200];

    kw(line, "PARTNER", partner, sizeof(partner));
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (partner[0] && strcmp(partner, r.partner) != 0)
            continue;
        snprintf(out, sizeof(out), "100 %.8s %c %c %-8s %-26s %s %s",
                 r.key, r.dir, r.state, r.partner, r.dsn, r.local, r.text);
        oftp_net_puts(s, out);
    }
    oftp_net_puts(s, "200 end of list");
}

static int
ctl_thread(void *arg1, void *arg2)
{
    OFTP_MON *m = arg1;
    char ip[16], line[256];

    (void)arg2;
    while (!m->stop) {
        int s, n;
        ECB e = 0;

        ecb_timed_wait(&e, 100, 0);
        if (m->stop)
            break;
        if ((s = oftp_net_accept(m->csock, ip, sizeof(ip))) < 0)
            continue;
        /* one request per connection; no security during development */
        oftp_net_puts(s, "220 OFTPD control port");
        n = oftp_net_gets(s, line, sizeof(line), 30);
        if (n > 0) {
            char *c;

            for (c = line; *c; c++)
                if (*c >= 'a' && *c <= 'z')
                    *c = (char)(*c - 'a' + 'A');
            if (strncmp(line, "SEND ", 5) == 0)
                ctl_send(m, s, line);
            else if (strncmp(line, "LIST", 4) == 0)
                ctl_list(m, s, line);
            else
                oftp_net_puts(s, "500 unknown request");
        }
        oftp_net_close(s);
    }
    return 0;
}

/* ---- start, stop, recovery, console ------------------------------ */

int
oftp_mon_start(OFTP_MON *m)
{
    int err;

    if ((m->lsock = oftp_net_listen(m->cfg.port, &err)) < 0) {
        oftp_mon_msg("OFTP050E LISTEN ON PORT %d FAILED, ERRNO %d",
                     m->cfg.port, err);
        return -1;
    }
    oftp_mon_msg("OFTP051I LISTENING ON PORT %d", m->cfg.port);
    m->csock = -1;
    if (m->cfg.ctlport > 0) {
        if ((m->csock = oftp_net_listen(m->cfg.ctlport, &err)) < 0)
            oftp_mon_msg("OFTP052W CONTROL PORT %d NOT OPEN, ERRNO %d",
                         m->cfg.ctlport, err);
        else
            oftp_mon_msg("OFTP053I CONTROL PORT %d (NO SECURITY)",
                         m->cfg.ctlport);
    }
    m->t_listen = cthread_create_ex(listen_thread, m, NULL, 32 * 1024);
    m->t_sched = cthread_create_ex(sched_thread, m, NULL, 32 * 1024);
    if (m->csock >= 0)
        m->t_ctl = cthread_create_ex(ctl_thread, m, NULL, 32 * 1024);
    return m->t_listen && m->t_sched ? 0 : -1;
}

static void
join(CTHDTASK **t)
{
    if (*t == NULL)
        return;
    ecb_wait(&(*t)->termecb);
    cthread_delete(t);
}

void
oftp_mon_stop(OFTP_MON *m)
{
    int i;

    m->stop = 1;
    ecb_post(&m->wake, 0);
    join(&m->t_listen);
    join(&m->t_sched);
    join(&m->t_ctl);
    oftp_net_close(m->lsock);
    oftp_net_close(m->csock);
    for (i = 0; m->nsess > 0 && i < 60; i++) {
        ECB e = 0;

        if (i % 10 == 0)
            oftp_mon_msg("OFTP095I WAITING FOR %d SESSIONS", m->nsess);
        ecb_timed_wait(&e, 100, 0);
    }
}

void
oftp_mon_recover(OFTP_MON *m)
{
    OFTP_SPREC r;
    char last[8], tmp[45];
    int sends = 0, recvs = 0;

    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.state != SPL_ACTIVE)
            continue;
        if (r.dir == SPL_SEND) {
            r.state = SPL_WAITING;
            r.tnext = 0;
            settext(&r, "interrupted by a stop, will be sent again");
            oftp_spl_put(m->spool, &r);
            sends++;
        } else {
            snprintf(tmp, sizeof(tmp), "%s.%.8s", m->cfg.tmpprefix, r.key);
            remove(tmp);
            oftp_spl_del(m->spool, r.key);
            recvs++;
        }
    }
    if (sends || recvs)
        oftp_mon_msg("OFTP040I RECOVERED %d SENDS, DISCARDED %d PARTIAL RECEIVES",
                     sends, recvs);
}

static void
cmd_display(OFTP_MON *m, const char *what)
{
    int i;

    if (what[0] == 'P') {
        for (i = 0; i < m->cfg.nptn; i++) {
            OFTP_PARTNER *p = &m->cfg.ptn[i];

            oftp_mon_msg("OFTP100I %-8s %-25s %s%s LEVEL %d %s:%d", p->peer.name,
                         p->peer.code, p->enabled ? "ENABLED" : "DISABLED",
                         m->busy[i] ? " IN SESSION" : "", p->peer.level,
                         p->host[0] ? p->host : "-", p->port);
        }
        oftp_mon_msg("OFTP101I %d PARTNERS, %d SESSIONS", m->cfg.nptn, m->nsess);
    } else if (what[0] == 'Q') {
        OFTP_SPREC r;
        char last[8];
        int n = 0;

        last[0] = '\0';
        while (oftp_spl_next(m->spool, last, &r) == 0) {
            memcpy(last, r.key, 8);
            oftp_mon_msg("OFTP110I %.8s %c %c %-8s %s", r.key, r.dir,
                         r.state, r.partner, r.dsn);
            n++;
        }
        oftp_mon_msg("OFTP111I %d TRANSFERS IN THE SPOOL", n);
    } else
        oftp_mon_msg("OFTP101I %d SESSIONS", m->nsess);
}

void
oftp_mon_command(OFTP_MON *m, const char *cmd)
{
    char verb[16], arg[40];
    OFTP_PARTNER *p;

    /* split by hand: verb, then the first operand */
    {
        const char *p = cmd;
        size_t n = 0;

        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && *p != ',' && n < sizeof(verb) - 1)
            verb[n++] = *p++;
        verb[n] = '\0';
        while (*p == ' ' || *p == ',')
            p++;
        n = 0;
        while (*p && *p != ' ' && n < sizeof(arg) - 1)
            arg[n++] = *p++;
        arg[n] = '\0';
    }
    if (strcmp(verb, "D") == 0 || strcmp(verb, "DISPLAY") == 0)
        cmd_display(m, arg);
    else if (strcmp(verb, "CALL") == 0) {
        if ((p = oftp_cfg_find(&m->cfg, arg)) == NULL || !p->call)
            oftp_mon_msg("OFTP120E %s IS NOT A PARTNER THAT CAN BE CALLED", arg);
        else {
            m->callreq[ptn_index(m, p)] = 1;
            ecb_post(&m->wake, 0);
            oftp_mon_msg("OFTP121I CALL OF %s REQUESTED", p->peer.name);
        }
    } else
        oftp_mon_msg("OFTP199E UNKNOWN COMMAND: %s", cmd);
}
