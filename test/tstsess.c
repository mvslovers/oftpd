/* TSTSESS - two session engines against each other (host only)
**
** The initiator runs in a child process, the responder in the parent,
** joined by a socket pair. Files live in memory. Each scenario checks
** what both sides saw; the child reports through its exit code.
*/
#include <errno.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mbtcheck.h>

#include "oftpdata.h"
#include "oftpsess.h"

/* ---- transport over a socket ------------------------------------- */

static int
x_send(void *ctx, const unsigned char *b, unsigned n)
{
    int fd = *(int *)ctx;

    while (n > 0) {
        ssize_t w = write(fd, b, n);

        if (w <= 0)
            return -1;
        b += w;
        n -= (unsigned)w;
    }
    return 0;
}

static int
x_recv(void *ctx, unsigned char *b, unsigned n, int secs)
{
    int fd = *(int *)ctx;

    while (n > 0) {
        struct pollfd p;
        ssize_t r;

        p.fd = fd;
        p.events = POLLIN;
        if (poll(&p, 1, secs * 1000) == 0)
            return OFTP_XTIMEOUT;
        r = read(fd, b, n);
        if (r == 0)
            return OFTP_XCLOSED;
        if (r < 0)
            return -1;
        b += r;
        n -= (unsigned)r;
    }
    return 0;
}

/* ---- files in memory --------------------------------------------- */

#define MAXF    4

typedef struct mout {
    const char *dsn;
    char        fmt;
    unsigned    lrecl;
    const char *recs[8];        /* F/V: records; U/T: recs[0] is the data */
    int         next;           /* read position                          */
    int         done;           /* 1 delivered, 2 refused                 */
    int         reas;
} MOUT;

typedef struct min {
    char        dsn[OFTP_DSNLEN + 1];
    char        fmt;
    unsigned char data[2048];   /* F/V: records joined by '|'             */
    unsigned    len;
    int         closed;         /* 1 kept, 2 discarded                    */
} MIN;

typedef struct mfiles {
    MOUT        out[MAXF];
    int         nout;
    MIN         in[MAXF];
    int         nin;
    int         erp_pending[MAXF];  /* EERPs owed for in[i]               */
    int         eerps;              /* EERPs received                     */
    int         refuse;             /* SFNA reason for every open_in      */
    int         nerp;               /* answer in[0] with a NERP, reason   */
    int         nerps;              /* NERPs received                     */
    int         nreas;              /* the last one's reason and creator  */
    char        ncrea[OFTP_IDLEN + 1];
    int         cut;                /* drop the line when an EERP arrives,
                                       before the RTR goes out          */
    int         erps_any;           /* EERPs received, for whatever file   */
    int         jump;               /* 'R' read, 'W' write: leave the
                                       engine by longjmp, as an abend
                                       under try() does; 'C': the process
                                       ends in its first write, which
                                       cuts the connection              */
} MFILES;

static jmp_buf abend_jb;

static int
m_next_out(void *ctx, const OFTP_PEER *peer, OFTP_SFIDCMD *f, OFTP_FH **fh)
{
    MFILES *m = ctx;
    int i;

    for (i = 0; i < m->nout; i++) {
        if (m->out[i].done == 0 && m->out[i].next == 0) {
            strcpy(f->dsn, m->out[i].dsn);
            strcpy(f->date, "20261012");
            strcpy(f->time, "1200000001");
            strcpy(f->dest, peer->code);
            strcpy(f->orig, peer->ourcode);
            f->fmt = m->out[i].fmt;
            f->lrecl = m->out[i].lrecl;
            f->fsiz = 1;
            *fh = &m->out[i];
            return 1;
        }
    }
    return 0;
}

static int
m_read(void *ctx, OFTP_FH *fh, unsigned char *b, unsigned cap, unsigned *n,
       int *last)
{
    MOUT *o = fh;
    const char *r;

    if (((MFILES *)ctx)->jump == 'R')
        longjmp(abend_jb, 1);
    if (o->fmt == 'U' || o->fmt == 'T') {
        r = o->recs[0];
        *n = (unsigned)strlen(r);
        memcpy(b, r, *n);
        *last = 1;
        o->next = 1;
        return 1;
    }
    r = o->recs[o->next];
    if (r == NULL)
        return 0;
    *n = (unsigned)strlen(r);
    if (*n > cap)
        return -1;
    memcpy(b, r, *n);
    o->next++;
    return 1;
}

static void
m_out_done(void *ctx, OFTP_FH *fh, int reas, char retry)
{
    MOUT *o = fh;

    (void)ctx;
    (void)retry;
    o->done = reas == 0 ? 1 : 2;
    o->reas = reas;
}

static int
m_open_in(void *ctx, const OFTP_PEER *peer, const OFTP_SFIDCMD *f,
          OFTP_FH **fh, char *retry)
{
    MFILES *m = ctx;
    MIN *in;

    (void)peer;
    if (m->refuse) {
        *retry = 'Y';
        return m->refuse;
    }
    in = &m->in[m->nin++];
    strcpy(in->dsn, f->dsn);
    in->fmt = f->fmt;
    in->len = 0;
    *fh = in;
    return 0;
}

static int
m_write(void *ctx, OFTP_FH *fh, const unsigned char *b, unsigned n, int eor)
{
    MIN *in = fh;

    if (((MFILES *)ctx)->jump == 'W')
        longjmp(abend_jb, 1);
    if (((MFILES *)ctx)->jump == 'C')
        _exit(0);
    memcpy(in->data + in->len, b, n);
    in->len += n;
    if (eor)
        in->data[in->len++] = '|';
    return 0;
}

static int
m_close_in(void *ctx, OFTP_FH *fh, int ok)
{
    MFILES *m = ctx;
    MIN *in = fh;

    in->closed = ok ? 1 : 2;
    if (ok)
        m->erp_pending[in - m->in] = 1;
    return 0;
}

static int
m_next_erp(void *ctx, const OFTP_PEER *peer, char *code, OFTP_ERPCMD *e,
           OFTP_FH **h)
{
    MFILES *m = ctx;
    int i;

    for (i = 0; i < m->nin; i++) {
        if (m->erp_pending[i]) {
            memset(e, 0, sizeof(*e));
            *code = OFTP_EERP;
            if (i == 0 && m->nerp) {
                *code = OFTP_NERP;
                e->reas = m->nerp;
                strcpy(e->crea, peer->ourcode);
            }
            strcpy(e->dsn, m->in[i].dsn);
            strcpy(e->date, "20261012");
            strcpy(e->time, "1200000001");
            strcpy(e->dest, peer->code);
            strcpy(e->orig, peer->ourcode);
            *h = &m->erp_pending[i];
            return 1;
        }
    }
    return 0;
}

static void
m_erp_sent(void *ctx, OFTP_FH *h)
{
    (void)ctx;
    *(int *)h = 0;
}

static void
m_erp_recv(void *ctx, const OFTP_PEER *peer, char code, const OFTP_ERPCMD *e)
{
    MFILES *m = ctx;
    int i;

    (void)peer;
    if (m->cut)
        _exit(0);
    m->erps_any++;
    if (code == OFTP_NERP) {
        m->nerps++;
        m->nreas = e->reas;
        strcpy(m->ncrea, e->crea);
        return;
    }
    for (i = 0; i < m->nout; i++)
        if (strcmp(m->out[i].dsn, e->dsn) == 0)
            m->eerps++;
}

static const OFTP_FILES mfiles_ops = {
    NULL, m_next_out, m_read, m_out_done, m_open_in, m_write, m_close_in,
    m_next_erp, m_erp_sent, m_erp_recv
};

/* ---- partners ---------------------------------------------------- */

static int looked_up;           /* the responder knows the partner      */
static OFTP_PEER resp_view;     /* how the responder sees the initiator */
static int lookup_rc;           /* not 0: the lookup answers this        */

static int
lookup(void *ctx, const char *code, OFTP_PEER *peer)
{
    (void)ctx;
    looked_up = 1;
    if (lookup_rc != 0)
        return lookup_rc;
    if (strcmp(code, resp_view.code) != 0)
        return -1;
    *peer = resp_view;
    return 0;
}

static void
quiet(void *ctx, int warn, const char *line)
{
    (void)ctx;
    (void)warn;
    (void)line;
}

/* The responder's log and trace switch; the initiator's side is quiet */
static OFTP_LOGFN g_log = quiet;
static OFTP_TRACEFN g_traced;
static const char *g_pw = "PW";         /* both sides' password         */
static const char *g_rpw;               /* the responder answers with this
                                           instead of g_pw, when set     */
static const char *g_ocode = "O0013000000MVS1"; /* initiator's own ID    */
static const char *g_dsn;               /* initiator's first file name   */

static void
peer_init(OFTP_PEER *p, const char *name, const char *code,
          const char *ourcode, int level, unsigned sdeb, unsigned cred,
          int cmpr)
{
    memset(p, 0, sizeof(*p));
    strcpy(p->name, name);
    strcpy(p->code, code);
    strcpy(p->ourcode, ourcode);
    strcpy(p->pswd_in, g_pw);
    strcpy(p->pswd_out, g_pw);
    p->level = level;
    p->sdeb = sdeb;
    p->cred = cred;
    p->mode = 'B';
    p->cmpr = cmpr;
}

/* ---- scenarios ----------------------------------------------------- */

typedef struct scen {
    const char *name;
    int         ilevel, rlevel;     /* levels offered / accepted         */
    unsigned    sdeb, cred;
    int         cmpr;
    const char *ipw;                /* password the initiator sends      */
    int         refuse;             /* responder refuses with this       */
    int         nerp;               /* responder answers a file with a
                                       NERP of this reason instead      */
} SCEN;

/* Initiator files: an F file, a T file */
static void
init_files(MFILES *m)
{
    memset(m, 0, sizeof(*m));
    m->nout = 2;
    m->out[0].dsn = g_dsn != NULL ? g_dsn : "PAYROLL.DATA";
    m->out[0].fmt = 'F';
    m->out[0].lrecl = 10;
    m->out[0].recs[0] = "AAAAAAAAAA";
    m->out[0].recs[1] = "BBBBBBBBBB";
    m->out[0].recs[2] = "          ";
    m->out[1].dsn = "POEM.TXT";
    m->out[1].fmt = 'T';
    m->out[1].recs[0] = "It is an ancient Mariner,\r\nAnd he stoppeth one of three.\r\n";
}

/* Responder files: a V file with an empty record */
static void
resp_files(MFILES *m)
{
    memset(m, 0, sizeof(*m));
    m->nout = 1;
    m->out[0].dsn = "BACK.V";
    m->out[0].fmt = 'V';
    m->out[0].lrecl = 100;
    m->out[0].recs[0] = "short";
    m->out[0].recs[1] = "";
    m->out[0].recs[2] = "a longer record of the variable file, longer than one subrecord of sixty-three";
}

static OFTP_SESS sess;

static void
sess_init(OFTP_SESS *s, int *fd, MFILES *m, int initiator)
{
    memset(s, 0, sizeof(*s));
    s->x.ctx = fd;
    s->x.send = x_send;
    s->x.recv = x_recv;
    s->f = mfiles_ops;
    s->f.ctx = m;
    s->p.lookup = lookup;
    s->log = initiator ? quiet : g_log;
    s->traced = initiator ? NULL : g_traced;
    s->initiator = initiator;
    s->timeout = 5;
}

/* The initiator's side, run in the child: 0 when it saw what it should */
static int
run_initiator(const SCEN *sc, int fd)
{
    MFILES m;
    int rc;

    init_files(&m);
    sess_init(&sess, &fd, &m, 1);
    peer_init(&sess.peer, "MVS2", "O0013000000MVS2", "O0013000000MVS1",
              sc->ilevel, sc->sdeb, sc->cred, sc->cmpr);
    strcpy(sess.peer.pswd_out, sc->ipw);
    rc = oftp_sess_run(&sess);

    if (sess.xbuf != NULL)
        return 20;
    if (g_rpw != NULL && strcmp(g_rpw, g_pw) != 0)
        return rc == 4 && sess.esid_local ? 0 : 9;
    if (strcmp(sc->ipw, g_pw) != 0)
        return rc == 4 && !sess.esid_local ? 0 : 10;
    if (sc->refuse)
        return rc == 0 && m.out[0].done == 2 && m.out[0].reas == sc->refuse ? 0 : 11;
    if (rc != 0)
        return 12;
    if (m.out[0].done != 1 || m.out[1].done != 1)
        return 13;
    if (m.nin != 1 || m.in[0].closed != 1)
        return 14;
    if (m.in[0].len != 86 || memcmp(m.in[0].data, "short||a longer", 15) != 0)
        return 15;
    if (m.eerps != (sc->nerp ? 1 : 2))  /* EERPs for both files */
        return 16;
    /* a NERP arrives from 1.4 on; at 1.3 there is none, nor an EERP */
    if (sc->nerp && sess.level >= OFTP_LEV14
        && (m.nerps != 1 || m.nreas != sc->nerp
            || strcmp(m.ncrea, "O0013000000MVS2") != 0))
        return 18;
    if (sc->nerp && sess.level < OFTP_LEV14 && m.nerps != 0)
        return 19;
    if (sess.level != (sc->ilevel < sc->rlevel ? sc->ilevel : sc->rlevel))
        return 17;
    return 0;
}

static void
scenario(const SCEN *sc)
{
    static const char *fwant = "AAAAAAAAAA|BBBBBBBBBB|          |";
    static const char *twant = "It is an ancient Mariner,\r\nAnd he stoppeth one of three.\r\n";
    MFILES m;
    int sv[2], status = -1, rc;
    pid_t pid;
    char msg[120];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(sv[1]);
        rc = run_initiator(sc, sv[0]);
        close(sv[0]);
        _exit(rc);
    }
    close(sv[0]);

    resp_files(&m);
    m.refuse = sc->refuse;
    m.nerp = sc->nerp;
    peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
              sc->rlevel, sc->sdeb, sc->cred, sc->cmpr);
    if (g_rpw != NULL)
        strcpy(resp_view.pswd_out, g_rpw);
    sess_init(&sess, &sv[1], &m, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    rc = oftp_sess_run(&sess);
    close(sv[1]);
    waitpid(pid, &status, 0);
    snprintf(msg, sizeof(msg), "%s: no record buffer left", sc->name);
    CHECK(sess.xbuf == NULL, msg);

    snprintf(msg, sizeof(msg), "%s: initiator side (child rc %d)", sc->name,
             WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, msg);

    if (strcmp(sc->ipw, g_pw) != 0) {
        snprintf(msg, sizeof(msg), "%s: responder ends with ESID 04", sc->name);
        CHECK(rc == 4 && sess.esid_local, msg);
        return;
    }
    if (g_rpw != NULL && strcmp(g_rpw, g_pw) != 0) {
        snprintf(msg, sizeof(msg), "%s: responder is left by ESID 04", sc->name);
        CHECK(rc == 4 && !sess.esid_local, msg);
        return;
    }
    snprintf(msg, sizeof(msg), "%s: normal end (rc %d)", sc->name, rc);
    CHECK(rc == 0, msg);
    if (sc->refuse) {
        snprintf(msg, sizeof(msg), "%s: nothing received", sc->name);
        CHECK(m.nin == 0, msg);
        return;
    }
    snprintf(msg, sizeof(msg), "%s: F file records intact", sc->name);
    CHECK(m.nin == 2 && m.in[0].fmt == 'F' && m.in[0].len == strlen(fwant)
          && memcmp(m.in[0].data, fwant, m.in[0].len) == 0, msg);
    snprintf(msg, sizeof(msg), "%s: T file stream intact", sc->name);
    CHECK(m.in[1].fmt == 'T' && m.in[1].len == strlen(twant)
          && memcmp(m.in[1].data, twant, m.in[1].len) == 0, msg);
    snprintf(msg, sizeof(msg), "%s: no response left owed", sc->name);
    CHECK(m.erp_pending[0] == 0 && m.erp_pending[1] == 0, msg);
    snprintf(msg, sizeof(msg), "%s: EERP for the V file arrived", sc->name);
    CHECK(m.eerps == 1 && m.out[0].done == 1, msg);
}

/* An abend in the middle of a file, as try() sees it: the engine is left
** by longjmp from a callback, with the record buffer of the file still
** allocated. It must be in sess.xbuf, and oftp_sess_free() frees it.
** 'W': in the responder's write, receiving the initiator's F file;
** 'R': in the responder's read, sending its V file back.
*/
static void
abend_scenario(int where)
{
    static const SCEN sc = { "abend", 5, 5, 4096, 99, 0, "PW", 0, 0 };
    MFILES m;
    int sv[2], status = -1;
    pid_t pid;
    char msg[120];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(sv[1]);
        run_initiator(&sc, sv[0]);      /* sees the line drop */
        close(sv[0]);
        _exit(0);
    }
    close(sv[0]);

    resp_files(&m);
    m.jump = where;
    peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
              sc.rlevel, sc.sdeb, sc.cred, sc.cmpr);
    sess_init(&sess, &sv[1], &m, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    if (setjmp(abend_jb) == 0) {
        oftp_sess_run(&sess);
        snprintf(msg, sizeof(msg), "abend in %s: callback reached",
                 where == 'W' ? "write" : "read");
        CHECK(0, msg);
    } else {
        snprintf(msg, sizeof(msg), "abend in %s: record buffer in the session",
                 where == 'W' ? "write" : "read");
        CHECK(sess.xbuf != NULL, msg);
        oftp_sess_free(&sess);
        snprintf(msg, sizeof(msg), "abend in %s: freed by oftp_sess_free",
                 where == 'W' ? "write" : "read");
        CHECK(sess.xbuf == NULL, msg);
    }
    close(sv[1]);
    waitpid(pid, &status, 0);
}

/* The partner's connection is cut in the middle of a file we send: the
** monitor counts that as an attempt, and counts on three things here.
** The session ends with a reason (not 0), by our side (esid_local); the
** file is not reported done (out_done is not called: the monitor's
** cleanup of the claimed file is what records the attempt); and no file
** counts as sent (files_out), so that the monitor's wait before the next
** call applies.
*/
static void
midfile_scenario(void)
{
    MFILES m, r;
    int sv[2], status = -1, rc;
    pid_t pid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        return;
    }
    signal(SIGPIPE, SIG_IGN);
    pid = fork();
    if (pid == 0) {
        close(sv[0]);
        resp_files(&r);
        r.jump = 'C';
        peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
                  5, 4096, 99, 0);
        sess_init(&sess, &sv[1], &r, 0);
        strcpy(sess.localcode, "O0013000000MVS2");
        oftp_sess_run(&sess);
        _exit(1);                       /* the cut did not come */
    }
    close(sv[1]);

    init_files(&m);
    sess_init(&sess, &sv[0], &m, 1);
    peer_init(&sess.peer, "MVS2", "O0013000000MVS2", "O0013000000MVS1",
              5, 4096, 99, 0);
    rc = oftp_sess_run(&sess);
    close(sv[0]);
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "cut: the partner was cut in its first write");
    CHECK(rc != 0 && sess.esid_local, "cut: the session ends with a reason");
    CHECK(m.out[0].done == 0, "cut: the file is not reported done");
    CHECK(sess.files_out == 0, "cut: no file counts as sent");
    CHECK(sess.xbuf == NULL, "cut: no record buffer left");
}

/* The caller is refused by the lookup: -1 (unknown) ends the session
** with ESID 03, a reason code (8, the partner is busy) with that one.
** Both sides must see it, the initiator as the partner's.
*/
static void
refuse_scenario(int lrc, int esid)
{
    static const SCEN sc = { "refused", 5, 5, 4096, 99, 0, "PW", 0, 0 };
    MFILES m;
    int sv[2], status = -1, rc;
    pid_t pid;
    char msg[120];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        return;
    }
    pid = fork();
    if (pid == 0) {
        MFILES im;
        int fd = sv[0];

        close(sv[1]);
        init_files(&im);
        sess_init(&sess, &fd, &im, 1);
        peer_init(&sess.peer, "MVS2", "O0013000000MVS2", g_ocode,
                  sc.ilevel, sc.sdeb, sc.cred, sc.cmpr);
        rc = oftp_sess_run(&sess);
        close(sv[0]);
        _exit(rc == esid && !sess.esid_local && im.out[0].done == 0 ? 0 : 1);
    }
    close(sv[0]);

    resp_files(&m);
    peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
              sc.rlevel, sc.sdeb, sc.cred, sc.cmpr);
    sess_init(&sess, &sv[1], &m, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    lookup_rc = lrc;
    rc = oftp_sess_run(&sess);
    lookup_rc = 0;
    close(sv[1]);
    waitpid(pid, &status, 0);

    snprintf(msg, sizeof(msg), "lookup %d: responder ends with ESID %02d (rc %d)",
             lrc, esid, rc);
    CHECK(rc == esid && sess.esid_local, msg);
    snprintf(msg, sizeof(msg), "lookup %d: initiator sees the partner's ESID %02d",
             lrc, esid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, msg);
}

/* One session between an initiator (in the child) and the responder
** that keeps its files across sessions. The initiator sends the files
** in im, receives what the responder owes, and exits with the number of
** EERPs it received (after a cut: 0). The child's exit code goes to
** *crc, the responder's session result is returned.
*/
static int
pair_session(MFILES *im, MFILES *rm, int *crc)
{
    int sv[2], status = -1, rc;
    pid_t pid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        *crc = -1;
        return -1;
    }
    pid = fork();
    if (pid == 0) {
        close(sv[1]);
        sess_init(&sess, &sv[0], im, 1);
        peer_init(&sess.peer, "MVS2", "O0013000000MVS2", "O0013000000MVS1",
                  5, 4096, 99, 0);
        oftp_sess_run(&sess);
        close(sv[0]);
        _exit(im->erps_any);
    }
    close(sv[0]);
    peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
              5, 4096, 99, 0);
    sess_init(&sess, &sv[1], rm, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    rc = oftp_sess_run(&sess);
    close(sv[1]);
    waitpid(pid, &status, 0);
    *crc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return rc;
}

/* A session cut while its EERP is out and the RTR has not come (#57):
** the engine must not report the EERP as sent, so the owner of the
** record (the monitor, which holds a claim on it for the session) still
** has it to give back. The next session offers it again and, once the
** RTR has come, the one after that does not; the file was received once.
*/
static void
cut_scenario(void)
{
    MFILES im, rm;
    int crc, rc;

    memset(&rm, 0, sizeof(rm));
    memset(&im, 0, sizeof(im));
    im.nout = 1;
    im.out[0].dsn = "POEM.TXT";
    im.out[0].fmt = 'T';
    im.out[0].recs[0] = "It is an ancient Mariner,\r\n";
    im.cut = 1;
    rc = pair_session(&im, &rm, &crc);
    CHECK(rm.nin == 1 && rm.in[0].closed == 1,
          "cut at the EERP: the file arrived and was kept");
    CHECK(rc != 0, "cut at the EERP: the responder's session ends in error");
    CHECK(rm.erp_pending[0] == 1,
          "cut at the EERP: no erp_sent without the RTR, the EERP is still owed");

    memset(&im, 0, sizeof(im));
    rc = pair_session(&im, &rm, &crc);
    CHECK(rc == 0 && crc == 1,
          "next session: the owed EERP arrives once, normal end");
    CHECK(rm.erp_pending[0] == 0, "next session: erp_sent after the RTR");

    rc = pair_session(&im, &rm, &crc);
    CHECK(rc == 0 && crc == 0, "third session: no EERP again");
    CHECK(rm.nin == 1, "three sessions: the file was delivered once");
}

/* ---- trace ---------------------------------------------------------- */

static char trlog[96][160];
static int ntr;

static void
capture(void *ctx, int warn, const char *line)
{
    (void)ctx;
    (void)warn;
    if (strncmp(line, "TRACE ", 6) == 0 && ntr < 96)
        snprintf(trlog[ntr++], sizeof(trlog[0]), "%s", line);
}

static int
trace_always(void *ctx)
{
    (void)ctx;
    return 1;
}

/* a partner-only switch: off until the responder has identified it */
static int
trace_known(void *ctx)
{
    (void)ctx;
    return looked_up;
}

static int
tr_count(const char *what)
{
    int i, n = 0;

    for (i = 0; i < ntr; i++)
        if (strstr(trlog[i], what) != NULL)
            n++;
    return n;
}

static void
trace_scenario(const char *name, OFTP_TRACEFN fn, int level)
{
    SCEN sc = { "trace", 5, 5, 4096, 99, 0, "TOPSECR1", 0, 0 };
    char msg[120];

    sc.ilevel = sc.rlevel = level;
    sc.name = name;
    ntr = 0;
    looked_up = 0;
    g_log = capture;
    g_traced = fn;
    g_pw = "TOPSECR1";
    scenario(&sc);
    g_log = quiet;
    g_traced = NULL;
    g_pw = "PW";

    snprintf(msg, sizeof(msg), "%s: no password in any trace line", name);
    CHECK(tr_count("TOPSECR1") == 0, msg);
    if (fn == NULL) {
        snprintf(msg, sizeof(msg), "%s: switch off, no trace line", name);
        CHECK(ntr == 0, msg);
        return;
    }
    snprintf(msg, sizeof(msg), "%s: SSID both ways, once each (%d lines)", name, ntr);
    CHECK(tr_count("RECV SSID") == 1 && tr_count("SEND SSID") == 1, msg);
    snprintf(msg, sizeof(msg), "%s: SSID shows level and buffer", name);
    CHECK(tr_count("RECV SSID") == 1 && tr_count("level") >= 1
          && tr_count("sdeb 4096") >= 1, msg);
    snprintf(msg, sizeof(msg), "%s: both files' SFID, EFID and the EERP", name);
    CHECK(tr_count("RECV SFID") == 2 && tr_count("PAYROLL.DATA") >= 1
          && tr_count("SEND SFID") == 1 && tr_count("RECV EFID") == 2
          && tr_count("EERP") >= 1, msg);
    snprintf(msg, sizeof(msg), "%s: DATA reported as a run, not per buffer", name);
    CHECK(tr_count("RECV DATA") >= 1 && tr_count("RECV DATA") < 4
          && tr_count("buffer") >= 1, msg);
    if (fn == trace_always)
        CHECK(tr_count("SEND SSRM") == 1, "trace on from the start: SSRM seen");
    else
        CHECK(tr_count("SSRM") == 0, "partner-only trace: SSRM came before the partner was known");
}

/* ---- a peer that packs F records as mendelson OFTP2 CE does ------- */

#define MF_LREC     80
#define MF_NREC     6
#define MF_SDEB     240

static int
mf_frame(int fd, unsigned char *b, unsigned n)
{
    unsigned len = n + 4;

    b[0] = 0x10;
    b[1] = (unsigned char)((len >> 16) & 0xFF);
    b[2] = (unsigned char)((len >> 8) & 0xFF);
    b[3] = (unsigned char)(len & 0xFF);
    return x_send(&fd, b, len);
}

/* Reads one frame; returns its command code, 0 on failure */
static char
mf_read(int fd, unsigned char *b)
{
    unsigned len;

    if (x_recv(&fd, b, 4, 5) != 0)
        return 0;
    len = ((unsigned)b[1] << 16) | ((unsigned)b[2] << 8) | b[3];
    if (len < 5 || len > 4100 || x_recv(&fd, b + 4, len - 4, 5) != 0)
        return 0;
    return oftp_cmd_code(b + 4, len - 4);
}

/* The initiator's side, run in the child. Each buffer is filled with
** subrecords of 63 octets and of 17 with the end-of-record flag, the
** pattern starting afresh at the head of every buffer whatever the record
** boundary: a record cut by the end of a buffer loses its flag and the
** flags after it stand 63 octets into the next record. The EFID counts the
** flags set. 0 when the responder sent EFPA.
*/
static int
run_mendelson(int fd, const unsigned char *data, unsigned total)
{
    unsigned char b[4200];
    OFTP_SSIDCMD o;
    OFTP_SFIDCMD f;
    OFTP_EFIDCMD e;
    unsigned long eors = 0;
    unsigned pos = 0;
    int n;

    if (mf_read(fd, b) != OFTP_SSRM)
        return 1;
    memset(&o, 0, sizeof(o));
    o.level = 5;
    strcpy(o.code, "O0013000000MVS1");
    strcpy(o.pswd, g_pw);
    o.sdeb = MF_SDEB;
    o.sr = 'B';
    o.cmpr = o.rest = o.spec = o.auth = 'N';
    o.cred = 99;
    n = oftp_enc_ssid(&o, b + 4, sizeof(b) - 4);
    if (n < 0 || mf_frame(fd, b, (unsigned)n) != 0)
        return 2;
    if (mf_read(fd, b) != OFTP_SSID)
        return 3;

    memset(&f, 0, sizeof(f));
    strcpy(f.dsn, "MENDEL.F80");
    strcpy(f.date, "20261020");
    strcpy(f.time, "1200000001");
    strcpy(f.dest, "O0013000000MVS2");
    strcpy(f.orig, "O0013000000MVS1");
    f.fmt = 'F';
    f.lrecl = MF_LREC;
    f.fsiz = f.osiz = 1;
    f.sign = 'N';
    n = oftp_enc_sfid(5, &f, b + 4, MF_SDEB);
    if (n < 0 || mf_frame(fd, b, (unsigned)n) != 0)
        return 4;
    if (mf_read(fd, b) != OFTP_SFPA)
        return 5;

    while (pos < total) {
        unsigned used = 1;
        int tail = 0;

        b[4] = OFTP_DATA;
        while (pos < total) {
            unsigned k = tail ? MF_LREC - OFTP_SR_MAX : OFTP_SR_MAX;
            unsigned char hdr;

            if (k > total - pos)
                k = total - pos;
            if (used + 1 + k > MF_SDEB)
                break;
            hdr = (unsigned char)k;
            if (tail || pos + k == total) {
                hdr |= OFTP_SR_EOR;
                eors++;
            }
            b[4 + used] = hdr;
            memcpy(b + 4 + used + 1, data + pos, k);
            used += 1 + k;
            pos += k;
            tail = !tail;
        }
        if (mf_frame(fd, b, used) != 0)
            return 6;
    }
    e.rcnt = eors;
    e.ucnt = total;
    n = oftp_enc_efid(5, &e, b + 4, MF_SDEB);
    if (n < 0 || mf_frame(fd, b, (unsigned)n) != 0)
        return 7;
    return mf_read(fd, b) == OFTP_EFPA ? 0 : 8;
}

/* An F file from such a peer arrives whole, in records of its LRECL */
static void
mendelson_scenario(void)
{
    unsigned char data[MF_NREC * MF_LREC];
    char want[MF_NREC * (MF_LREC + 1) + 1];
    MFILES m;
    int sv[2], status = -1, rc, i;
    unsigned w = 0;
    pid_t pid;
    char msg[120];

    for (i = 0; i < MF_NREC; i++) {
        memset(data + i * MF_LREC, 'A' + i, MF_LREC);
        data[i * MF_LREC] = (unsigned char)('0' + i);
        memcpy(want + w, data + i * MF_LREC, MF_LREC);
        w += MF_LREC;
        want[w++] = '|';
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        CHECK(0, "socketpair");
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(sv[1]);
        rc = run_mendelson(sv[0], data, sizeof(data));
        close(sv[0]);
        _exit(rc);
    }
    close(sv[0]);

    memset(&m, 0, sizeof(m));
    peer_init(&resp_view, "MVS1", "O0013000000MVS1", "O0013000000MVS2",
              5, MF_SDEB, 99, 0);
    sess_init(&sess, &sv[1], &m, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    rc = oftp_sess_run(&sess);
    close(sv[1]);
    waitpid(pid, &status, 0);

    snprintf(msg, sizeof(msg), "mendelson F: the peer got EFPA (child rc %d)",
             WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, msg);
    CHECK(m.nin == 1 && m.in[0].closed == 1, "mendelson F: file kept");
    CHECK(m.nin == 1 && m.in[0].len == w
          && memcmp(m.in[0].data, want, w) == 0,
          "mendelson F: records cut at the LRECL, payload intact");
    (void)rc;
}

/* The trace goes off in the middle of the first file's DATA run (#86):
** the buffers counted before the switch must not be logged at the end of
** the session, after the operator was told TRACE OFF.
*/
static int tr_calls, tr_limit, tr_pend;

static int
trace_limited(void *ctx)
{
    (void)ctx;
    if (++tr_calls == tr_limit + 1)
        tr_pend = (int)(sess.tr_nbuf[0] + sess.tr_nbuf[1]);
    return tr_calls <= tr_limit;
}

static void
switchoff_scenario(void)
{
    SCEN sc = { "switch off", 5, 5, 4096, 99, 0, "PW", 0, 0 };

    ntr = 0;
    tr_calls = tr_pend = 0;
    tr_limit = 6;               /* SSRM, SSID, SSID, SFID, SFPA, first DATA */
    g_log = capture;
    g_traced = trace_limited;
    scenario(&sc);
    g_log = quiet;
    g_traced = NULL;

    CHECK(tr_calls > tr_limit, "switch off: the switch was asked after it went off");
    CHECK(tr_count("RECV SFID") == 1, "switch off: the trace was on at the start");
    CHECK(tr_pend > 0, "switch off: DATA was counted before the switch");
    CHECK(tr_count("buffers") == 0, "switch off: no DATA line after the switch");
    CHECK(sess.tr_nbuf[0] == 0 && sess.tr_nbuf[1] == 0,
          "switch off: the counters are dropped");
}

/* Bytes of the network in the log (#86) */
static char alog[24][160];
static int nal;

static void
capture_all(void *ctx, int warn, const char *line)
{
    (void)ctx;
    (void)warn;
    if (nal < 24)
        snprintf(alog[nal++], sizeof(alog[0]), "%s", line);
}

static int
al_dirty(void)
{
    int i;
    const unsigned char *p;

    for (i = 0; i < nal; i++)
        for (p = (const unsigned char *)alog[i]; *p; p++)
            if (*p < ' ' || *p > '~')
                return 1;
    return 0;
}

static int
al_count(const char *what)
{
    int i, n = 0;

    for (i = 0; i < nal; i++)
        if (strstr(alog[i], what) != NULL)
            n++;
    return n;
}

static void
printable_scenario(void)
{
    SCEN sc = { "file refused", 5, 5, 4096, 99, 0, "PW", 13, 0 };

    /* an unknown caller and a refused one, with control characters in
    ** the ID; the trace of the SSID shows it as well */
    g_log = capture_all;
    g_traced = trace_always;
    g_ocode = "O00130\x01\x1b[2JMVS1";
    nal = 0;
    refuse_scenario(-1, 3);
    CHECK(al_count("unknown caller O00130..[2JMVS1") == 1, "ID with controls: unknown caller logged");
    CHECK(al_count("RECV SSID") == 1, "ID with controls: SSID traced");
    CHECK(!al_dirty(), "ID with controls: unknown caller, no control byte in the log");
    nal = 0;
    refuse_scenario(8, 8);
    CHECK(al_count("caller O00130..[2JMVS1") == 1 && al_count("refused, reason 08") == 1,
          "ID with controls: refused caller logged");
    CHECK(!al_dirty(), "ID with controls: refused caller, no control byte in the log");
    g_ocode = "O0013000000MVS1";

    /* a file name with controls, traced and refused */
    g_dsn = "PAY\x01ROLL\x1b[31m\x7f\xe9.X";
    nal = 0;
    scenario(&sc);
    g_dsn = NULL;
    CHECK(al_count("RECV SFID 165: dsn PAY.ROLL") == 1
          && al_count("PAY.ROLL.[31m...X refused") == 1,
          "file name with controls: traced and refused, both lines show it");
    CHECK(!al_dirty(), "file name with controls: no control byte in the log");
    g_log = quiet;
    g_traced = NULL;
}

/* A partner without PASSWORD expects an empty password and sends one: the
** check is an exact comparison in both directions of a call, and an empty
** password is no wildcard (the books say so: ref-cfg, "A Partner Without a
** Password")
*/
static void
empty_pw_scenarios(void)
{
    static const SCEN both = { "empty both ways", 5, 5, 4096, 99, 0, "", 0, 0 };
    static const SCEN sent = { "empty expected, one sent", 5, 5, 4096, 99, 0,
                               "XX", 0, 0 };
    static const SCEN none = { "one expected, empty sent", 5, 5, 4096, 99, 0,
                               "", 0, 0 };
    static const SCEN ans1 = { "empty expected, answer has one", 5, 5, 4096, 99,
                               0, "", 0, 0 };
    static const SCEN ans2 = { "one expected, answer empty", 5, 5, 4096, 99,
                               0, "PW", 0, 0 };

    g_pw = "";
    scenario(&both);
    scenario(&sent);
    g_rpw = "XX";
    scenario(&ans1);
    g_pw = "PW";
    g_rpw = "";
    scenario(&ans2);
    g_rpw = NULL;
    scenario(&none);
    g_pw = "PW";
}

int
main(void)
{
    static const SCEN sc[] = {
        { "2.0 both",          5, 5, 4096, 99, 0, "PW", 0, 0 },
        { "2.0 to 1.4",        5, 4, 4096, 99, 0, "PW", 0, 0 },
        { "2.0 to 1.3",        5, 2, 4096, 99, 0, "PW", 0, 0 },
        { "1.4 both",          4, 5, 4096, 99, 0, "PW", 0, 0 },
        { "small buffers",     5, 5,  165,  1, 0, "PW", 0, 0 },
        { "minimum 1.4",       4, 4,  128,  1, 0, "PW", 0, 0 },
        { "compressed",        5, 5,  200,  2, 1, "PW", 0, 0 },
        { "compressed 1.3",    2, 2,  256,  3, 1, "PW", 0, 0 },
        { "wrong password",    5, 5, 4096, 99, 0, "XX", 0, 0 },
        { "file refused",      5, 5, 4096, 99, 0, "PW", 13, 0 },
        { "NERP 2.0",          5, 5, 4096, 99, 0, "PW", 0, 99 },
        { "NERP 1.4",          4, 4, 4096, 99, 0, "PW", 0, 12 },
        { "NERP to 1.3",       5, 2, 4096, 99, 0, "PW", 0, 99 },
    };
    unsigned i;

    printf("=== TSTSESS session engine ===\n");
    for (i = 0; i < sizeof(sc) / sizeof(sc[0]); i++)
        scenario(&sc[i]);
    empty_pw_scenarios();
    abend_scenario('W');
    abend_scenario('R');
    midfile_scenario();
    refuse_scenario(-1, 3);
    refuse_scenario(8, 8);
    cut_scenario();
    trace_scenario("trace off", NULL, 5);
    trace_scenario("trace on 2.0", trace_always, 5);
    trace_scenario("trace on 1.4", trace_always, 4);
    trace_scenario("trace on 1.3", trace_always, 2);
    trace_scenario("trace partner", trace_known, 5);
    mendelson_scenario();
    switchoff_scenario();
    printable_scenario();
    return mbt_test_summary("TSTSESS");
}
