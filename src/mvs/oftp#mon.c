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

#include "oftp.h"
#include "oftpdsf.h"
#include "oftpdsn.h"
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

/* An EERP or NERP to pass back to the partner a relayed file came from:
** a receive record in state RECEIVED that holds no file, only the
** response (f_next_erp sends it like the EERP of a received file).
*/
static void
queue_erp(OFTP_MON *m, const OFTP_SPREC *sent, char code, int reas,
          const char *crea)
{
    OFTP_SPREC e;

    memset(&e, 0, sizeof(e));
    e.dir = SPL_RECV;
    e.state = SPL_RECEIVED;
    e.relay = 'Y';
    e.erp = code;
    e.reas = reas;
    strcpy(e.partner, sent->from);
    strcpy(e.dsn, sent->dsn);
    strcpy(e.date, sent->date);
    strcpy(e.time, sent->time);
    strcpy(e.user, sent->user);
    strcpy(e.orig, sent->orig);
    strcpy(e.dest, sent->dest);
    snprintf(e.crea, sizeof(e.crea), "%s", crea);
    e.tqueued = e.tlast = (long)time(NULL);
    snprintf(e.text, sizeof(e.text), "relay: %s to pass back to %s",
             code == OFTP_EERP ? "EERP" : "NERP", sent->from);
    if (oftp_spl_add(m->spool, &e) == 0)
        oftp_mon_msg("OFTP240I %.8s RELAY: %s FOR %s QUEUED FOR %s", e.key,
                     code == OFTP_EERP ? "EERP" : "NERP", e.dsn, e.partner);
    ecb_post(&m->wake, 0);
}

/* A send that failed this time, its reason and text set: it waits for
** the next attempt (oftp_cfg_retry), or it has had its last. Our own
** file is then held, for a person to decide; a relayed one is given up
** and its originator gets a NERP. The caller writes the record.
*/
static void
failed(OFTP_MON *m, OFTP_SPREC *r)
{
    char t[80];

    if (oftp_cfg_retry(&m->cfg, r->attempts, (long)time(NULL), &r->tnext) == 0) {
        r->state = SPL_WAITING;
        return;
    }
    snprintf(t, sizeof(t), "%.48s, given up after %u attempts", r->text,
             r->attempts);
    settext(r, t);
    if (r->relay == 'Y') {
        r->state = SPL_REJECTED;
        oftp_mon_msg("OFTP242W %.8s RELAY: %s FOR %s GIVEN UP AFTER %u ATTEMPTS",
                     r->key, r->dsn, r->dest, r->attempts);
        queue_erp(m, r, OFTP_NERP, r->reasby == 'P' && r->reas ? r->reas : 99,
                  m->cfg.odetteid);
        remove(r->local);
    } else {
        r->state = SPL_HELD;
        oftp_mon_msg("OFTP215W %.8s %s: %s HELD AFTER %u ATTEMPTS", r->key,
                     r->partner, r->dsn, r->attempts);
    }
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
        /* a relayed file is read back raw, as it was stored */
        if (oftp_dsf_open_out(name,
                              r.fmt == 'T' && r.relay != 'Y' ? 'T' : 0,
                              r.xlate == 'Y' && r.relay != 'Y', f,
                              &c->odsf) != 0) {
            r.state = SPL_FAILED;
            r.reasby = 'L';
            r.reas = 12;
            settext(&r, "data set cannot be opened");
            oftp_spl_put(c->m->spool, &r);
            oftp_mon_msg("OFTP210E %.8s %s: %s CANNOT BE OPENED", r.key,
                         r.partner, name);
            continue;
        }
        if (r.relay == 'Y') {
            /* passed through unchanged: a T file was stored as received,
            ** as an undefined-format byte stream */
            if (r.fmt == 'T') {
                f->fmt = 'T';
                f->lrecl = 0;
            }
        } else {
            r.fmt = f->fmt;
            r.lrecl = f->lrecl;
        }
        strcpy(f->dsn, r.dsn);
        strcpy(f->date, r.date);
        strcpy(f->time, r.time);
        strcpy(f->user, r.user);
        /* the final recipient: the partner itself, or the one behind it */
        strcpy(f->dest, r.dest[0] ? r.dest : c->pt->peer.code);
        /* the originator: we, or for a relayed file the one who sent it */
        strcpy(f->orig, r.relay == 'Y' ? r.orig : c->pt->peer.ourcode);
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
        snprintf(t, sizeof(t), "refused by the partner, reason %02d", reas);
        settext(r, t);
        oftp_mon_msg("OFTP212W %.8s %s: %s REFUSED, REASON %02d RETRY %c",
                     r->key, r->partner, r->dsn, reas, retry);
        if (retry == 'Y')
            failed(c->m, r);
        else {
            r->state = SPL_REJECTED;
            /* a relayed file that cannot go on: the originator learns it */
            if (r->relay == 'Y') {
                queue_erp(c->m, r, OFTP_NERP, reas, c->m->cfg.odetteid);
                remove(r->local);
            }
        }
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

    OFTP_PARTNER *next = NULL, *target = NULL;
    OFTP_SFIDCMD alloc;

    *retry = 'N';
    /* for us, by our own ID or the one we have towards this partner; for
    ** anyone else only as a node (RELAY=Y) towards a partner we know */
    if (strcmp(f->dest, c->m->cfg.odetteid) != 0
        && strcmp(f->dest, peer->ourcode) != 0) {
        if (c->m->cfg.relay)
            next = oftp_cfg_route(&c->m->cfg, f->dest, &target);
        if (next == NULL || next == c->pt) {
            oftp_mon_msg("OFTP223W %s: %s FOR %s REFUSED, %s", c->pt->peer.name,
                         f->dsn, f->dest,
                         !c->m->cfg.relay ? "NOT OUR ODETTE ID"
                         : next == NULL ? "NO ROUTE" : "IT WOULD GO BACK");
            return 2;
        }
    }
    if (next != NULL)
        snprintf(final, sizeof(final), "%s", "");
    /* any virtual file name becomes a valid data set name */
    else if (oftp_dsn_map(c->m->cfg.inprefix, f->dsn, "", final) != 0)
        return 1;

    /* the same virtual file again: answer its EERP once more */
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if ((r.dir == SPL_RECV || r.relay == 'Y') && r.local[0]
            && strcmp(r.orig, f->orig) == 0
            && strcmp(r.dsn, f->dsn) == 0 && strcmp(r.date, f->date) == 0
            && strcmp(r.time, f->time) == 0) {
            if (r.dir == SPL_RECV && r.state == SPL_CONFIRMED) {
                r.state = SPL_RECEIVED;
                oftp_spl_put(c->m->spool, &r);
            }
            return 13;
        }
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
    if (next != NULL) {
        /* relayed: where it goes on, and where its EERP goes back */
        r.relay = 'Y';
        r.xlate = 'N';
        strcpy(r.from, c->pt->peer.name);
        strcpy(r.target, target != next ? target->peer.name : "");
        strcpy(r.partner, next->peer.name);
    }
    if (oftp_spl_add(c->m->spool, &r) != 0) {
        *retry = 'Y';
        return 99;
    }
    /* the name is taken: the transfer number makes it unique */
    if (next == NULL && exists(final)) {
        char key[9];

        memcpy(key, r.key, 8);
        key[8] = '\0';
        if (oftp_dsn_map(c->m->cfg.inprefix, f->dsn, key, final) != 0
            || exists(final)) {
            oftp_spl_del(c->m->spool, r.key);
            *retry = 'Y';
            return 13;
        }
        strcpy(r.local, final);
        oftp_spl_put(c->m->spool, &r);
    }
    snprintf(c->tmp, sizeof(c->tmp), "%s.%.8s", c->m->cfg.tmpprefix, r.key);
    memset(&opt, 0, sizeof(opt));
    opt.tlrecl = c->m->cfg.tlrecl;
    opt.ublksize = c->m->cfg.ublksize;
    strcpy(opt.unit, c->m->cfg.unit);
    strcpy(opt.volser, c->m->cfg.volser);
    /* a relayed T file is kept as it came: a byte stream, format U */
    alloc = *f;
    if (next != NULL && f->fmt == 'T')
        alloc.fmt = 'U';
    rc = oftp_dsf_open_in(c->tmp, &alloc, r.xlate == 'Y', &opt, &c->idsf);
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
    if (r->relay == 'Y') {
        /* not ours: it goes on as it is, and only the final recipient
        ** answers with an EERP */
        r->dir = SPL_SEND;
        r->state = SPL_QUEUED;
        strcpy(r->local, c->tmp);
        r->tdone = (long)time(NULL);
        settext(r, "relay: received, queued to go on");
        oftp_spl_put(c->m->spool, r);
        oftp_mon_msg("OFTP241I %.8s RELAY: %s FROM %s FOR %s QUEUED FOR %s",
                     r->key, r->dsn, r->from, r->dest, r->partner);
        r->key[0] = '\0';
        ecb_post(&c->m->wake, 0);
        return 0;
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
        *code = r.erp == OFTP_NERP ? OFTP_NERP : OFTP_EERP;
        e->reas = r.reas;
        strcpy(e->crea, r.crea);
        strcpy(e->dsn, r.dsn);
        strcpy(e->date, r.date);
        strcpy(e->time, r.time);
        strcpy(e->user, r.user);
        strcpy(e->dest, r.orig);
        strcpy(e->orig, r.dest[0] ? r.dest : peer->ourcode);
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
        /* a NERP to a 1.3 partner is dropped by the engine, and done */
        settext(&r, r.erp == OFTP_NERP ? "NERP passed on, or dropped below 1.4"
                                       : "received, EERP sent");
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
            || strcmp(r.time, e->time) != 0
            || (r.dest[0] && strcmp(r.dest, e->orig) != 0))
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
        if (r.relay == 'Y') {
            queue_erp(c->m, &r, code, e->reas, e->crea);
            remove(r.local);
        }
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

    /* a file cut off in the middle: an attempt that failed, or with a
    ** stop none at all; a receive is away */
    if (c->odsf != NULL) {
        oftp_dsf_close(c->odsf, 1);
        c->out.reasby = 'L';
        settext(&c->out, "session ended during the transfer");
        if (m->stop) {
            c->out.state = SPL_WAITING;
            c->out.tnext = 0;
            if (c->out.attempts > 0)
                c->out.attempts--;
        } else
            failed(m, &c->out);
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
    /* a call that ended in an error before any file: not again at once */
    if (c->initiator && rc != 0 && c->s.files_out == 0 && c->s.files_in == 0)
        m->tcall[ptn_index(m, c->pt)] = (long)time(NULL) + m->cfg.retry;
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

/* A partner that was not reached: each send due for it has had an
** attempt, and the partner is not called again before RETRY seconds.
** busy[i] is still set, so no session for it changes these records.
*/
static void
unreached(OFTP_MON *m, int i)
{
    OFTP_PARTNER *p = &m->cfg.ptn[i];
    OFTP_SPREC r;
    char last[8];
    long now = (long)time(NULL);

    m->tcall[i] = now + m->cfg.retry;
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_SEND || strcmp(r.partner, p->peer.name) != 0
            || !(r.state == SPL_QUEUED
                 || (r.state == SPL_WAITING && r.tnext <= now)))
            continue;
        r.attempts++;
        r.tlast = now;
        r.reasby = 'L';
        r.reas = 0;
        settext(&r, "partner not reached");
        failed(m, &r);
        oftp_spl_put(m->spool, &r);
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

            if (!(work[i] || m->callreq[i]) || !p->enabled || !p->call
                || (!m->callreq[i] && (long)time(NULL) < m->tcall[i]))
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
                unreached(m, i);
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
    char partner[16], dsn[60], vfn[40], fmt[4], xl[4], user[12], owner[12];
    char hold[4], reply[100];
    int empty;
    char *lp;
    struct tm *tm;
    time_t now = time(NULL);

    kw(line, "PARTNER", partner, sizeof(partner));
    kw(line, "DSN", dsn, sizeof(dsn));
    kw(line, "VFN", vfn, sizeof(vfn));
    kw(line, "FMT", fmt, sizeof(fmt));
    kw(line, "XLATE", xl, sizeof(xl));
    kw(line, "USER", user, sizeof(user));
    kw(line, "OWNER", owner, sizeof(owner));
    kw(line, "HOLD", hold, sizeof(hold));
    if ((p = oftp_cfg_find(&m->cfg, partner)) == NULL) {
        oftp_net_puts(s, "501 unknown or disabled partner");
        return;
    }
    memset(&r, 0, sizeof(r));
    /* a partner behind another: the session goes to that one */
    if (p->via[0]) {
        OFTP_PARTNER *v = oftp_cfg_find(&m->cfg, p->via);

        if (v == NULL) {
            oftp_net_puts(s, "501 the partner it is reached through is disabled");
            return;
        }
        strcpy(r.target, p->peer.name);
        strcpy(r.dest, p->peer.code);
        p = v;
    }
    if ((lp = strchr(dsn, '(')) != NULL) {
        char *e = strchr(lp, ')');

        *lp = '\0';
        if (e)
            *e = '\0';
        snprintf(r.member, sizeof(r.member), "%s", lp + 1);
    }
    if (!oftp_dsn_valid(dsn) || !exists(dsn)) {
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
    /* an empty file is queued all the same, with a warning */
    {
        RFILE *rf;
        char q[60], *one;
        size_t got = 0;

        if (r.member[0])
            snprintf(q, sizeof(q), "'%s(%s)'", dsn, r.member);
        else
            snprintf(q, sizeof(q), "'%s'", dsn);
        if (ropen(q, 0, &rf) != 0) {
            oftp_net_puts(s, "502 data set cannot be opened");
            return;
        }
        /* one record, on the heap: this thread's stack is 32 KB */
        if ((one = malloc((size_t)rf->lrecl + 4)) == NULL) {
            rclose(rf);
            oftp_net_puts(s, "451 no storage");
            return;
        }
        empty = rread(rf, one, &got) != 0;
        free(one);
        rclose(rf);
    }
    r.dir = SPL_SEND;
    r.state = hold[0] == 'Y' ? SPL_HELD : SPL_QUEUED;
    r.fmt = fmt[0] == 'T' ? 'T' : 0;
    r.xlate = xl[0] == 'Y' ? 'Y' : 'N';
    strcpy(r.partner, p->peer.name);
    snprintf(r.owner, sizeof(r.owner), "%s", owner);
    snprintf(r.user, sizeof(r.user), "%s", user);
    strcpy(r.dsn, vfn);
    strcpy(r.local, dsn);
    if (r.dest[0] == '\0')
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
    settext(&r, r.state == SPL_HELD ? "queued and held" : "queued");
    if (oftp_spl_add(m->spool, &r) != 0) {
        oftp_net_puts(s, "451 spool not available");
        return;
    }
    /* 210: done, with a warning */
    snprintf(reply, sizeof(reply), "%s %.8s %s for %s%s%s as %s%s",
             empty ? "210" : "200", r.key,
             r.state == SPL_HELD ? "queued and held" : "queued",
             r.target[0] ? r.target : r.partner, r.target[0] ? " via " : "",
             r.target[0] ? r.partner : "", r.dsn,
             empty ? " -- the data set is empty" : "");
    oftp_net_puts(s, reply);
    oftp_mon_msg("OFTP230I %.8s QUEUED FOR %s: %s AS %s", r.key, r.partner,
                 dsn, r.dsn);
    ecb_post(&m->wake, 0);
}

static const char *
state_name(char st)
{
    switch (st) {
    case SPL_QUEUED:    return "QUEUED";
    case SPL_ACTIVE:    return "ACTIVE";
    case SPL_WAITING:   return "WAITING";
    case SPL_HELD:      return "HELD";
    case SPL_SENT:      return "SENT";
    case SPL_DELIVERED: return "DELIVERED";
    case SPL_REJECTED:  return "REJECTED";
    case SPL_FAILED:    return "FAILED";
    case SPL_RECEIVED:  return "RECEIVED";
    case SPL_CONFIRMED: return "CONFIRMED";
    default:            return "?";
    }
}

static void
ctl_list(OFTP_MON *m, int s, const char *line)
{
    OFTP_SPREC r;
    char last[8], partner[16], status[16], owner[12], out[200];
    int n = 0;

    kw(line, "PARTNER", partner, sizeof(partner));
    kw(line, "STATUS", status, sizeof(status));
    kw(line, "OWNER", owner, sizeof(owner));
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (partner[0] && strcmp(partner, r.partner) != 0
            && strcmp(partner, r.target) != 0)
            continue;
        if (status[0] && strncmp(status, state_name(r.state), strlen(status)) != 0)
            continue;
        if (owner[0] && strcmp(owner, r.owner) != 0)
            continue;
        snprintf(out, sizeof(out), "100 %.8s %-4s %-9s %-8s %-26s %s",
                 r.key, r.dir == SPL_SEND ? "SEND" : "RECV",
                 state_name(r.state), r.target[0] ? r.target : r.partner,
                 r.dsn, r.local);
        oftp_net_puts(s, out);
        n++;
    }
    snprintf(out, sizeof(out), "200 %d transfers", n);
    oftp_net_puts(s, out);
}

static void
ctl_show(OFTP_MON *m, int s, const char *line)
{
    OFTP_SPREC r;
    char key[12], out[200];

    kw(line, "KEY", key, sizeof(key));
    if (strlen(key) != 8 || oftp_spl_get(m->spool, key, &r) != 0) {
        oftp_net_puts(s, "504 no such transfer");
        return;
    }
    snprintf(out, sizeof(out), "100 %.8s %s %s, partner %s%s%s, owner %s",
             r.key, r.dir == SPL_SEND ? "SEND" : "RECEIVE",
             state_name(r.state), r.target[0] ? r.target : r.partner,
             r.target[0] ? " via " : "", r.target[0] ? r.partner : "",
             r.owner[0] ? r.owner : "-");
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 virtual file %s %s %s, format %c, LRECL %u",
             r.dsn, r.date, r.time, r.fmt ? r.fmt : '-', r.lrecl);
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 data set %s%s%s%s", r.local,
             r.member[0] ? "(" : "", r.member, r.member[0] ? ")" : "");
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 from %s to %s, %lu records, %u attempts",
             r.orig, r.dest, r.recs, r.attempts);
    oftp_net_puts(s, out);
    if (r.reas)
        snprintf(out, sizeof(out), "100 reason %02d (%s): %s", r.reas,
                 r.reasby == 'P' ? "partner" : "local", r.text);
    else
        snprintf(out, sizeof(out), "100 %s", r.text);
    oftp_net_puts(s, out);
    oftp_net_puts(s, "200 end");
}

/* DELETE, HOLD, RELEASE, RETRY of one transfer */
static void
ctl_change(OFTP_MON *m, int s, const char *verb, const char *line)
{
    OFTP_SPREC r;
    char key[12], out[80];

    kw(line, "KEY", key, sizeof(key));
    if (strlen(key) != 8 || oftp_spl_get(m->spool, key, &r) != 0) {
        oftp_net_puts(s, "504 no such transfer");
        return;
    }
    if (r.state == SPL_ACTIVE) {
        oftp_net_puts(s, "450 transfer is active, try later");
        return;
    }
    if (strcmp(verb, "DELETE") == 0) {
        if (oftp_spl_del(m->spool, r.key) != 0) {
            oftp_net_puts(s, "451 spool not available");
            return;
        }
        snprintf(out, sizeof(out), "200 %.8s deleted", r.key);
        oftp_net_puts(s, out);
        oftp_mon_msg("OFTP231I %.8s DELETED", r.key);
        return;
    }
    if (r.dir != SPL_SEND) {
        oftp_net_puts(s, "505 only a send can be held, released or retried");
        return;
    }
    if (strcmp(verb, "HOLD") == 0) {
        if (r.state != SPL_QUEUED && r.state != SPL_WAITING) {
            oftp_net_puts(s, "505 only a queued or waiting send can be held");
            return;
        }
        r.state = SPL_HELD;
        settext(&r, "held");
    } else if (strcmp(verb, "RELEASE") == 0) {
        if (r.state != SPL_HELD) {
            oftp_net_puts(s, "505 not held");
            return;
        }
        r.state = SPL_QUEUED;
        r.attempts = 0;
        settext(&r, "released");
    } else {                            /* RETRY */
        if (r.state == SPL_QUEUED) {
            oftp_net_puts(s, "505 already queued");
            return;
        }
        /* a SENT file goes out once more: the partner sees it twice */
        r.state = SPL_QUEUED;
        r.attempts = 0;
        r.reas = 0;
        r.erp = 0;
        settext(&r, "queued again");
    }
    if (oftp_spl_put(m->spool, &r) != 0) {
        oftp_net_puts(s, "451 spool not available");
        return;
    }
    snprintf(out, sizeof(out), "200 %.8s %s", r.key, state_name(r.state));
    oftp_net_puts(s, out);
    oftp_mon_msg("OFTP232I %.8s %s BY CONTROL PORT", r.key, verb);
    ecb_post(&m->wake, 0);
}

static void
ctl_partners(OFTP_MON *m, int s)
{
    char out[160];
    int i;

    for (i = 0; i < m->cfg.nptn; i++) {
        OFTP_PARTNER *p = &m->cfg.ptn[i];

        snprintf(out, sizeof(out), "100 %-8s %-25s %-8s %s LEVEL %d %s%s",
                 p->peer.name, p->peer.code,
                 p->enabled ? "ENABLED" : "DISABLED",
                 m->busy[i] ? "BUSY" : "IDLE", p->peer.level,
                 p->via[0] ? "via " : p->call ? p->host : "(not called)",
                 p->via[0] ? p->via : "");
        oftp_net_puts(s, out);
    }
    snprintf(out, sizeof(out), "200 %d partners", m->cfg.nptn);
    oftp_net_puts(s, out);
}

static void
ctl_status(OFTP_MON *m, int s)
{
    OFTP_SPREC r;
    char last[8], out[160];
    int cnt[128], i, on = 0, total = 0;

    memset(cnt, 0, sizeof(cnt));
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        cnt[(unsigned char)r.state & 0x7F]++;
        total++;
    }
    for (i = 0; i < m->cfg.nptn; i++)
        if (m->cfg.ptn[i].enabled)
            on++;
    snprintf(out, sizeof(out), "100 Odette ID %s, OFTP port %d, control port %d",
             m->cfg.odetteid, m->cfg.port, m->cfg.ctlport);
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 sessions %d of %d, partners %d of %d enabled",
             m->nsess, m->cfg.maxsess, on, m->cfg.nptn);
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 spool %d: queued %d, waiting %d, held %d, "
             "active %d, sent %d", total, cnt[SPL_QUEUED & 0x7F],
             cnt[SPL_WAITING & 0x7F], cnt[SPL_HELD & 0x7F],
             cnt[SPL_ACTIVE & 0x7F], cnt[SPL_SENT & 0x7F]);
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 delivered %d, rejected %d, failed %d, "
             "received %d, confirmed %d", cnt[SPL_DELIVERED & 0x7F],
             cnt[SPL_REJECTED & 0x7F], cnt[SPL_FAILED & 0x7F],
             cnt[SPL_RECEIVED & 0x7F], cnt[SPL_CONFIRMED & 0x7F]);
    oftp_net_puts(s, out);
    snprintf(out, sizeof(out), "100 received files go to %s, %s",
             m->cfg.inprefix, oftp_version());
    oftp_net_puts(s, out);
    oftp_net_puts(s, "200 status");
}

static void
ctl_call(OFTP_MON *m, int s, const char *line)
{
    OFTP_PARTNER *p;
    char partner[16], out[80];

    kw(line, "PARTNER", partner, sizeof(partner));
    if ((p = oftp_cfg_find(&m->cfg, partner)) == NULL || !p->call) {
        oftp_net_puts(s, "501 not a partner that can be called");
        return;
    }
    m->callreq[ptn_index(m, p)] = 1;
    ecb_post(&m->wake, 0);
    snprintf(out, sizeof(out), "200 call of %s requested", p->peer.name);
    oftp_net_puts(s, out);
}

static int
ctl_thread(void *arg1, void *arg2)
{
    OFTP_MON *m = arg1;
    char ip[16], line[256];

    (void)arg2;
    while (!m->stop) {
        int s, n;

        /* a dialog waits for every answer: look for the next request
        ** every 0.1 s, and at once after one
        */
        if ((s = oftp_net_accept(m->csock, ip, sizeof(ip))) < 0) {
            ECB e = 0;

            ecb_timed_wait(&e, 10, 0);
            continue;
        }
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
            else if (strncmp(line, "SHOW", 4) == 0)
                ctl_show(m, s, line);
            else if (strncmp(line, "DELETE", 6) == 0)
                ctl_change(m, s, "DELETE", line);
            else if (strncmp(line, "HOLD", 4) == 0)
                ctl_change(m, s, "HOLD", line);
            else if (strncmp(line, "RELEASE", 7) == 0)
                ctl_change(m, s, "RELEASE", line);
            else if (strncmp(line, "RETRY", 5) == 0)
                ctl_change(m, s, "RETRY", line);
            else if (strncmp(line, "CALL", 4) == 0)
                ctl_call(m, s, line);
            else if (strncmp(line, "PARTNERS", 8) == 0)
                ctl_partners(m, s);
            else if (strncmp(line, "STATUS", 6) == 0)
                ctl_status(m, s);
            else if (strncmp(line, "REFRESH", 7) == 0) {
                char out[80];
                int on = oftp_mon_refresh(m);

                if (on < 0)
                    oftp_net_puts(s, "451 partner member cannot be read");
                else {
                    snprintf(out, sizeof(out), "200 partners reloaded, %d enabled", on);
                    oftp_net_puts(s, out);
                }
            }
            else
                oftp_net_puts(s, "500 unknown request");
        }
        oftp_net_close(s);
    }
    return 0;
}

/* ---- configuration ----------------------------------------------- */

static void
cfgmsg(void *ctx, const char *line)
{
    oftp_mon_msg("OFTP010W %s: %s", (const char *)ctx, line);
}

int
oftp_mon_load(OFTP_CONFIG *c, const char *dd, int partner)
{
    char name[16], line[256];
    FILE *fp;
    int n = 0;

    snprintf(name, sizeof(name), "DD:%s", dd);
    if ((fp = fopen(name, "r")) == NULL)
        return -1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* fixed records: columns 73-80 are sequence numbers */
        if (strlen(line) > 72)
            line[72] = '\0';
        oftp_cfg_line(c, partner, line, ++n, cfgmsg, (void *)dd);
    }
    fclose(fp);
    return 0;
}

int
oftp_mon_refresh(OFTP_MON *m)
{
    OFTP_CONFIG *n;
    int i, j, on = 0, added = 0, dropped = 0;

    if ((n = malloc(sizeof(*n))) == NULL)
        return -1;
    *n = m->cfg;                        /* the monitor settings as they are */
    n->nptn = 0;
    n->errors = 0;
    if (oftp_mon_load(n, "OFTPPTN", 1) != 0) {
        free(n);
        return -1;
    }
    oftp_cfg_finish(n, cfgmsg, (void *)"OFTPPTN");

    lock(m, 0);
    for (i = 0; i < m->cfg.nptn; i++) {
        OFTP_PARTNER *o = &m->cfg.ptn[i];

        for (j = 0; j < n->nptn; j++)
            if (strcmp(n->ptn[j].peer.name, o->peer.name) == 0)
                break;
        if (j == n->nptn) {
            if (o->enabled)
                dropped++;
            o->enabled = 0;
        } else {
            *o = n->ptn[j];
            m->tcall[i] = 0;                    /* perhaps a new HOST */
            n->ptn[j].peer.name[0] = '\0';     /* taken */
        }
    }
    for (j = 0; j < n->nptn; j++) {
        if (n->ptn[j].peer.name[0] == '\0')
            continue;
        if (m->cfg.nptn >= OFTP_MAXPTN) {
            oftp_mon_msg("OFTP015W PARTNER %s NOT ADDED, TABLE FULL",
                         n->ptn[j].peer.name);
            continue;
        }
        m->cfg.ptn[m->cfg.nptn++] = n->ptn[j];
        added++;
    }
    for (i = 0; i < m->cfg.nptn; i++)
        if (m->cfg.ptn[i].enabled)
            on++;
    unlock(m, 0);
    free(n);
    oftp_mon_msg("OFTP016I PARTNERS RELOADED: %d ENABLED, %d NEW, %d DISABLED",
                 on, added, dropped);
    return on;
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
    } else if (strcmp(verb, "REFRESH") == 0) {
        if (oftp_mon_refresh(m) < 0)
            oftp_mon_msg("OFTP017E DD OFTPPTN CANNOT BE READ");
    } else
        oftp_mon_msg("OFTP199E UNKNOWN COMMAND: %s", cmd);
}
