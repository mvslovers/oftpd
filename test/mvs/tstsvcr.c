/* TSTSVCR - the reply buffer freed while its caller is parked (MVS)
**
** The routine tests the caller's reply buffer with TPROT on entry and
** copies the answer into it after the WAIT (xmem.md section 6), so for
** the whole time a request is parked the buffer can change under it.
** Here a subtask of the caller frees the buffer while OFTPD holds the
** request, and the answer is then copied out into storage that is no
** longer there. What this measures: what the caller sees, what the slot
** and the in-flight count look like afterwards, and that the pool is
** whole once the sweep has run.
**
** Needs a running OFTPD **built with -DOFTP_DEBUG_SLOW** (the request
** carries SLOW=n, which holds the slot thread n seconds). No shipped
** OFTPD has that, so the gate is PARM='SLOW': the operator saying this
** one has. Given the gate, a call that returns at once is a FAILURE (it
** is not a SLOW build), not a skip.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/ecb.h>
#include <mvs/recovery.h>
#include <mvs/storage.h>
#include <mvs/subsys.h>
#include <mvs/thread.h>

#include "oftpsvc.h"

#define HOLD_SEC    3                   /* SLOW=n: the request is parked */
#define FREE_AFTER  20                  /* poll 0.2 s, up to 20 s        */
#define ROUNDS      (2 * OFTP_SVC_NSLOT)
#define SWEEP_SEC   30                  /* the sweep needs ~2-3 seconds  */

typedef struct ctx {
    ECB          ready;                 /* buffer is there               */
    ECB          freed;                 /* buffer has been given back    */
    OFTP_ANCHOR *anchor;
    char        *buf;
    int          pending_seen;          /* a slot was PENDING when freed */
    int          freemain_rc;
} CTX;

static void
nap(unsigned hundredths)
{
    ECB e = 0;

    ecb_timed_wait(&e, hundredths, 0);
}

/* The subtask: owns the buffer, gives it back once a request is parked */
static int
owner(void *arg1, void *arg2)
{
    CTX *x = arg1;
    int  i, k;

    (void)arg2;
    x->buf = getmain(OFTP_SVC_REPMAX, 0);
    if (x->buf != NULL)
        memset(x->buf, '.', OFTP_SVC_REPMAX);
    ecb_post(&x->ready, 1);
    if (x->buf == NULL)
        return 1;
    for (i = 0; i < FREE_AFTER * 5 && !x->pending_seen; i++) {
        nap(20);
        for (k = 0; k < OFTP_SVC_NSLOT; k++)
            if (x->anchor->slot[k].state == OFTP_SL_PENDING)
                x->pending_seen = 1;
    }
    x->freemain_rc = freemain(x->buf);
    ecb_post(&x->freed, 1);
    return 0;
}

typedef struct call {
    char     *buf;
    int       rc;
    unsigned  len;
} CALL;

static int
do_call(CALL *c)
{
    c->rc = oftp_svc_call("LIST SLOW=3", c->buf, OFTP_SVC_REPMAX, 0, &c->len,
                          NULL);
    return 0;
}

int
main(int argc, char **argv)
{
    CTX          x;
    CALL         c;
    CTHDTASK    *t;
    SSCT        *ssct;
    OFTP_ANCHOR *anchor;
    char         sbuf[OFTP_SVC_REPMAX];
    unsigned     len;
    int          round, rc, ab, abends = 0, answers = 0, quick = 0;
    time_t       t0;

    printf("=== TSTSVCR reply buffer freed while parked ===\n");
    if (argc < 2 || strcmp(argv[1], "SLOW") != 0) {
        printf("  SKIP: needs an OFTPD built with -DOFTP_DEBUG_SLOW and"
               " PARM='SLOW' (test/mvs/tstsvcr.jcl)\n");
        return 0;
    }
    CHECK_EQ(__isauth(), 0, "the client job is NOT APF-authorized");
    if (__isauth())
        return mbt_test_summary("TSTSVCR");

    ssct = ssct_find(OFTP_SVC_SSNAME);
    CHECK(ssct != NULL, "subsystem " OFTP_SVC_SSNAME " is installed");
    if (ssct == NULL)
        return mbt_test_summary("TSTSVCR");
    anchor = (OFTP_ANCHOR *)ssct->ssctsuse;
    CHECK_EQ((int)anchor->inflight, 0, "nothing is in flight to begin with");

    for (round = 0; round < ROUNDS; round++) {
        memset(&x, 0, sizeof(x));
        x.anchor = anchor;
        t = cthread_create_ex(owner, &x, NULL, 32 * 1024);
        CHECK(t != NULL, "the owner subtask starts");
        if (t == NULL)
            break;
        ecb_wait(&x.ready);
        CHECK(x.buf != NULL, "it got the reply buffer");
        if (x.buf == NULL)
            break;

        c.buf = x.buf;
        c.rc  = -99;
        c.len = 0;
        t0 = time(NULL);
        ab = try(do_call, &c);
        ecb_wait(&x.freed);
        if (ab != 0)
            abends++;
        else if (c.rc == OFTP_SVC_OK)
            answers++;
        /* The two outcomes the routine can have: the copy-out lands in
        ** the freed block, which stays addressable in the caller's own
        ** region (measured: that is what happens), or it program-checks
        ** with S0C4 in the caller's own task.
        */
        CHECK(ab == 0 || ab == 0x4000, "the call ends cleanly or with S0C4");
        if (ab == 0) {
            CHECK_EQ(c.rc, OFTP_SVC_OK, "the answer came back OK");
            CHECK(c.len > 0 && x.buf[0] != '.',
                  "and it was copied into the freed block");
        }
        if (time(NULL) - t0 < HOLD_SEC - 1)
            quick++;
        printf("    round %d: %lds, try %08X, rc %d, len %u, freemain %d,"
               " parked seen %d\n", round, (long)(time(NULL) - t0),
               (unsigned)ab, c.rc, c.len, x.freemain_rc, x.pending_seen);
        CHECK_EQ(x.pending_seen, 1, "the buffer was freed while parked");
        CHECK_EQ(x.freemain_rc, 0, "FREEMAIN of it succeeded");
        while (!(t->termecb & ECB_POSTED_BIT))
            nap(10);
        cthread_delete(&t);
    }
    CHECK_EQ(quick, 0, "every call was held (this OFTPD is a SLOW build)");
    printf("    %d round(s): %d abended, %d answered\n", ROUNDS, abends,
           answers);

    /* the pool is whole again once the sweep has run */
    for (round = 0; round < SWEEP_SEC * 2 && anchor->inflight != 0; round++)
        nap(50);
    CHECK_EQ((int)anchor->inflight, 0, "no call is left in flight");
    memset(sbuf, 0, sizeof(sbuf));
    len = 0;
    rc = oftp_svc_call("STATUS", sbuf, sizeof(sbuf), 0, &len, NULL);
    CHECK_EQ(rc, OFTP_SVC_OK, "STATUS still answers after them");
    return mbt_test_summary("TSTSVCR");
}
