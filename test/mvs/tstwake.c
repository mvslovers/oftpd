/* TSTWAKE - a caller that wakes on a slot that is no longer its own (MVS)
**
** The sweep frees a DONE slot whose caller has not come back (a
** stalled, swapped-out address space), and another caller can claim it
** before the first is dispatched again. The first then wakes on a slot
** that belongs to someone else, and OFTPSVC must leave: return DOWN,
** store nothing into the slot, decrement nothing (#124). Before the
** fix it freed the slot, took the in-flight count down a second time
** and wiped the other caller's request, or collected the other
** caller's reply.
**
** This test is its own OFTPD: oftp_svc_start() installs the real
** OFTPSVC under an SVC number, a real caller parks in it through that
** SVC, and a subtask plays the sweep and the other claimant at the
** moment the caller sits in its WAIT, then posts the caller's ECB. Three
** slot states are tried: FREE (swept, not yet reclaimed), PENDING
** (reclaimed, request staged) and DONE (reclaimed and answered); a
** fourth round, where nobody took the slot, shows that a caller still
** holding its own slot is answered as before.
**
** Needs an APF-authorized STEPLIB holding TSTWAKE (AC(1)) and OFTPSVC,
** and a free SVC number: PARM='RUN' looks for one from 255 down,
** PARM='RUN nnn' takes that one.
** No OFTPD may be running under the same number. Gated on PARM='RUN'
** (test/mvs/tstwake.jcl).
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/ecb.h>
#include <mvs/storage.h>
#include <mvs/thread.h>
#include <mvs/xmem.h>

#include "oftpmon.h"
#include "oftpsvc.h"

static int quiet;

void
oftp_mon_msg(const char *fmt, ...)      // NOSONAR: must match the prototype
{
    va_list ap;

    if (quiet)
        return;
    va_start(ap, fmt);
    printf("    ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

#define MODE_FREE     1
#define MODE_PENDING  2
#define MODE_DONE     3
#define MODE_OWN      4     /* answered normally: the control round */

#define OTHER_REQ     "OTHER CALLER REQUEST"
#define OTHER_REPLY   "100 OTHER CALLER REPLY\n"
#define OTHER_SHIFT   5                 /* the other claim number is ours + 5 */

typedef struct stage {
    OFTP_MON    *m;
    volatile int mode;          /* what the next wake-up does          */
    volatile int quit;
    volatile int acted;         /* wake-ups that found PENDING         */
    volatile unsigned claim;    /* the caller's claim number           */
} STAGE;

/* The subtask: waits for the caller to park, then does to its slot what
** the sweep and a second caller would have done, and posts the first
** caller's ECB as OFTPD's DONE would have.
*/
static int
stage_thread(void *arg1, void *arg2)
{
    STAGE      *g = arg1;
    OFTP_MON   *m = g->m;
    OFTP_SLOT  *s = &m->anchor->slot[0];

    (void)arg2;
    for (;;) {
        unsigned char savekey;
        void         *ascb;

        ecb_wait(&m->svcwake);
        if (g->quit)
            break;
        m->svcwake = 0;
        if (__super(PSWKEY0, &savekey) != 0)
            continue;
        if (s->state != OFTP_SL_PENDING) {
            __prob(savekey, NULL);
            continue;
        }
        g->acted++;
        ascb     = s->ascb;
        g->claim = s->claim;
        if (g->mode == MODE_FREE) {
            memset(s->req, 0, sizeof(s->req));
            s->state    = OFTP_SL_FREE;
            m->anchor->inflight = 0;
        } else if (g->mode == MODE_PENDING) {
            s->claim = g->claim + OTHER_SHIFT;
            memset(s->req, 0, sizeof(s->req));
            memcpy(s->req, OTHER_REQ, sizeof(OTHER_REQ));
            s->reqlen   = sizeof(OTHER_REQ) - 1;
            m->anchor->inflight = 1;
        } else if (g->mode == MODE_OWN) {
            memset(s->reply, 0, sizeof(s->reply));
            memcpy(s->reply, OTHER_REPLY, sizeof(OTHER_REPLY));
            s->replen   = sizeof(OTHER_REPLY) - 1;
            s->rc       = OFTP_SVC_OK;
            s->next     = 0;
            s->state    = OFTP_SL_DONE;
        } else {
            s->claim = g->claim + OTHER_SHIFT;
            memset(s->reply, 0, sizeof(s->reply));
            memcpy(s->reply, OTHER_REPLY, sizeof(OTHER_REPLY));
            s->replen   = sizeof(OTHER_REPLY) - 1;
            s->rc       = OFTP_SVC_OK;
            s->next     = 0;
            s->state    = OFTP_SL_DONE;
            m->anchor->inflight = 1;
        }
        __xmpost(ascb, (void *)&s->recb, 0);
        __prob(savekey, NULL);
    }
    return 0;
}

/* The SVC with a hand-built block. R0 goes in non-zero, so a 0 coming
** out is the routine's doing. */
static void __attribute__((noinline))
raw_svc(OFTP_SVCREQ *req, unsigned svcn, unsigned *out)
{
    __asm__ __volatile__(
        "         LR    1,%1\n"
        "         LR    6,%2\n"
        "         LA    0,1\n"
        "         EX    6,TSTWKS0\n"
        "         B     TSTWKSX\n"
        "TSTWKS0  SVC   0\n"
        "TSTWKSX  ST    15,0(,%0)\n"
        "         ST    0,4(,%0)\n"
        "         ST    1,8(,%0)\n"
        :
        : "r"(out), "r"(req), "r"(svcn)
        : "0", "1", "6", "15", "memory");
}

typedef struct snap {
    unsigned state, inflight, claim, replen;
    char     req[32];
    char     reply[32];
} SNAP;

static void
snap_of(OFTP_ANCHOR *a, SNAP *n)
{
    unsigned char savekey;
    OFTP_SLOT    *s = &a->slot[0];

    memset(n, 0, sizeof(*n));
    if (__super(PSWKEY0, &savekey) != 0)
        return;
    n->state    = s->state;
    n->inflight = a->inflight;
    n->claim    = s->claim;
    n->replen   = s->replen;
    memcpy(n->req, s->req, sizeof(n->req) - 1);
    memcpy(n->reply, s->reply, sizeof(n->reply) - 1);
    __prob(savekey, NULL);
}

static void
idle(OFTP_ANCHOR *a)
{
    unsigned char savekey;
    OFTP_SLOT    *s = &a->slot[0];

    if (__super(PSWKEY0, &savekey) != 0)
        return;
    memset(s->req, 0, sizeof(s->req));
    memset(s->reply, 0, sizeof(s->reply));
    s->state    = OFTP_SL_FREE;
    s->replen   = 0;
    a->inflight = 0;
    __prob(savekey, NULL);
}

/* One caller parks, the slot is changed under it, it wakes. */
static void
round_of(STAGE *g, int mode, const char *what)
{
    OFTP_ANCHOR *a = g->m->anchor;
    OFTP_SVCREQ  blk;
    SNAP         now;
    char         buf[OFTP_SVC_REPMAX];
    unsigned     out[3];
    int          acted = g->acted;

    printf("  -- %s\n", what);
    idle(a);
    memset(buf, 'B', sizeof(buf));
    memset(&blk, 0, sizeof(blk));
    memcpy(blk.eye, "OFTQ", 4);
    blk.ver    = OFTP_SVC_VER;
    blk.reply  = buf;
    blk.cap    = sizeof(buf);
    blk.reqlen = 6;
    memcpy(blk.req, "STATUS", 6);
    out[0] = out[1] = out[2] = 99;
    g->mode = mode;
    raw_svc(&blk, a->svc, out);
    snap_of(a, &now);

    CHECK_EQ(g->acted, acted + 1, "the caller parked and the slot was changed");
    if (mode == MODE_OWN) {
        CHECK_EQ((int)out[0], OFTP_SVC_OK, "the caller whose slot it is gets its reply");
        CHECK_EQ((int)out[1], (int)(sizeof(OTHER_REPLY) - 1), "with its length");
        CHECK(strncmp(buf, OTHER_REPLY, sizeof(OTHER_REPLY) - 1) == 0, "and its text");
        CHECK_EQ((int)now.state, OFTP_SL_FREE, "the slot is released");
        CHECK_EQ((int)now.inflight, 0, "and counted down once");
        return;
    }
    CHECK_EQ((int)out[0], OFTP_SVC_DOWN, "the caller is told: not reached");
    CHECK_EQ((int)out[1], 0, "and nothing is reported written");
    CHECK(memcmp(buf, "BBBB", 4) == 0 && buf[sizeof(buf) - 1] == 'B',
          "its reply buffer is untouched");
    if (mode == MODE_FREE) {
        CHECK_EQ((int)now.state, OFTP_SL_FREE, "the slot stays FREE");
        CHECK_EQ((int)now.inflight, 0, "and the in-flight count is not taken below zero");
    } else if (mode == MODE_PENDING) {
        CHECK_EQ((int)now.state, OFTP_SL_PENDING, "the other caller's slot is still PENDING");
        CHECK_EQ((int)now.inflight, 1, "its in-flight count is still there");
        CHECK(strcmp(now.req, OTHER_REQ) == 0, "and its request is not wiped");
        CHECK_EQ((int)now.claim, (int)(g->claim + OTHER_SHIFT), "and its claim is its own");
    } else {
        CHECK_EQ((int)now.state, OFTP_SL_DONE, "the other caller's reply is not collected");
        CHECK_EQ((int)now.inflight, 1, "its in-flight count is still there");
        CHECK(strcmp(now.reply, OTHER_REPLY) == 0, "and its reply is not wiped");
    }
}

int
main(int argc, char **argv)
{
    STAGE       g;
    OFTP_MON   *m;
    CTHDTASK   *t;
    unsigned    svc = 0;
    unsigned char savekey;

    printf("=== TSTWAKE a caller waking on a slot that is not its own ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("SKIP: PARM='RUN' from an APF-authorized library only\n");
        return 0;
    }
    if (argc > 2)
        svc = (unsigned)atoi(argv[2]);
    CHECK(__isauth() != 0, "the library is APF-authorized");
    if (__isauth() == 0)
        return mbt_test_summary("TSTWAKE");

    m = calloc(1, sizeof(*m));
    CHECK(m != NULL, "storage");
    if (m == NULL)
        return mbt_test_summary("TSTWAKE");
    /* With no number given, the first free one from 255 down: a stand
    ** has products on a good many of 200-255.
    */
    if (argc > 2) {
        m->cfg.svc = (int)svc;
        CHECK_EQ(oftp_svc_start(m), 0, "OFTPSVC is installed under the SVC given");
    } else {
        quiet = 1;
        for (svc = 255; svc >= 200; svc--) {
            m->cfg.svc = (int)svc;
            if (oftp_svc_start(m) == 0)
                break;
        }
        quiet = 0;
        CHECK(m->anchor != NULL, "OFTPSVC is installed under a free SVC");
        if (m->anchor != NULL)
            printf("    SVC %u\n", svc);
    }
    if (m->anchor == NULL) {
        printf("    (no free SVC %u, or OFTPSVC not in the STEPLIB)\n", svc);
        free(m);
        return mbt_test_summary("TSTWAKE");
    }

    memset(&g, 0, sizeof(g));
    g.m = m;
    t = cthread_create_ex(stage_thread, &g, NULL, 32 * 1024);
    CHECK(t != NULL, "the stand-in subtask starts");
    if (t != NULL && __super(PSWKEY0, &savekey) == 0) {
        m->anchor->nslot = 1;
        m->anchor->flags |= OFTP_ANC_ACTIVE;
        __prob(savekey, NULL);

        round_of(&g, MODE_FREE, "slot swept, nobody has claimed it yet");
        round_of(&g, MODE_PENDING, "slot swept and claimed by another caller");
        round_of(&g, MODE_DONE, "slot swept, claimed and answered");
        round_of(&g, MODE_OWN, "control: the slot is still ours and is answered");

        g.quit = 1;
        ecb_post(&m->svcwake, 0);
        ecb_wait(&t->termecb);
        cthread_delete(&t);
    }
    idle(m->anchor);
    oftp_svc_stop(m);
    CHECK_EQ(m->svc_taken, 0, "the SVC entry is given back");
    free(m);
    return mbt_test_summary("TSTWAKE");
}
