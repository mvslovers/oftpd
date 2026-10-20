/* oftp#mon.c - the monitor: sessions, listener, scheduler, the requests
** of the utilities and console commands (MVS only)
**
** Every session runs in a thread of its own. Its file callbacks go to
** the spool (one record per transfer) and to the data set layer; the
** session engine does the protocol. No writable static data: OFTPD is
** AC(1) and reentrant, everything lives in the OFTP_MON block.
*/
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ext/array.h>
#include <mvs/clock.h>
#include <mvs/crt.h>
#include <mvs/lock.h>
#include <mvs/recovery.h>
#include <mvs/rfile.h>
#include <mvs/storage.h>
#include <mvs/wto.h>

#include "oftp.h"
#include "oftpdsf.h"
#include "oftpdsn.h"
#include "oftplog.h"
#include "oftpmon.h"
#include "oftpsch.h"
#include "oftpsec.h"
#include "oftpnet.h"
#include "oftpsess.h"

/* ---- messages ---------------------------------------------------- */

/* The log is a RFILE on DD OFTPLOG, in the OFTP_MON block; OFTPD holds no
** writable static data, so the block is found through the GRT slot that
** oftp_mon_logopen() sets (the GRT is the process's and every subtask sees
** the same one). Before it is set, and after oftp_mon_logclose(), a message
** goes to the console alone.
*/
static int
log_put(void *file, const char *rec, unsigned len)
{
    errno = 0;
    if (rwrite((RFILE *)file, rec, len) == 0)
        return 0;
    return errno != 0 ? errno : -1;
}

/* wait: lock the log; else only if it is free. A call that cannot lock it
** (the ESTAE, which may have interrupted a write in this very task) puts
** the line on the console, whatever its class: lost is worse than loud.
*/
static void
msgv(const char *fmt, va_list ap, int wait)
{
    char line[OFTP_MSG_MAX + 1];
    CLIBGRT *grt;
    OFTP_LOG *lg;
    OFTP_LOG_IO io;

    oftp_log_vformat(line, sizeof(line), fmt, ap);
    grt = __grtget();
    lg = grt != NULL ? (OFTP_LOG *)grt->grtapp2 : NULL;
    if (lg == NULL || (wait ? lock(lg, 0) : trylock(lg, 0)) != 0) {
        wto(line);
        return;
    }
    io.put = log_put;
    io.console = wto;
    oftp_log_route(lg, &io, (long)time(NULL), line);
    unlock(lg, 0);
}

void
oftp_mon_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    msgv(fmt, ap, 1);
    va_end(ap);
}

void
oftp_mon_msg_nowait(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    msgv(fmt, ap, 0);
    va_end(ap);
}

int
oftp_mon_logopen(OFTP_MON *m)
{
    CLIBGRT *grt = __grtget();
    RFILE *rf = NULL;

    if (grt != NULL)
        grt->grtapp2 = &m->log;
    if (ropen("dd:OFTPLOG", 1, &rf) != 0) {
        oftp_mon_msg("OFTP022W DD OFTPLOG CANNOT BE OPENED, MESSAGES GO TO"
                     " THE CONSOLE");
        return -1;
    }
    if (rf->recfm != RFILE_RECFM_F) {
        rclose(rf);
        oftp_mon_msg("OFTP022W DD OFTPLOG IS NOT RECFM=F OR FB, MESSAGES GO"
                     " TO THE CONSOLE");
        return -1;
    }
    m->log.file = rf;
    m->log.reclen = (unsigned)rf->lrecl;
    return 0;
}

void
oftp_mon_logclose(OFTP_MON *m)
{
    CLIBGRT *grt = __grtget();
    RFILE *rf;
    int rc = 0;

    lock(&m->log, 0);
    rf = (RFILE *)m->log.file;
    m->log.file = NULL;
    if (rf != NULL) {
        errno = 0;
        if (rclose(rf) != 0)
            rc = errno != 0 ? errno : -1;
    }
    if (grt != NULL && grt->grtapp2 == &m->log)
        grt->grtapp2 = NULL;
    unlock(&m->log, 0);
    if (rc != 0)
        oftp_mon_msg("OFTP023E LOG CLOSE FAILED, ERRNO %d", rc);
}

/* ---- helpers ----------------------------------------------------- */

static int
ptn_index(const OFTP_MON *m, const OFTP_PARTNER *p)
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

/* A send a session may take now */
static int
send_due(const OFTP_SPREC *r, long now)
{
    if (r->dir != SPL_SEND
        || !(r->state == SPL_QUEUED
             || (r->state == SPL_WAITING && r->tnext <= now)))
        return 0;
    /* STRTDATE/STRTTIME and SENDWIN (oftpsch.h): a retry waits for them
    ** as well as the first attempt */
    if (r->tstart > now)
        return 0;
    if (r->winfrom || r->winto) {
        time_t tt = (time_t)now;
        struct tm tm;

        localtime_r(&tt, &tm);
        return oftp_sch_inwin(r->winfrom, r->winto,
                              (unsigned)(tm.tm_hour * 60 + tm.tm_min));
    }
    return 1;
}

/* Whether a session is sending the EERP or NERP of key: under lock(m)
** where the answer decides a claim */
static int
erp_held(OFTP_MON *m, const char *key)
{
    int i;

    for (i = 0; i < OFTP_MAXSESS; i++)
        if (memcmp(m->erpheld[i], key, 8) == 0)
            return 1;
    return 0;
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
        oftp_mon_msg("OFTP240P %s: %.8s RELAY: %s FOR %s QUEUED", e.partner,
                     e.key, code == OFTP_EERP ? "EERP" : "NERP", e.dsn);
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
        oftp_mon_msg("OFTP242W %s: %.8s RELAY: %s FOR %s GIVEN UP AFTER %u"
                     " ATTEMPTS", r->partner, r->key, r->dsn, r->dest,
                     r->attempts);
        queue_erp(m, r, OFTP_NERP, r->reasby == 'P' && r->reas ? r->reas : 99,
                  m->cfg.odetteid);
        remove(r->local);
    } else {
        r->state = SPL_HELD;
        oftp_mon_msg("OFTP215W %s: %.8s %s HELD AFTER %u ATTEMPTS", r->partner,
                     r->key, r->dsn, r->attempts);
    }
}

/* ---- one session ------------------------------------------------- */

typedef struct sesctx {
    OFTP_MON   *m;
    OFTP_PARTNER *pt;
    const OFTP_PARTNER *tp;             /* the caller's entry, even when
                                        ** disabled: for TRACE only    */
    char        ip[16];
    int         sock;
    int         slot;                   /* in m->ssock, or -1            */
    int         initiator;
    int         starting;               /* our call, counted in
                                        ** m->calling until it first
                                        ** looks for a file to send     */
    OFTP_SPREC  out;                    /* the file being sent, claimed
                                        ** while out.key is set         */
    OFTP_DSF   *odsf;
    OFTP_SPREC  in;                     /* the file being received       */
    OFTP_DSF   *idsf;
    char        tmp[45];
    char        erpkey[8];
    int         rc;                     /* oftp_sess_run(): try() keeps
                                        ** only the abend code          */
    OFTP_SESS   s;
} SESCTX;

/* Our call is past its start: the scheduler counts its work as taken
** from now on (sched_thread) */
static void
started(SESCTX *c)
{
    if (!c->starting)
        return;
    lock(c->m, 0);
    c->m->calling[ptn_index(c->m, c->pt)]--;
    c->starting = 0;
    unlock(c->m, 0);
}

/* Takes the send of key for this session. Under the monitor lock it is
** read again and, if it is still due, written back ACTIVE: of two
** sessions to one partner only one gets it (#18). The data set is opened
** afterwards, outside the lock. 1 when it is ours, in *r.
*/
static int
claim_out(SESCTX *c, const char *key, OFTP_SPREC *r, long now)
{
    int ok;

    lock(c->m, 0);
    ok = oftp_spl_get(c->m->spool, key, r) == 0 && send_due(r, now);
    if (ok) {
        r->state = SPL_ACTIVE;
        r->attempts++;
        r->tlast = now;
        ok = oftp_spl_put(c->m->spool, r) == 0;
    }
    unlock(c->m, 0);
    return ok;
}

static int
f_next_out(void *ctx, const OFTP_PEER *peer, OFTP_SFIDCMD *f, OFTP_FH **fh)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    char last[8], name[56];
    long now = (long)time(NULL);

    (void)peer;
    started(c);
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_SEND || strcmp(r.partner, c->pt->peer.name) != 0
            || !send_due(&r, now) || !claim_out(c, last, &r, now))
            continue;
        c->out = r;                     /* ours: end_files gives it back */
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
            c->out.key[0] = '\0';
            oftp_mon_msg("OFTP210E %s: %.8s %s CANNOT BE OPENED", r.partner,
                         r.key, name);
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
        oftp_spl_put(c->m->spool, &r);  /* the format the open found */
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
        oftp_mon_msg("OFTP211P %s: %.8s %s SENT, %lu RECORDS", r->partner,
                     r->key, r->dsn, r->recs);
    } else {
        r->reas = reas;
        r->reasby = 'P';
        snprintf(t, sizeof(t), "refused by the partner, reason %02d", reas);
        settext(r, t);
        oftp_mon_msg("OFTP212P %s: %.8s %s REFUSED, REASON %02d RETRY %c",
                     r->partner, r->key, r->dsn, reas, retry);
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
#ifdef OFTP_DEBUG_ABEND
    /* Test builds only (-DOFTP_DEBUG_ABEND), never shipped: a file
    ** received under this virtual name ends its session in an S0C4 with
    ** the data set open and the spool record ACTIVE, to measure the
    ** recovery in session_thread() on MVS.
    */
    if (strcmp(c->in.dsn, "OFTPD.TEST.ABEND") == 0) {
        volatile int *trap = (volatile int *)0;

        *trap = 1;
    }
#endif
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
        oftp_mon_msg("OFTP241P %s: %.8s RELAY: %s FROM %s FOR %s QUEUED FOR %s",
                     c->pt->peer.name, r->key, r->dsn, r->from, r->dest,
                     r->partner);
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
    oftp_mon_msg("OFTP222P %s: %.8s %s RECEIVED AS %s, %lu RECORDS",
                 r->partner, r->key, r->dsn, r->local, r->recs);
    r->key[0] = '\0';
    return 0;
}

/* Takes the EERP or NERP of key for this session: the session's entry
** in m->erpheld, set under the lock once the record has been read again,
** since another session may have sent it after the scan read it. Given
** back by f_erp_sent, or with the session whichever way it ends.
*/
static int
claim_erp(SESCTX *c, const char *key, OFTP_SPREC *r)
{
    OFTP_MON *m = c->m;
    int ok;

    lock(m, 0);
    ok = oftp_spl_get(m->spool, key, r) == 0 && r->dir == SPL_RECV
         && r->state == SPL_RECEIVED && !erp_held(m, key);
    if (ok)
        memcpy(m->erpheld[c->slot], key, 8);
    unlock(m, 0);
    return ok;
}

/* The next EERP or NERP owed to the partner that no other session is
** sending. A session without a slot in m->ssock (more than MAXSESS at
** once) has no place for a claim and sends none.
*/
static int
f_next_erp(void *ctx, const OFTP_PEER *peer, char *code, OFTP_ERPCMD *e,
           OFTP_FH **h)
{
    SESCTX *c = ctx;
    OFTP_SPREC r;
    char last[8];

    if (c->slot < 0)
        return 0;
    last[0] = '\0';
    while (oftp_spl_next(c->m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_RECV || r.state != SPL_RECEIVED
            || strcmp(r.partner, c->pt->peer.name) != 0
            || !claim_erp(c, last, &r))
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
    /* only now: given back before CONFIRMED was written, another
    ** session could still find the record RECEIVED */
    lock(c->m, 0);
    memset(c->m->erpheld[c->slot], 0, 8);
    unlock(c->m, 0);
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
            /* DISP=DELETE: the partner has confirmed, now and never
            ** earlier; checked for ALTER when it was queued */
            if (r.disp == 'D' && r.relay != 'Y') {
                if (remove(r.local) == 0) {
                    settext(&r, "delivered, EERP received, data set deleted");
                    oftp_mon_msg("OFTP215P %s: %.8s %s DELETED (DISP=DELETE)",
                                 r.partner, r.key, r.local);
                } else {
                    settext(&r, "delivered, EERP received, data set NOT deleted");
                    oftp_mon_msg("OFTP216W %s: %.8s %s NOT DELETED (DISP=DELETE)",
                                 r.partner, r.key, r.local);
                }
            }
        } else {
            r.state = SPL_REJECTED;
            r.reas = e->reas;
            r.reasby = 'P';
            snprintf(t, sizeof(t), "NERP received, reason %02d", e->reas);
            settext(&r, t);
        }
        oftp_spl_put(c->m->spool, &r);
        oftp_mon_msg("OFTP213P %s: %.8s %s %s", r.partner, r.key, r.dsn,
                     code == OFTP_EERP ? "DELIVERED" : "NOT DELIVERED (NERP)");
        if (r.relay == 'Y') {
            queue_erp(c->m, &r, code, e->reas, e->crea);
            remove(r.local);
        }
        return;
    }
    oftp_mon_msg("OFTP214P %s: %s FOR UNKNOWN FILE %s", c->pt->peer.name,
                 code == OFTP_EERP ? "EERP" : "NERP", e->dsn);
}

static int
p_lookup(void *ctx, const char *code, OFTP_PEER *peer)
{
    SESCTX *c = ctx;
    OFTP_PARTNER *p = oftp_cfg_byid(&c->m->cfg, code);
    int i, n;

    for (i = 0; c->tp == NULL && i < c->m->cfg.nptn; i++)
        if (strcmp(c->m->cfg.ptn[i].peer.code, code) == 0)
            c->tp = &c->m->cfg.ptn[i];
    if (p == NULL)
        return -1;
    if (p->address[0] && strcmp(p->address, c->ip) != 0) {
        oftp_mon_msg("OFTP201W %s CALLED FROM %s, NOT %s", p->peer.name,
                     c->ip, p->address);
        return -1;
    }
    i = ptn_index(c->m, p);
    lock(c->m, 0);
    if ((n = c->m->busy[i]) >= p->sessions) {
        unlock(c->m, 0);
        oftp_mon_msg("OFTP202W %s REFUSED, %d SESSIONS RUNNING, SESSIONS=%d",
                     p->peer.name, n, p->sessions);
        return 8;                       /* ESID 08: retry later (#18)   */
    }
    c->m->busy[i]++;
    unlock(c->m, 0);
    c->pt = p;
    *peer = p->peer;
    return 0;
}

static void
s_log(void *ctx, int warn, const char *line)
{
    SESCTX *c = ctx;

    oftp_mon_msg(warn ? "OFTP290W %s: %s" : "OFTP290P %s: %s",
                 c->pt ? c->pt->peer.name : c->ip, line);
}

static int
s_trace(void *ctx)
{
    SESCTX *c = ctx;
    OFTP_MON *m = c->m;

    const OFTP_PARTNER *p = c->pt != NULL ? c->pt : c->tp;

    return m->trace_all || (p != NULL && m->trace[ptn_index(m, p)]);
}

/* The session itself, for try(): which keeps only the abend code */
static int
run_session(SESCTX *c)
{
    c->rc = oftp_sess_run(&c->s);
    return 0;
}

/* The files a session leaves open, under try(): after an abend the close
** may fail again on what the abend left behind. Each handle is taken out
** of the context before it is closed, so nothing is closed twice.
*/
static int
end_files(SESCTX *c)
{
    OFTP_MON *m = c->m;
    OFTP_DSF *f;

    /* a file cut off in the middle, or claimed and not yet open: an
    ** attempt that failed, or with a stop none at all; a receive is away */
    if ((f = c->odsf) != NULL || c->out.key[0] != '\0') {
        c->odsf = NULL;
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
        c->out.key[0] = '\0';
        if (f != NULL)
            oftp_dsf_close(f, 1);
    }
    if ((f = c->idsf) != NULL) {
        c->idsf = NULL;
        oftp_spl_del(m->spool, c->in.key);
        oftp_dsf_close(f, 0);
    }
    return 0;
}

/* An abend code from try() as the console shows it */
static void
abend_text(unsigned ab, char *out, size_t cap)
{
    if (ab > 0xFFF)
        snprintf(out, cap, "S%03X", (ab >> 12) & 0xFFF);
    else
        snprintf(out, cap, "U%04u", ab);
}

/* 5 to 60 seconds, from the low bits of the TOD clock: two monitors
** whose calls collided at the same second wait different times. It
** keeps no state; rand() keeps a seed, and where libc370 keeps it for a
** reentrant module was not checked.
*/
static long
busy_pause(void)
{
    unsigned char tod[8];

    __getclk(tod);
    return 5 + (long)(((unsigned)tod[4] << 8 | tod[5]) % 56);
}

/* A session in a subtask of its own. cthread runs it without recovery,
** so an abend would end the subtask with the session still counted, its
** partner still busy and its socket still in m->ssock -- where a later
** abend of OFTPD would close the number for whoever has it then. So the
** session runs under try(), and whatever way it ends, the same cleanup
** follows.
*/
static int
session_thread(void *arg1, void *arg2)
{
    SESCTX *c = arg1;
    OFTP_NETX nx;
    OFTP_MON *m = c->m;
    int ab;
    char abt[8];

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
    c->s.traced = s_trace;
    c->s.initiator = c->initiator;
    c->s.timeout = m->cfg.timeout;
    strcpy(c->s.localcode, m->cfg.odetteid);
    if (c->initiator)
        c->s.peer = c->pt->peer;

    c->rc = -1;
    if ((ab = try(run_session, c)) < 0) {
        /* no ESTAE: run it as before, unprotected */
        oftp_mon_msg("OFTP208W %s: SESSION WITHOUT ABEND RECOVERY, RC %d",
                     c->pt ? c->pt->peer.name : c->ip, ab);
        ab = 0;
        c->rc = oftp_sess_run(&c->s);
    }
    if (ab != 0) {
        /* The abend may have come inside a lock(m) window, or inside the
        ** spool's while queueing a request. The task lives on after try(),
        ** so MVS does not release them: we do. unlock() is a DEQ RET=HAVE,
        ** so on a lock this task does not hold it only returns a code.
        */
        unlock(m, 0);
        oftp_spl_release(m->spool);
        /* and the record buffer of a file the abend cut off */
        oftp_sess_free(&c->s);
        abend_text((unsigned)ab, abt, sizeof(abt));
        oftp_mon_msg("OFTP209E %s: SESSION ABEND %s, CLEANING UP",
                     c->pt ? c->pt->peer.name : c->ip, abt);
    }

    if (c->odsf != NULL || c->idsf != NULL || c->out.key[0] != '\0') {
        int fab = try(end_files, c);

        if (fab < 0)
            end_files(c);
        else if (fab > 0) {
            abend_text((unsigned)fab, abt, sizeof(abt));
            oftp_mon_msg("OFTP216E %s: ABEND %s CLOSING THE FILES OF THE SESSION",
                         c->pt ? c->pt->peer.name : c->ip, abt);
        }
    }
    /* the EERP claim goes with the slot, and before it: a session given
    ** the slot next must not find it taken, nor lose its own */
    if (c->slot >= 0) {
        lock(m, 0);
        memset(m->erpheld[c->slot], 0, 8);
        m->ssock[c->slot] = 0;
        unlock(m, 0);
    }
    oftp_net_close(c->sock);
    if (ab != 0)
        oftp_mon_msg("OFTP203P %s: SESSION ENDED BY ABEND %s, FILES %u OUT %u IN",
                     c->pt ? c->pt->peer.name : c->ip, abt,
                     c->s.files_out, c->s.files_in);
    else
        oftp_mon_msg("OFTP203P %s: SESSION ENDED, ESID %02d %s, FILES %u OUT %u IN",
                     c->pt ? c->pt->peer.name : c->ip, c->rc,
                     c->s.esid_local ? "(OURS)" : "(PARTNER)",
                     c->s.files_out, c->s.files_in);
    /* a call that ended in an error before any file, or in an abend:
    ** not again at once. A partner that answered ESID 08 is in a
    ** session already, most likely one it is calling us for at this
    ** moment: called again after a short pause of its own, so that two
    ** monitors calling each other drift apart (#18) */
    if (c->initiator && ab == 0 && c->rc == 8 && !c->s.esid_local
        && c->s.files_out == 0 && c->s.files_in == 0) {
        long w = busy_pause();

        oftp_mon_msg("OFTP218P %s BUSY, CALLED AGAIN IN %ld S",
                     c->pt->peer.name, w);
        m->tcall[ptn_index(m, c->pt)] = (long)time(NULL) + w;
    } else if (c->initiator && (ab != 0 || (c->rc != 0 && c->s.files_out == 0
                                            && c->s.files_in == 0)))
        m->tcall[ptn_index(m, c->pt)] = (long)time(NULL) + m->cfg.retry;
    lock(m, 0);
    if (c->pt) {
        m->busy[ptn_index(m, c->pt)]--;
        if (c->starting)
            m->calling[ptn_index(m, c->pt)]--;
    }
    m->nsess--;
    unlock(m, 0);
    free(c);
    return 0;
}

/* Session subtasks are kept until they have ended, then DETACHed and
** freed with their stack: an ended subtask's TCB and its 64 KB stack stay
** until then (#3, measured about 69 KB per session). Only the task that
** attached a subtask may DETACH it, so each attaching thread keeps its own
** array and reaps it itself. The ATTACH ECB tells which have ended; nothing
** waits on it, so its posted bit stays set. cthread_delete() would refuse
** a live one too, but with a console message.
*/
#define NOWN    (OFTP_MAXSESS * 2)

static int
ended(const CTHDTASK *t)
{
    return (t->termecb & ECB_POSTED_BIT) != 0;
}

/* Frees the ended ones; returns how many are still running */
static int
reap(CTHDTASK **own)
{
    int i, live = 0;

    for (i = 0; i < NOWN; i++) {
        if (own[i] == NULL)
            continue;
        if (ended(own[i]))
            cthread_delete(&own[i]);
        else
            live++;
    }
    return live;
}

/* When the attaching thread stops: a task that ends with a subtask still
** running takes it down with it (S0A03), so wait for the sessions, which
** end on m->stop, then free them. Bounded: what is left after a minute
** stays, and OFTPD's own end takes it.
*/
static void
reap_all(CTHDTASK **own, const char *who)
{
    int i, live;

    for (i = 0; (live = reap(own)) > 0 && i < 600; i++) {
        ECB e = 0;

        if (i % 100 == 0)
            oftp_mon_msg("OFTP095I %s: WAITING FOR %d SESSIONS", who, live);
        ecb_timed_wait(&e, 10, 0);
    }
    if (live > 0)
        oftp_mon_msg("OFTP096W %s: %d SESSION SUBTASKS STILL RUNNING", who,
                     live);
}

static int
start_session(OFTP_MON *m, CTHDTASK **own, int sock, const char *ip,
              OFTP_PARTNER *pt)
{
    SESCTX *c;
    CTHDTASK *t;
    int i, k;

    /* a place for the handle first: without one the subtask could never
    ** be freed */
    reap(own);
    for (k = 0; k < NOWN && own[k] != NULL; k++)
        ;
    if (k == NOWN) {
        oftp_mon_msg("OFTP217W %s: NO ROOM FOR ANOTHER SESSION SUBTASK",
                     pt ? pt->peer.name : ip);
        if (pt) {
            lock(m, 0);
            m->busy[ptn_index(m, pt)]--;
            m->calling[ptn_index(m, pt)]--;
            unlock(m, 0);
        }
        oftp_net_close(sock);
        return -1;
    }
    if ((c = calloc(1, sizeof(*c))) == NULL) {
        oftp_net_close(sock);
        return -1;
    }
    c->m = m;
    c->sock = sock;
    c->pt = pt;
    c->initiator = pt != NULL;
    c->starting = pt != NULL;
    snprintf(c->ip, sizeof(c->ip), "%s", ip);
    c->slot = -1;
    lock(m, 0);
    m->nsess++;
    for (i = 0; i < OFTP_MAXSESS; i++)
        if (m->ssock[i] == 0) {
            m->ssock[i] = sock;
            c->slot = i;
            break;
        }
    unlock(m, 0);
    t = cthread_create_ex(session_thread, c, NULL, 64 * 1024);
    if (t == NULL) {
        lock(m, 0);
        m->nsess--;
        if (pt) {
            m->busy[ptn_index(m, pt)]--;
            m->calling[ptn_index(m, pt)]--;
        }
        if (c->slot >= 0)
            m->ssock[c->slot] = 0;
        unlock(m, 0);
        oftp_net_close(sock);
        free(c);
        return -1;
    }
    own[k] = t;
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
        reap(m->lsess);
        if ((s = oftp_net_accept(m->lsock, ip, sizeof(ip))) < 0)
            continue;
        if (m->nsess >= m->cfg.maxsess) {
            oftp_mon_msg("OFTP204W %s: CALL REFUSED, %d SESSIONS", ip,
                         m->nsess);
            oftp_net_close(s);
            continue;
        }
        oftp_mon_msg("OFTP205P %s: CALL RECEIVED", ip);
        start_session(m, m->lsess, s, ip, NULL);
    }
    reap_all(m->lsess, "LISTENER");
    return 0;
}

/* Counts each partner's work that no session holds: sends due (a claimed
** one is ACTIVE), and EERPs or NERPs owed that no session is sending */
static void
due(OFTP_MON *m, int *sends, int *erps)
{
    OFTP_SPREC r;
    char last[8];
    long now = (long)time(NULL);
    int i, *n;

    memset(sends, 0, OFTP_MAXPTN * sizeof(*sends));
    memset(erps, 0, OFTP_MAXPTN * sizeof(*erps));
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (send_due(&r, now))
            n = sends;
        else if (r.dir == SPL_RECV && r.state == SPL_RECEIVED
                 && !erp_held(m, r.key))
            n = erps;
        else
            continue;
        for (i = 0; i < m->cfg.nptn; i++)
            if (strcmp(m->cfg.ptn[i].peer.name, r.partner) == 0)
                n[i]++;
    }
}

/* A partner that was not reached: each send due for it has had an
** attempt, and the partner is not called again before RETRY seconds.
** Not while another session with it runs (others): that one takes the
** sends itself. A call from the partner may still come in meanwhile, so
** each record is read again under the lock, as a claim reads it.
*/
static void
unreached(OFTP_MON *m, int i, int others)
{
    OFTP_PARTNER *p = &m->cfg.ptn[i];
    OFTP_SPREC r;
    char last[8];
    long now = (long)time(NULL);

    m->tcall[i] = now + m->cfg.retry;
    if (others)
        return;
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (strcmp(r.partner, p->peer.name) != 0 || !send_due(&r, now))
            continue;
        lock(m, 0);
        if (oftp_spl_get(m->spool, last, &r) == 0 && send_due(&r, now)) {
            r.attempts++;
            r.tlast = now;
            r.reasby = 'L';
            r.reas = 0;
            settext(&r, "partner not reached");
            failed(m, &r);
            oftp_spl_put(m->spool, &r);
        }
        unlock(m, 0);
    }
}

/* Deletes the finished transfers last changed more than days days ago
** (KEEPDAYS=, PURGE). Returns the number, or -1 when the spool failed;
** the console says either way, naming who asked.
*/
static int
purge(OFTP_MON *m, int days, const char *who)
{
    unsigned n = 0;
    long before = (long)time(NULL) - (long)days * 86400L;

    if (oftp_spl_purge(m->spool, before, &n) != 0) {
        oftp_mon_msg("OFTP236E SPOOL HOUSEKEEPING FAILED AFTER %u, BY %s",
                     n, who);
        return -1;
    }
    if (n > 0 || strcmp(who, "KEEPDAYS") != 0)
        oftp_mon_msg("OFTP235I %u FINISHED TRANSFERS OLDER THAN %d DAYS"
                     " DELETED BY %s", n, days, who);
    return (int)n;
}

static int
sched_thread(void *arg1, void *arg2)
{
    OFTP_MON *m = arg1;
    int sends[OFTP_MAXPTN], erps[OFTP_MAXPTN];
    int i;

    (void)arg2;
    while (!m->stop) {
        long now = (long)time(NULL), t = 1000;

        /* 10 s, earlier when a partner is due to be called again before
        ** then (a pause after ESID 08 counts in seconds), or when woken */
        for (i = 0; i < m->cfg.nptn; i++)
            if (m->tcall[i] > now && (m->tcall[i] - now) * 100 < t)
                t = (m->tcall[i] - now) * 100;
        /* cleared after the wait, not before it: a post while the last
        ** round ran (a RELEASE during a connect) ends this wait at once
        ** instead of waiting for the next tick */
        ecb_timed_wait(&m->wake, (unsigned)t, 0);
        m->wake = 0;
        if (m->stop)
            break;
        reap(m->ssess);
        /* housekeeping once a day, the first time at the first tick */
        if (m->cfg.keepdays > 0 && now - m->tpurge >= 86400L) {
            m->tpurge = now;
            purge(m, m->cfg.keepdays, "KEEPDAYS");
        }
        due(m, sends, erps);
        /* Another session to a partner, up to SESSIONS, while more files
        ** wait than our calls not yet under way will take: each of those
        ** is counted as taking one. EERPs and NERPs alone open a session
        ** only when none is running, since a running one sends them at
        ** its next turn; so does the operator's CALL.
        */
        for (i = 0; i < m->cfg.nptn && !m->stop; i++) {
            OFTP_PARTNER *p = &m->cfg.ptn[i];
            int s, others;
            int more = sends[i] > m->calling[i]
                       || (erps[i] > 0 && m->busy[i] == 0);

            if (!(more || m->callreq[i]) || !p->enabled || !p->call
                || (!m->callreq[i] && (long)time(NULL) < m->tcall[i]))
                continue;
            lock(m, 0);
            if (m->busy[i] >= p->sessions || m->nsess >= m->cfg.maxsess
                || (!more && m->busy[i] > 0)) {
                unlock(m, 0);
                continue;
            }
            m->busy[i]++;
            m->calling[i]++;
            unlock(m, 0);
            m->callreq[i] = 0;
            oftp_mon_msg("OFTP206P %s: CALLING AT %s PORT %d", p->peer.name,
                         p->host, p->port);
            if ((s = oftp_net_connect(p->host, p->port, 5)) < 0) {
                oftp_mon_msg("OFTP207W %s: NOT REACHED", p->peer.name);
                lock(m, 0);
                others = m->busy[i] - 1;
                unlock(m, 0);
                unreached(m, i, others);
                lock(m, 0);
                m->busy[i]--;
                m->calling[i]--;
                unlock(m, 0);
                continue;
            }
            start_session(m, m->ssess, s, p->host, p);
        }
    }
    reap_all(m->ssess, "SCHEDULER");
    return 0;
}

/* ---- requests ---------------------------------------------------- */

/* Where a request's answer goes (internals/design.md 7). The handlers
** write lines; the slot thread's put_slot() gathers them into the
** caller's reply area.
*/
typedef struct ctlout CTLOUT;
struct ctlout {
    void      (*put)(CTLOUT *o, const char *line);
    /* Who asks: the userid of the caller's ACEE, read by the SVC
    ** routine and never by the client, and ADMIN= decides what it may
    ** do.
    */
    int         admin;                  /* may see and change everything */
    char        user[9];                /* "" when nobody is known       */
};

/* May the asker see and change transfer r: an administrator, or the
** owner. No userid owns nothing, so a received file, which has no
** owner, is an administrator's.
*/
static int
ctl_may(const CTLOUT *o, const OFTP_SPREC *r)
{
    return o->admin || (o->user[0] != '\0' && strcmp(o->user, r->owner) == 0);
}

/* For the console: who changed a transfer */
static const char *
ctl_by(const CTLOUT *o)
{
    return o->user[0] ? o->user : "A CALLER WITHOUT USERID";
}

static void
ctl_put(CTLOUT *o, const char *line)
{
    o->put(o, line);
}

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
ctl_send(OFTP_MON *m, CTLOUT *o, const char *line)
{
    OFTP_SPREC r;
    OFTP_PARTNER *p;
    char partner[16], dsn[60], vfn[40], fmt[4], xl[4], user[12];
    char hold[4], disp[8], reply[100], sdate[8], stime[8], swin[16];
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
    kw(line, "HOLD", hold, sizeof(hold));
    kw(line, "DISP", disp, sizeof(disp));
    kw(line, "STRTDATE", sdate, sizeof(sdate));
    kw(line, "STRTTIME", stime, sizeof(stime));
    kw(line, "SENDWIN", swin, sizeof(swin));
    if (disp[0] && strcmp(disp, "KEEP") != 0 && strcmp(disp, "DELETE") != 0) {
        ctl_put(o, "508 DISP is KEEP or DELETE");
        return;
    }
    if ((p = oftp_cfg_find(&m->cfg, partner)) == NULL) {
        ctl_put(o, "501 unknown or disabled partner");
        return;
    }
    memset(&r, 0, sizeof(r));
    {
        struct tm nt;

        localtime_r(&now, &nt);
        if (oftp_sch_start(sdate, stime, &nt, (long)now, &r.tstart) != 0) {
            ctl_put(o, "508 STRTDATE is *, *+n or ddd; STRTTIME is hh:mm");
            return;
        }
        if (swin[0] && oftp_sch_window(swin, &r.winfrom, &r.winto) != 0) {
            ctl_put(o, "508 SENDWIN is hh:mm-hh:mm");
            return;
        }
    }
    /* a partner behind another: the session goes to that one */
    if (p->via[0]) {
        OFTP_PARTNER *v = oftp_cfg_find(&m->cfg, p->via);

        if (v == NULL) {
            ctl_put(o, "501 the partner it is reached through is disabled");
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
    if (disp[0] == 'D' && r.member[0]) {
        ctl_put(o, "510 DISP=DELETE takes a whole data set, not a member");
        return;
    }
    if (!oftp_dsn_valid(dsn)) {
        ctl_put(o, "502 data set not found");
        return;
    }
    /* The caller's authority decides, not OFTPD's, which reads the
    ** file later (oftpsec.h). Before the existence check: a data set
    ** the caller may not read answers the same whether it exists or
    ** not.
    */
    {
        char why[90];

        if (oftp_sec_may_read(o->user, dsn, disp[0] == 'D', why,
                              sizeof(why)) != 0) {
            snprintf(reply, sizeof(reply), "509 not authorized: %s", why);
            ctl_put(o, reply);
            oftp_mon_msg("OFTP238W SEND OF %s REFUSED: %s", dsn, why);
            return;
        }
    }
    if (!exists(dsn)) {
        ctl_put(o, "502 data set not found");
        return;
    }
    if (vfn[0] == '\0') {
        if (strlen(r.member[0] ? r.member : dsn) > OFTP_DSNLEN) {
            ctl_put(o, "503 name longer than 26, give VFN=");
            return;
        }
        strcpy(vfn, r.member[0] ? r.member : dsn);
    }
    if (strlen(vfn) > OFTP_DSNLEN) {
        ctl_put(o, "503 VFN longer than 26");
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
            ctl_put(o, "502 data set cannot be opened");
            return;
        }
        /* one record, on the heap: this thread's stack is 32 KB */
        if ((one = malloc((size_t)rf->lrecl + 4)) == NULL) {
            rclose(rf);
            ctl_put(o, "451 no storage");
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
    snprintf(r.owner, sizeof(r.owner), "%s", o->user);
    snprintf(r.user, sizeof(r.user), "%s", user);
    strcpy(r.dsn, vfn);
    strcpy(r.local, dsn);
    r.disp = disp[0] == 'D' ? 'D' : 0;
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
        ctl_put(o, "451 spool not available");
        return;
    }
    /* 210: done, with a warning */
    snprintf(reply, sizeof(reply), "%s %.8s %s for %s%s%s as %s%s",
             empty ? "210" : "200", r.key,
             r.state == SPL_HELD ? "queued and held" : "queued",
             r.target[0] ? r.target : r.partner, r.target[0] ? " via " : "",
             r.target[0] ? r.partner : "", r.dsn,
             empty ? " -- the data set is empty" : "");
    ctl_put(o, reply);
    oftp_mon_msg("OFTP230P %.8s QUEUED FOR %s: %s AS %s", r.key, r.partner,
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
ctl_list(OFTP_MON *m, CTLOUT *o, const char *line)
{
    OFTP_SPREC r;
    char last[8], partner[16], status[16], mine[4], out[200];
    int n = 0, own;

    kw(line, "PARTNER", partner, sizeof(partner));
    kw(line, "STATUS", status, sizeof(status));
    kw(line, "MINE", mine, sizeof(mine));
    /* Only one's own: asked for with MINE=Y, and all there is for
    ** anyone but an administrator
    */
    own = mine[0] == 'Y' || !o->admin;
    last[0] = '\0';
    while (oftp_spl_next(m->spool, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (partner[0] && strcmp(partner, r.partner) != 0
            && strcmp(partner, r.target) != 0)
            continue;
        if (status[0] && strncmp(status, state_name(r.state), strlen(status)) != 0)
            continue;
        if (own && !(o->user[0] && strcmp(o->user, r.owner) == 0))
            continue;
        snprintf(out, sizeof(out), "100 %.8s %-4s %-9s %-8s %-26s %s",
                 r.key, r.dir == SPL_SEND ? "SEND" : "RECV",
                 state_name(r.state), r.target[0] ? r.target : r.partner,
                 r.dsn, r.local);
        ctl_put(o, out);
        n++;
    }
    snprintf(out, sizeof(out), "200 %d transfers", n);
    ctl_put(o, out);
}

static void
ctl_show(OFTP_MON *m, CTLOUT *o, const char *line)
{
    OFTP_SPREC r;
    char key[12], out[200];

    kw(line, "KEY", key, sizeof(key));
    if (strlen(key) != 8 || oftp_spl_get(m->spool, key, &r) != 0) {
        ctl_put(o, "504 no such transfer");
        return;
    }
    if (!ctl_may(o, &r)) {
        ctl_put(o, "506 not your transfer");
        return;
    }
    snprintf(out, sizeof(out), "100 %.8s %s %s, partner %s%s%s, owner %s",
             r.key, r.dir == SPL_SEND ? "SEND" : "RECEIVE",
             state_name(r.state), r.target[0] ? r.target : r.partner,
             r.target[0] ? " via " : "", r.target[0] ? r.partner : "",
             r.owner[0] ? r.owner : "-");
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 virtual file %s %s %s, format %c, LRECL %u",
             r.dsn, r.date, r.time, r.fmt ? r.fmt : '-', r.lrecl);
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 data set %s%s%s%s%s", r.local,
             r.member[0] ? "(" : "", r.member, r.member[0] ? ")" : "",
             r.disp == 'D' ? ", deleted once the EERP is in" : "");
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 from %s to %s, %lu records, %u attempts",
             r.orig, r.dest, r.recs, r.attempts);
    ctl_put(o, out);
    if (r.tstart || r.winfrom || r.winto) {
        char nb[40] = "", wn[32] = "";

        if (r.tstart) {
            time_t tt = (time_t)r.tstart;
            struct tm tm;

            localtime_r(&tt, &tm);
            snprintf(nb, sizeof(nb), "not before %04d-%02d-%02d %02d:%02d",
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min);
        }
        if (r.winfrom || r.winto)
            snprintf(wn, sizeof(wn), "%sonly %02u:%02u-%02u:%02u",
                     nb[0] ? ", " : "", r.winfrom / 60U, r.winfrom % 60U,
                     r.winto / 60U, r.winto % 60U);
        snprintf(out, sizeof(out), "100 start %s%s", nb, wn);
        ctl_put(o, out);
    }
    if (r.reas)
        snprintf(out, sizeof(out), "100 reason %02d (%s): %s", r.reas,
                 r.reasby == 'P' ? "partner" : "local", r.text);
    else
        snprintf(out, sizeof(out), "100 %s", r.text);
    ctl_put(o, out);
    ctl_put(o, "200 end");
}

/* HOLD or RELEASE of every send to partner (all for "*") whose name
** matches vfn ("" any, "ABC*" a prefix), of those o may change -- o NULL
** is the operator, who may change all. HOLD takes QUEUED and WAITING,
** RELEASE takes HELD; anything else is left as it is. A send that
** changed between the walk and the write (a session claimed it) is left
** to its new state. *n gets the number changed; -1 when the spool failed.
*/
static int
bulk(OFTP_MON *m, const CTLOUT *o, int hold, const char *partner,
     const char *vfn, unsigned *n)
{
    OFTP_SPREC r;
    char last[8], was;
    int rc;

    *n = 0;
    last[0] = '\0';
    while ((rc = oftp_spl_next(m->spool, last, &r)) == 0) {
        memcpy(last, r.key, 8);
        if (r.dir != SPL_SEND || !oftp_spl_match(&r, partner, vfn)
            || (o != NULL && !ctl_may(o, &r)))
            continue;
        was = r.state;
        if (hold) {
            if (was != SPL_QUEUED && was != SPL_WAITING)
                continue;
            r.state = SPL_HELD;
            settext(&r, "held");
        } else {
            if (was != SPL_HELD)
                continue;
            r.state = SPL_QUEUED;
            r.attempts = 0;
            settext(&r, "released");
        }
        rc = oftp_spl_putif(m->spool, &r, was);
        if (rc == OFTP_SPL_CHANGED || rc == OFTP_SPL_NOTFOUND)
            continue;
        if (rc != 0)
            return -1;
        (*n)++;
    }
    if (rc != OFTP_SPL_NOTFOUND)
        return -1;
    if (*n > 0 && !hold)
        ecb_post(&m->wake, 0);
    return 0;
}

/* DELETE, HOLD, RELEASE, RETRY of one transfer; HOLD and RELEASE with
** PARTNER= instead of KEY= go to bulk() */
static void
ctl_change(OFTP_MON *m, CTLOUT *o, const char *verb, const char *line)
{
    OFTP_SPREC r;
    char key[12], out[80], was;
    int rc;

    kw(line, "KEY", key, sizeof(key));
    if (key[0] == '\0' && (strcmp(verb, "HOLD") == 0
                           || strcmp(verb, "RELEASE") == 0)) {
        char ptn[12], vfn[OFTP_DSNLEN + 2];
        unsigned n;

        kw(line, "PARTNER", ptn, sizeof(ptn));
        kw(line, "VFN", vfn, sizeof(vfn));
        if (ptn[0] == '\0') {
            ctl_put(o, "508 give KEY= or PARTNER= (PARTNER=* for all)");
            return;
        }
        if (strcmp(ptn, "*") != 0 && oftp_cfg_find(&m->cfg, ptn) == NULL) {
            ctl_put(o, "501 unknown partner");
            return;
        }
        if (bulk(m, o, verb[0] == 'H', ptn, vfn, &n) != 0) {
            ctl_put(o, "451 spool not available");
            return;
        }
        snprintf(out, sizeof(out), "200 %u sends %s", n,
                 verb[0] == 'H' ? "held" : "released");
        ctl_put(o, out);
        oftp_mon_msg("OFTP233P %u SENDS FOR %s%s%s %s BY %s", n, ptn,
                     vfn[0] ? " " : "", vfn,
                     verb[0] == 'H' ? "HELD" : "RELEASED", ctl_by(o));
        return;
    }
    if (strlen(key) != 8 || oftp_spl_get(m->spool, key, &r) != 0) {
        ctl_put(o, "504 no such transfer");
        return;
    }
    if (!ctl_may(o, &r)) {
        ctl_put(o, "506 not your transfer");
        return;
    }
    if (r.state == SPL_ACTIVE) {
        ctl_put(o, "450 transfer is active, try later");
        return;
    }
    if (strcmp(verb, "DELETE") == 0) {
        if (oftp_spl_del(m->spool, r.key) != 0) {
            ctl_put(o, "451 spool not available");
            return;
        }
        snprintf(out, sizeof(out), "200 %.8s deleted", r.key);
        ctl_put(o, out);
        oftp_mon_msg("OFTP231P %.8s DELETED BY %s", r.key, ctl_by(o));
        return;
    }
    if (r.dir != SPL_SEND) {
        ctl_put(o, "505 only a send can be held, released or retried");
        return;
    }
    was = r.state;
    if (strcmp(verb, "HOLD") == 0) {
        if (r.state != SPL_QUEUED && r.state != SPL_WAITING) {
            ctl_put(o, "505 only a queued or waiting send can be held");
            return;
        }
        r.state = SPL_HELD;
        settext(&r, "held");
    } else if (strcmp(verb, "RELEASE") == 0) {
        if (r.state != SPL_HELD) {
            ctl_put(o, "505 not held");
            return;
        }
        r.state = SPL_QUEUED;
        r.attempts = 0;
        settext(&r, "released");
    } else {                            /* RETRY */
        if (r.state == SPL_QUEUED) {
            ctl_put(o, "505 already queued");
            return;
        }
        /* a SENT file goes out once more: the partner sees it twice */
        r.state = SPL_QUEUED;
        r.attempts = 0;
        r.reas = 0;
        r.erp = 0;
        settext(&r, "queued again");
    }
    /* only if nothing changed it since the read: a session may have
    ** claimed it in between, and its state wins */
    if ((rc = oftp_spl_putif(m->spool, &r, was)) == OFTP_SPL_CHANGED
        || rc == OFTP_SPL_NOTFOUND) {
        ctl_put(o, "450 transfer changed meanwhile, try again");
        return;
    }
    if (rc != 0) {
        ctl_put(o, "451 spool not available");
        return;
    }
    snprintf(out, sizeof(out), "200 %.8s %s", r.key, state_name(r.state));
    ctl_put(o, out);
    oftp_mon_msg("OFTP232P %.8s %s BY %s", r.key, verb, ctl_by(o));
    ecb_post(&m->wake, 0);
}

static void
ctl_partners(OFTP_MON *m, CTLOUT *o)
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
        ctl_put(o, out);
    }
    snprintf(out, sizeof(out), "200 %d partners", m->cfg.nptn);
    ctl_put(o, out);
}

static void
ctl_status(OFTP_MON *m, CTLOUT *o)
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
    snprintf(out, sizeof(out), "100 Odette ID %s, OFTP port %d, SVC %d",
             m->cfg.odetteid, m->cfg.port, m->cfg.svc);
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 sessions %d of %d, partners %d of %d enabled",
             m->nsess, m->cfg.maxsess, on, m->cfg.nptn);
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 spool %d: queued %d, waiting %d, held %d, "
             "active %d, sent %d", total, cnt[SPL_QUEUED & 0x7F],
             cnt[SPL_WAITING & 0x7F], cnt[SPL_HELD & 0x7F],
             cnt[SPL_ACTIVE & 0x7F], cnt[SPL_SENT & 0x7F]);
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 delivered %d, rejected %d, failed %d, "
             "received %d, confirmed %d", cnt[SPL_DELIVERED & 0x7F],
             cnt[SPL_REJECTED & 0x7F], cnt[SPL_FAILED & 0x7F],
             cnt[SPL_RECEIVED & 0x7F], cnt[SPL_CONFIRMED & 0x7F]);
    ctl_put(o, out);
    snprintf(out, sizeof(out), "100 received files go to %s, %s",
             m->cfg.inprefix, oftp_version());
    ctl_put(o, out);
    ctl_put(o, "200 status");
}

static void
ctl_call(OFTP_MON *m, CTLOUT *o, const char *line)
{
    OFTP_PARTNER *p;
    char partner[16], out[80];

    if (!o->admin) {
        ctl_put(o, "507 needs ADMIN= in OFTPPRM");
        return;
    }
    kw(line, "PARTNER", partner, sizeof(partner));
    if ((p = oftp_cfg_find(&m->cfg, partner)) == NULL || !p->call) {
        ctl_put(o, "501 not a partner that can be called");
        return;
    }
    m->callreq[ptn_index(m, p)] = 1;
    ecb_post(&m->wake, 0);
    snprintf(out, sizeof(out), "200 call of %s requested", p->peer.name);
    ctl_put(o, out);
}

/* One request, whatever carried it: line in, lines out through o */
/* PURGE [DAYS=n]: housekeeping now, administrators only. Without DAYS
** it is KEEPDAYS; DAYS=0 takes every finished transfer, however young.
*/
static void
ctl_purge(OFTP_MON *m, CTLOUT *o, const char *line)
{
    char d[8], out[80];
    char *end;
    long days;
    int n;

    if (!o->admin) {
        ctl_put(o, "507 needs ADMIN= in OFTPPRM");
        return;
    }
    kw(line, "DAYS", d, sizeof(d));
    if (d[0] == '\0') {
        if (m->cfg.keepdays == 0) {
            ctl_put(o, "508 KEEPDAYS=0 keeps everything: say DAYS=n");
            return;
        }
        days = m->cfg.keepdays;
    } else {
        days = strtol(d, &end, 10);
        if (*end != '\0' || days < 0 || days > 9999) {
            ctl_put(o, "508 DAYS must be 0 to 9999");
            return;
        }
    }
    if ((n = purge(m, (int)days, ctl_by(o))) < 0) {
        ctl_put(o, "451 spool not available");
        return;
    }
    snprintf(out, sizeof(out), "200 %d finished transfers deleted", n);
    ctl_put(o, out);
}

static void
ctl_request(OFTP_MON *m, CTLOUT *o, char *line)
{
    char *c;

    for (c = line; *c; c++)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - 'a' + 'A');
    if (strncmp(line, "SEND ", 5) == 0)
        ctl_send(m, o, line);
    else if (strncmp(line, "LIST", 4) == 0)
        ctl_list(m, o, line);
    else if (strncmp(line, "SHOW", 4) == 0)
        ctl_show(m, o, line);
    else if (strncmp(line, "DELETE", 6) == 0)
        ctl_change(m, o, "DELETE", line);
    else if (strncmp(line, "HOLD", 4) == 0)
        ctl_change(m, o, "HOLD", line);
    else if (strncmp(line, "RELEASE", 7) == 0)
        ctl_change(m, o, "RELEASE", line);
    else if (strncmp(line, "RETRY", 5) == 0)
        ctl_change(m, o, "RETRY", line);
    else if (strncmp(line, "CALL", 4) == 0)
        ctl_call(m, o, line);
    else if (strncmp(line, "PARTNERS", 8) == 0)
        ctl_partners(m, o);
    else if (strncmp(line, "STATUS", 6) == 0)
        ctl_status(m, o);
    else if (strncmp(line, "PURGE", 5) == 0)
        ctl_purge(m, o, line);
    else if (strncmp(line, "REFRESH", 7) == 0) {
        char out[80];
        int on;

        if (!o->admin) {
            ctl_put(o, "507 needs ADMIN= in OFTPPRM");
            return;
        }
        if ((on = oftp_mon_refresh(m)) < 0)
            ctl_put(o, "451 partner member cannot be read");
        else {
            snprintf(out, sizeof(out), "200 partners reloaded, %d enabled", on);
            ctl_put(o, out);
        }
    }
    else
        ctl_put(o, "500 unknown request");
}

/* ---- the cross-memory slot thread -------------------------------- */
/* The second transport for the same requests (internals/xmem.md
** section 4, steps 9 and 10). Everything here works on an
** OFTP_SVCJOB and a plain char buffer, both in OFTPD's own key-8
** storage: oftp#svc.c copies a slot out and writes the answer back,
** and no line below ever holds a CSA address. That is what lets the
** ordinary ctl_* handlers serve a cross-memory request without
** knowing one arrived -- and it is why this half does not wait on
** the key-8 read oftpd#15 measures.
*/

static void join(CTHDTASK **t);

/* Where a cross-memory answer accumulates. CTLOUT must stay the first
** member: ctl_put() hands the handlers' output back as a CTLOUT * and
** this casts it down again.
*/
typedef struct slotout {
    CTLOUT   o;
    char    *buf;
    unsigned cap;
    unsigned len;
    unsigned sent;      /* 1xx lines the caller has after this piece  */
    unsigned skip;      /* 1xx lines it already had: the resume index */
    int      more;      /* the buffer ran out: this is a piece        */
} SLOTOUT;

/* One line into the reply area.
**
** Long answers come in pieces, and the boundary is counted in 1xx
** lines and nothing else. A 1xx line is a row of the answer; the
** single 2xx/4xx/5xx line that ends one is its status, belongs to
** every piece that ends, and counting it would make the resume index
** mean two different things on the two sides of the SVC.
**
** A piece never ends inside a line. The client reads whole lines, and
** half of one is indistinguishable from a short one -- so when the
** next line does not fit, the piece ends before it.
*/
static void
put_slot(CTLOUT *co, const char *line)
{
    SLOTOUT *o = (SLOTOUT *)co;
    unsigned n;

    if (line == NULL)
        return;
    if (line[0] == '1' && o->sent < o->skip) {
        o->sent++;                      /* the caller has this row    */
        return;
    }
    if (o->more)
        return;
    n = (unsigned)strlen(line);
    if (o->len + n + 1 > o->cap) {
        o->more = 1;
        return;
    }
    memcpy(o->buf + o->len, line, n);
    o->len += n;
    o->buf[o->len++] = '\n';
    if (line[0] == '1')
        o->sent++;
}

/* The request line as the handlers will see it, and who sent it.
**
** **The caller's own OWNER= is dropped, and the userid the routine
** read out of the caller's ACEE goes to the handlers beside the line,
** never in it** -- that is the property the whole transport exists
** for: the request block has no field a client could have put a
** userid in, and a line that names one is answered as if it did not.
**
** What the userid may do is ADMIN='s to say (ctl_may()): an
** administrator sees and changes every transfer, anyone else only
** their own. An empty userid -- an address space without an ACEE --
** is nobody, and owns nothing.
**
** **`USER=` is deliberately left alone**, and that is not an
** oversight in the same spirit. It is not an identity on this system:
** ctl_send() puts it in the spool record's `user`, which travels in
** the OFTP SFID user-data field (oftp#cmd.c, +48/+52) as free-form
** text agreed between partners. The identity is `owner`, and that is
** the one taken from the slot.
*/
static void
svc_line(OFTP_MON *m, const OFTP_SVCJOB *j, char *out, size_t cap,
         CTLOUT *who)
{
    const char *p = j->req;
    size_t      w = 0;
    int         n;

    while (*p != '\0' && w + 1 < cap) {
        const char *s = p;
        size_t      l = 0;

        while (s[l] != '\0' && s[l] != ' ')
            l++;
        /* ctl_request() uppercases the line afterwards, so a keyword
        ** typed in either case has to be recognised here
        */
        if (!(l >= 6 && (s[0] == 'O' || s[0] == 'o')
                     && (s[1] == 'W' || s[1] == 'w')
                     && (s[2] == 'N' || s[2] == 'n')
                     && (s[3] == 'E' || s[3] == 'e')
                     && (s[4] == 'R' || s[4] == 'r')
                     && s[5] == '=')) {
            size_t i;

            if (w != 0 && w + 1 < cap)
                out[w++] = ' ';
            for (i = 0; i < l && w + 1 < cap; i++)
                out[w++] = s[i];
        }
        p = s + l;
        while (*p == ' ')
            p++;
    }
    out[w] = '\0';

    for (n = 8; n > 0 && (j->user[n - 1] == ' ' || j->user[n - 1] == '\0');
         n--)
        ;
    memcpy(who->user, j->user, (size_t)n);
    who->user[n] = '\0';
    who->admin = oftp_cfg_admin(&m->cfg, who->user);
}

static int
svc_thread(void *arg1, void *arg2)
{
    OFTP_MON   *m = arg1;
    OFTP_SVCJOB j;
    SLOTOUT     o;
    char        line[OFTP_SVC_REQMAX + 24];
    char        buf[OFTP_SVC_REPMAX];
    long        swept = 0;
    unsigned    i;

    (void)arg2;
    while (!m->stop && !m->svcstop) {
        long now;

        /* Zero the ECB BEFORE the scan, never after. A POST landing
        ** between the two is found by the scan; one landing after it
        ** leaves the ECB posted and the next wait comes straight
        ** back. The other order loses a wake-up, and nothing on
        ** either side would notice -- the caller waits until the
        ** stop.
        */
        m->svcwake = 0;
        for (i = 0; i < OFTP_SVC_NSLOT; i++) {
            if (!oftp_svc_take(m, i, &j))
                continue;
            memset(&o, 0, sizeof(o));
            o.o.put  = put_slot;
            o.buf    = buf;
            o.cap    = j.cap;
            o.skip   = j.resume;
            svc_line(m, &j, line, sizeof(line), &o.o);
#ifdef OFTP_DEBUG_SLOW
            /* Test builds only (-DOFTP_DEBUG_SLOW), never shipped: a
            ** request carrying SLOW=n holds the slot thread n seconds
            ** before it is served, so that other callers stay parked
            ** behind it (#34). A stop cuts the wait short and leaves
            ** this request and every one behind it unserved, as if the
            ** stop had come before the thread took them: that is the
            ** path where oftp_svc_stop() has parked slots to post.
            */
            {
                const char *sl = strstr(line, "SLOW=");
                int k, secs = sl != NULL ? atoi(sl + 5) : 0;

                for (k = 0; k < secs && !m->stop && !m->svcstop; k++) {
                    ECB e = 0;

                    ecb_timed_wait(&e, 100, 0);
                }
                if (secs > 0 && (m->stop || m->svcstop))
                    break;
                /* LATE=n holds it n seconds whatever happens and then
                ** serves it, so that its answer comes after a stop
                ** has cleared ACTIVE: the request was carried out and
                ** its caller must get the answer, not "not reached".
                */
                sl = strstr(line, "LATE=");
                secs = sl != NULL ? atoi(sl + 5) : 0;
                for (k = 0; k < secs; k++) {
                    ECB e = 0;

                    ecb_timed_wait(&e, 100, 0);
                }
            }
#endif
            ctl_request(m, &o.o, line);
            /* A single line longer than the caller's whole buffer
            ** would answer MORE for ever and the client would ask
            ** for ever.
            **
            ** It cannot happen with today's handlers, and the bound
            ** is structural rather than arithmetic: every reply line
            ** is formatted by snprintf into a `char out[N]` local,
            ** and the largest N in this file is 200 (ctl_list,
            ** ctl_show). With the newline that is 201 bytes against
            ** an OFTP_SVC_REPMIN of 256. A handler that grows its
            ** buffer past 255 breaks that, which is why this says so
            ** instead of guarding silently.
            */
            if (o.more && o.len == 0) {
                const char *e = "552 reply does not fit in the buffer\n";

                o.len = (unsigned)strlen(e);
                if (o.len > o.cap)
                    o.len = o.cap;
                memcpy(buf, e, o.len);
                o.more = 0;
            }
            if (!oftp_svc_answer(m, i, buf, o.len, o.sent,
                                 o.more ? OFTP_SVC_MORE : OFTP_SVC_OK))
                oftp_mon_msg("OFTP064W CROSS MEMORY: THE OWNER OF SLOT %u"
                             " IS GONE, ANSWER DISCARDED", i);
        }
        /* The sweep counts passes, so a pass has to be worth about a
        ** second whatever the traffic is -- see OFTP_SW_* in
        ** oftpsvc.h for why that is not a detail.
        */
        now = (long)time(NULL);
        if (now != swept) {
            swept = now;
            oftp_svc_sweep(m);
        }
        if (m->stop || m->svcstop)
            break;
        /* 100 BINTVL units = 1 s. A negative rc means STIMER REAL
        ** failed and nothing waited at all (libc370 #94); fall back
        ** to the untimed wait rather than spin. The sweep stops
        ** ticking, requests are still served, and the stop still
        ** wakes us.
        */
        if (ecb_timed_wait(&m->svcwake, 100, 0) < 0)
            ecb_wait(&m->svcwake);
    }
    return 0;
}

int
oftp_mon_svc_start(OFTP_MON *m)
{
    m->svcstop = 0;
    m->t_svc = cthread_create_ex(svc_thread, m, NULL, 32 * 1024);
    if (m->t_svc == NULL) {
        oftp_mon_msg("OFTP065E CROSS MEMORY SLOT THREAD NOT STARTED,"
                     " CROSS MEMORY STAYS OFF");
        return -1;
    }
    /* The thread first, the flag second: from the moment ACTIVE goes
    ** on, any program on the system can park a request in a slot.
    */
    oftp_svc_activate(m);
    return 0;
}

void
oftp_mon_svc_stop(OFTP_MON *m)
{
    oftp_svc_deactivate(m);
    m->svcstop = 1;
    ecb_post(&m->svcwake, 0);
    join(&m->t_svc);
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
    oftp_mon_msg("OFTP051P LISTENING ON PORT %d, SOCKET %d", m->cfg.port,
                 m->lsock);
    m->t_listen = cthread_create_ex(listen_thread, m, NULL, 32 * 1024);
    m->t_sched = cthread_create_ex(sched_thread, m, NULL, 32 * 1024);
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
    /* Normally already gone: oftp_mon_svc_stop() runs ahead of
    ** oftp_svc_stop() so the anchor is not freed under the thread.
    ** This is the net for a path that skipped it -- join() takes a
    ** NULL and cthread_delete() clears the pointer, so it is a no-op
    ** the second time.
    */
    m->svcstop = 1;
    ecb_post(&m->svcwake, 0);
    join(&m->t_svc);
    /* closed numbers go back to the system: forget them, or an abend
    ** during the wait below would close whatever got them next
    */
    oftp_net_close(m->lsock);
    m->lsock = -1;
    for (i = 0; m->nsess > 0 && i < 60; i++) {
        ECB e = 0;

        if (i % 10 == 0)
            oftp_mon_msg("OFTP095I WAITING FOR %d SESSIONS", m->nsess);
        ecb_timed_wait(&e, 100, 0);
    }
}

int
oftp_mon_abend(OFTP_MON *m)
{
    int i, n = 0, sk;

    /* The SVC entry FIRST, ahead of every socket. An entry nobody gives
    ** back makes that SVC number unavailable to every program on the
    ** system until the next IPL, where a socket left open costs one
    ** port until Hercules restarts -- and the closes below are the slow
    ** part. It issues no message of its own: in an S222 only the first
    ** and the last WTO are sure to reach the console, so the ESTAE
    ** spends its one on OFTP098E and reads m->svc_taken to say how this
    ** went.
    */
    oftp_svc_restore(m);

    /* No lock: the task that held it may be the one that failed. A
    ** session ending at this moment may close its socket as well; the
    ** second close of a number nobody has reused yet is harmless.
    */
    m->stop = 1;
    if (m->lsock > 0) {
        oftp_net_close(m->lsock);
        m->lsock = -1;
        n++;
    }
    for (i = 0; i < OFTP_MAXSESS; i++)
        if ((sk = m->ssock[i]) > 0) {
            m->ssock[i] = 0;
            oftp_net_close(sk);
            n++;
        }
    return n;
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
        oftp_mon_msg("OFTP040P RECOVERED %d SENDS, DISCARDED %d PARTIAL RECEIVES",
                     sends, recvs);
}

/* The largest block of subpool 0 a GETMAIN still gets, to 4 KB: what a
** new session's stack needs. Each probe is freed at once.
*/
static unsigned
largest_free(void)
{
    unsigned lo = 0, hi = 16u * 1024 * 1024;

    while (hi - lo > 4096) {
        unsigned mid = (lo + (hi - lo) / 2) & ~4095u;
        void *p = getmain(mid, 0);

        if (p != NULL) {
            freemain(p);
            lo = mid;
        } else
            hi = mid;
    }
    return lo;
}

/* What F OFTPD,TRACE has switched on, for D */
static void
show_trace(const OFTP_MON *m)
{
    char list[100];
    size_t n = 0;
    int i;

    if (m->trace_all) {
        oftp_mon_msg("OFTP127I TRACE ON FOR ALL SESSIONS");
        return;
    }
    list[0] = '\0';
    for (i = 0; i < m->cfg.nptn; i++) {
        const char *name = m->cfg.ptn[i].peer.name;

        if (!m->trace[i] || n + strlen(name) + 2 > sizeof(list))
            continue;
        n += (size_t)snprintf(list + n, sizeof(list) - n, " %s", name);
    }
    if (list[0] == '\0')
        oftp_mon_msg("OFTP127I TRACE OFF");
    else
        oftp_mon_msg("OFTP127I TRACE ON FOR PARTNERS:%s", list);
}

/* F OFTPD,TRACE ON|OFF [partner]. OFF without a partner clears all of it;
** OFF for one partner cannot take it out of a trace for all, so it says so.
*/
static void
cmd_trace(OFTP_MON *m, const char *onoff, const char *ptn)
{
    int on = strcmp(onoff, "ON") == 0;
    int i;

    if (!on && strcmp(onoff, "OFF") != 0) {
        oftp_mon_msg("OFTP126E TRACE NEEDS ON OR OFF, THEN A PARTNER OR NOTHING");
        return;
    }
    if (ptn[0] == '\0') {
        m->trace_all = (char)on;
        if (!on)
            for (i = 0; i < OFTP_MAXPTN; i++)
                m->trace[i] = 0;
        oftp_mon_msg("OFTP124I TRACE %s FOR ALL SESSIONS", onoff);
        return;
    }
    {
        OFTP_PARTNER *p = NULL;

        for (i = 0; p == NULL && i < m->cfg.nptn; i++)
            if (strcmp(m->cfg.ptn[i].peer.name, ptn) == 0)
                p = &m->cfg.ptn[i];
        if (p == NULL) {
            oftp_mon_msg("OFTP120E %s IS NOT A PARTNER", ptn);
            return;
        }
        if (!on && m->trace_all) {
            oftp_mon_msg("OFTP125E TRACE IS ON FOR ALL SESSIONS, "
                         "TRACE OFF WITHOUT A PARTNER ENDS IT");
            return;
        }
        m->trace[ptn_index(m, p)] = (char)on;
        oftp_mon_msg("OFTP124I TRACE %s FOR PARTNER %s", onoff, p->peer.name);
    }
}

static void
cmd_display(OFTP_MON *m, const char *what)
{
    int i;

    if (what[0] == 'P') {
        for (i = 0; i < m->cfg.nptn; i++) {
            OFTP_PARTNER *p = &m->cfg.ptn[i];
            char ss[32];

            ss[0] = '\0';
            if (m->busy[i])
                snprintf(ss, sizeof(ss), " %d OF %d SESSIONS", m->busy[i],
                         p->sessions);
            if (m->trace_all || m->trace[i])
                strcat(ss, " TRACE");
            oftp_mon_msg("OFTP100I %-8s %-25s %s%s LEVEL %d %s:%d", p->peer.name,
                         p->peer.code, p->enabled ? "ENABLED" : "DISABLED",
                         ss, p->peer.level, p->host[0] ? p->host : "-", p->port);
        }
        oftp_mon_msg("OFTP101I %d PARTNERS, %d SESSIONS", m->cfg.nptn, m->nsess);
        show_trace(m);
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
    } else if (what[0] == 'M') {
        CLIBGRT *grt = __grtget();
        /* the array's address, not the array: arraycount(&array) */
        unsigned th = grt != NULL && grt->grtcthrd != NULL
                      ? array_count(&grt->grtcthrd) : 0;

        oftp_mon_msg("OFTP102I LARGEST FREE BLOCK %u KB, %u SUBTASK HANDLES, "
                     "%d SESSIONS", largest_free() / 1024, th, m->nsess);
    } else {
        oftp_mon_msg("OFTP101I %d SESSIONS", m->nsess);
        show_trace(m);
    }
}

void
oftp_mon_command(OFTP_MON *m, const char *cmd)
{
    char verb[16], arg[40], arg2[40];
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
        while (*p && *p != ' ' && *p != ',' && n < sizeof(arg) - 1)
            arg[n++] = *p++;
        arg[n] = '\0';
        while (*p == ' ' || *p == ',')
            p++;
        n = 0;
        while (*p && *p != ' ' && *p != ',' && n < sizeof(arg2) - 1)
            arg2[n++] = *p++;
        arg2[n] = '\0';
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
    } else if (strcmp(verb, "HOLD") == 0 || strcmp(verb, "RELEASE") == 0) {
        /* F OFTPD,HOLD|RELEASE partner|ALL: every send to it */
        const char *ptn = strcmp(arg, "ALL") == 0 ? "*" : arg;
        unsigned n;

        if (arg[0] == '\0')
            oftp_mon_msg("OFTP123E %s NEEDS A PARTNER OR ALL", verb);
        else if (strcmp(ptn, "*") != 0 && oftp_cfg_find(&m->cfg, ptn) == NULL)
            oftp_mon_msg("OFTP120E %s IS NOT A PARTNER", arg);
        else if (bulk(m, NULL, verb[0] == 'H', ptn, "", &n) != 0)
            oftp_mon_msg("OFTP237E %s OF %s: SPOOL NOT AVAILABLE", verb, arg);
        else
            oftp_mon_msg("OFTP233I %u SENDS FOR %s %s BY OPERATOR", n, arg,
                         verb[0] == 'H' ? "HELD" : "RELEASED");
    } else if (strcmp(verb, "TRACE") == 0)
        cmd_trace(m, arg, arg2);
    else if (strcmp(verb, "PURGE") == 0) {
        /* F OFTPD,PURGE [days]: without a number, KEEPDAYS */
        char *end;
        long days = arg[0] ? strtol(arg, &end, 10) : m->cfg.keepdays;

        if (arg[0] && (*end != '\0' || days < 0 || days > 9999))
            oftp_mon_msg("OFTP122E PURGE: DAYS MUST BE 0-9999, NOT %s", arg);
        else if (!arg[0] && days == 0)
            oftp_mon_msg("OFTP122E PURGE NEEDS DAYS: KEEPDAYS=0 KEEPS EVERYTHING");
        else
            purge(m, (int)days, "OPERATOR");
    } else
        oftp_mon_msg("OFTP199E UNKNOWN COMMAND: %s", cmd);
}
