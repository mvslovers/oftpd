/* TSTSESS - two session engines against each other (host only)
**
** The initiator runs in a child process, the responder in the parent,
** joined by a socket pair. Files live in memory. Each scenario checks
** what both sides saw; the child reports through its exit code.
*/
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mbtcheck.h>

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
} MFILES;

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

    (void)ctx;
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

    (void)ctx;
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

static OFTP_PEER resp_view;     /* how the responder sees the initiator */

static int
lookup(void *ctx, const char *code, OFTP_PEER *peer)
{
    (void)ctx;
    if (strcmp(code, resp_view.code) != 0)
        return -1;
    *peer = resp_view;
    return 0;
}

static void
quiet(void *ctx, const char *line)
{
    (void)ctx;
    (void)line;
}

static void
peer_init(OFTP_PEER *p, const char *name, const char *code,
          const char *ourcode, int level, unsigned sdeb, unsigned cred,
          int cmpr)
{
    memset(p, 0, sizeof(*p));
    strcpy(p->name, name);
    strcpy(p->code, code);
    strcpy(p->ourcode, ourcode);
    strcpy(p->pswd_in, "PW");
    strcpy(p->pswd_out, "PW");
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
    m->out[0].dsn = "PAYROLL.DATA";
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
    s->log = quiet;
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

    if (strcmp(sc->ipw, "PW") != 0)
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
    sess_init(&sess, &sv[1], &m, 0);
    strcpy(sess.localcode, "O0013000000MVS2");
    rc = oftp_sess_run(&sess);
    close(sv[1]);
    waitpid(pid, &status, 0);

    snprintf(msg, sizeof(msg), "%s: initiator side (child rc %d)", sc->name,
             WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, msg);

    if (strcmp(sc->ipw, "PW") != 0) {
        snprintf(msg, sizeof(msg), "%s: responder ends with ESID 04", sc->name);
        CHECK(rc == 4 && sess.esid_local, msg);
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
    return mbt_test_summary("TSTSESS");
}
