/* TSTSVC - the cross-memory transport, end to end (MVS)
**
** Runs an unauthorized, problem-state, key-8 program against a
** running OFTPD: find the subsystem, issue the private SVC, get the
** answer back. That is the whole point of the transport, and it is
** the one property no host test can reach.
**
** **Needs a running OFTPD with SVC= set in OFTPPRM**, which is why it
** is gated on PARM='RUN' (test/mvs/tstsvc.jcl). The gate is the
** operator saying the stand is ready. Once it is given, a transport
** that cannot be found is a FAILURE and not a skip: a test that
** quietly passes because the thing it tests was absent is worth less
** than no test, and this suite has the gate precisely so that case
** does not need to be guessed at.
*/
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/subsys.h>

#include "oftpsvc.h"

/* The sizes the assembler's EQUs were written against. The field
** offsets cannot be checked here -- libc370's offsetof() is not an
** integer constant expression and cc370 has no __builtin_offsetof --
** so they are checked at run time in mirror_ok() (src/mvs/oftp#svc.c)
** before anything is installed. The sizes are the half that CAN be
** caught at compile time, and this puts that catch in a second place
** on purpose: oftp#svc.c only builds into OFTPD.
*/
typedef char tstsvc_sizes[
    (sizeof(OFTP_SLOT)   == 2400
  && sizeof(OFTP_ANCHOR) == 64 + OFTP_SVC_NSLOT * 2400) ? 1 : -1];

/* The reply of a piece, as lines */
static int
count_lines(const char *buf, unsigned len, char first)
{
    unsigned i;
    int      n = 0, start = 1;

    for (i = 0; i < len; i++) {
        if (start && buf[i] == first)
            n++;
        start = (buf[i] == '\n');
    }
    return n;
}

static int
has_line(const char *buf, unsigned len, const char *want)
{
    unsigned n = (unsigned)strlen(want);
    unsigned i;

    if (len < n)
        return 0;
    for (i = 0; i + n <= len; i++)
        if ((i == 0 || buf[i - 1] == '\n') && memcmp(buf + i, want, n) == 0)
            return 1;
    return 0;
}

/* Issue the SVC with a hand-built block, past oftp_svc_call(): the
** client refuses a request over REQMAX itself, so only a caller that
** does not go through it can reach the routine's own length check.
** Same sequence as oftp#svcc.c, with labels of its own (this module
** and the client link into one program). R0 goes in non-zero, so a 0
** coming out is the routine's doing and not a leftover.
*/
static void __attribute__((noinline))
raw_svc(OFTP_SVCREQ *req, unsigned svcn, unsigned *out)
{
    __asm__ __volatile__(
        "         LR    1,%1\n"
        "         LR    6,%2\n"
        "         LA    0,1\n"
        "         EX    6,TSTSCS0\n"
        "         B     TSTSCSX\n"
        "TSTSCS0  SVC   0\n"
        "TSTSCSX  ST    15,0(,%0)\n"
        "         ST    0,4(,%0)\n"
        "         ST    1,8(,%0)\n"
        :
        : "r"(out), "r"(req), "r"(svcn)
        : "0", "1", "6", "15", "memory");
}

/* The pool as the anchor shows it: every slot FREE and nobody inside
** the routine. A key-8 fetch of subpool 241, which #15 measured legal.
*/
static int
pool_idle(const OFTP_ANCHOR *a)
{
    unsigned i;

    if (a->inflight != 0)
        return 0;
    for (i = 0; i < OFTP_SVC_NSLOT; i++)
        if (a->slot[i].state != OFTP_SL_FREE)
            return 0;
    return 1;
}

int
main(int argc, char **argv)
{
    char     buf[OFTP_SVC_REPMAX];
    char     small[OFTP_SVC_REPMIN];
    unsigned len, next;
    int      rc, rounds, rows;

    printf("=== TSTSVC cross-memory transport ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs a running OFTPD with SVC= and PARM='RUN'"
               " (test/mvs/tstsvc.jcl)\n");
        return 0;
    }

    /* 0. **This job must NOT be authorized**, and until now nothing
    ** here said so. The whole claim of the transport is that an
    ** ordinary program reaches OFTPD without any authority of its
    ** own; run from an APF library, every assertion below would still
    ** pass and would have demonstrated nothing. So an authorized run
    ** is a FAILURE of the test, not a convenience -- and the failure
    ** text has to say that the fix is to move the STEPLIB, never to
    ** authorize it.
    */
    CHECK_EQ(__isauth(), 0, "the client job is NOT APF-authorized");
    if (__isauth()) {
        printf("    (this STEPLIB is authorized, so this run proves"
               " nothing about an unauthorized caller -- move the job to"
               " a library that is not in IEAAPF00; do NOT authorize it)\n");
        return mbt_test_summary("TSTSVC");
    }

    /* 1. Discovery. A failure here says OFTPD is not running, or is
    ** running without SVC= -- not that the transport is broken, so
    ** the message has to say which.
    */
    CHECK(ssct_find(OFTP_SVC_SSNAME) != NULL,
          "subsystem " OFTP_SVC_SSNAME " is installed");
    if (ssct_find(OFTP_SVC_SSNAME) == NULL) {
        printf("    (no %s: start OFTPD with SVC=nnn in OFTPPRM)\n",
               OFTP_SVC_SSNAME);
        return mbt_test_summary("TSTSVC");
    }

    /* 2. The client refuses a buffer below the floor without issuing
    ** the SVC at all -- the routine would answer BADREQ anyway, and
    ** this proves the client does not need it to.
    */
    len = 99;
    rc = oftp_svc_call("STATUS", buf, OFTP_SVC_REPMIN - 1, 0, &len, &next);
    CHECK_EQ(rc, OFTP_SVC_BADREQ, "a reply buffer under the floor is refused");
    CHECK_EQ((int)len, 0, "and nothing is reported written");

    /* 2b. A request longer than the routine takes is refused, not cut:
    ** what was left of it would be another request.
    */
    {
        char longreq[OFTP_SVC_REQMAX + 2];

        memset(longreq, 'X', sizeof(longreq) - 1);
        longreq[sizeof(longreq) - 1] = '\0';
        len = 99;
        rc = oftp_svc_call(longreq, buf, sizeof(buf), 0, &len, &next);
        CHECK_EQ(rc, OFTP_SVC_BADREQ, "a request over REQMAX is refused");
        CHECK_EQ((int)len, 0, "and nothing is reported written");
    }

    /* 2c. **The routine's own length check**, which step 2b cannot
    ** reach: the client turns the request away before the SVC. Here the
    ** block is built by hand and the SVC issued directly, with a
    ** declared length the routine must refuse (over REQMAX, zero,
    ** negative). The refusal comes after the claim, so what it has to
    ** prove is the release: rc BADREQ, nothing reported written, no
    ** abend, every slot FREE and nothing in flight afterwards. Twice
    ** the pool's worth of calls, as in step 7, so one slot or one count
    ** left behind per call fills the pool; STATUS after them must still
    ** be answered by OFTPD.
    */
    {
        static const unsigned bad[] = {
            OFTP_SVC_REQMAX + 1, OFTP_SVC_REQMAX + 2, 1000, 0x7FFFFFFFU,
            0x80000000U, 0xFFFFFFFFU, 0
        };
        SSCT        *ssct = ssct_find(OFTP_SVC_SSNAME);
        OFTP_ANCHOR *anc  = (OFTP_ANCHOR *)ssct->ssctsuse;
        OFTP_SVCREQ  blk;
        unsigned     out[3], k, refused = 0, wrote = 0, nbad;

        nbad = (unsigned)(sizeof(bad) / sizeof(bad[0]));
        CHECK(pool_idle(anc), "before: every slot FREE, none in flight");
        for (k = 0; k < 2 * OFTP_SVC_NSLOT + nbad; k++) {
            memset(&blk, 0, sizeof(blk));
            memcpy(blk.eye, "OFTQ", 4);
            blk.ver    = OFTP_SVC_VER;
            blk.reply  = buf;
            blk.cap    = sizeof(buf);
            blk.reqlen = (k < nbad) ? bad[k] : OFTP_SVC_REQMAX + 1;
            memset(blk.req, 'S', OFTP_SVC_REQMAX);
            out[0] = out[1] = out[2] = 0;
            raw_svc(&blk, anc->svc, out);
            if (out[0] == OFTP_SVC_BADREQ)
                refused++;
            if (out[1] != 0)
                wrote++;
        }
        CHECK_EQ((int)refused, (int)(2 * OFTP_SVC_NSLOT + nbad),
                 "a declared length the routine cannot take is refused,"
                 " every time");
        CHECK_EQ((int)wrote, 0, "and the routine reports nothing written");
        CHECK(pool_idle(anc), "after: every slot FREE, none in flight");
        memset(buf, 0, sizeof(buf));
        len = 0;
        rc = oftp_svc_call("STATUS", buf, sizeof(buf), 0, &len, NULL);
        CHECK_EQ(rc, OFTP_SVC_OK, "STATUS still answers after them: OFTPD unharmed");
        CHECK(has_line(buf, len, "200 status"), "and the answer is complete");
    }

    /* 3. STATUS, the round trip this test exists for: an unauthorized
    ** program in its own address space gets OFTPD's answer back.
    */
    memset(buf, 0, sizeof(buf));
    len = next = 0;
    rc = oftp_svc_call("STATUS", buf, sizeof(buf), 0, &len, &next);
    CHECK_EQ(rc, OFTP_SVC_OK, "STATUS answers OK");
    CHECK(len > 0 && len <= sizeof(buf), "and the reply has a sane length");
    if (rc == OFTP_SVC_OK && len > 0) {
        CHECK(has_line(buf, len, "100 "), "it carries at least one 1xx row");
        CHECK(has_line(buf, len, "200 status"), "it ends with 200 status");
        CHECK_EQ(buf[len - 1], '\n', "every line is terminated");
        printf("    %u bytes, %d rows\n", len, count_lines(buf, len, '1'));
    }

    /* 4. The userid is OFTPD's to decide, not the caller's. OWNER= in
    ** the request is dropped and the slot's userid goes to the
    ** handlers beside the line, so a request naming somebody else must
    ** not be answered as that somebody -- there is nothing in the
    ** STATUS reply to read it off, so this only checks that dropping
    ** it does not break the request. What a second owner sees is
    ** measured with OFTP itself (internals/xmem.md section 13).
    */
    memset(buf, 0, sizeof(buf));
    len = 0;
    rc = oftp_svc_call("STATUS OWNER=NOBODY", buf, sizeof(buf), 0, &len, NULL);
    CHECK_EQ(rc, OFTP_SVC_OK, "a request carrying OWNER= is still answered");

    /* 5. Paging. LIST into the smallest buffer the transport takes:
    ** either it fits (OK) or it comes in pieces (MORE), and in the
    ** second case the resume index has to move and the sequence has
    ** to end. A piece that returns the same index twice would loop
    ** for ever, which is the failure worth testing for.
    */
    rounds = 0;
    rows   = 0;
    next   = 0;
    do {
        unsigned was = next;

        memset(small, 0, sizeof(small));
        len = 0;
        rc = oftp_svc_call("LIST", small, sizeof(small), next, &len, &next);
        if (rc != OFTP_SVC_OK && rc != OFTP_SVC_MORE)
            break;
        rows += count_lines(small, len, '1');
        if (rc == OFTP_SVC_MORE)
            CHECK(next > was, "a further piece moves the resume index on");
        if (rc == OFTP_SVC_MORE && next <= was)
            break;                      /* would not terminate */
    } while (rc == OFTP_SVC_MORE && ++rounds < 40);

    CHECK(rc == OFTP_SVC_OK || rc == OFTP_SVC_MORE,
          "LIST answers OK or MORE");
    CHECK_EQ(rc, OFTP_SVC_OK, "and the last piece of LIST is a complete one");
    CHECK(rounds < 40, "LIST terminates in fewer than 40 pieces");
    printf("    LIST: %d rows in %d piece(s) of %u bytes\n",
           rows, rounds + 1, (unsigned)sizeof(small));

    /* 5b. **The paging path, forced.** Step 5 exercises it only when
    ** the spool has enough rows, and on a fresh system it has none --
    ** which is how a green run can leave the most intricate code
    ** here untouched. STATUS is a fixed five 1xx rows and ~300 bytes,
    ** so into the smallest buffer the transport takes it MUST come in
    ** pieces. The rows then have to add up to exactly what the
    ** single-shot answer gave: a resume index that is off by one
    ** loses a row or repeats one, and both look like a working
    ** answer.
    */
    rounds = 0;
    rows   = 0;
    next   = 0;
    do {
        unsigned was = next;

        memset(small, 0, sizeof(small));
        len = 0;
        rc = oftp_svc_call("STATUS", small, sizeof(small), next, &len, &next);
        if (rc != OFTP_SVC_OK && rc != OFTP_SVC_MORE)
            break;
        rows += count_lines(small, len, '1');
        if (rc == OFTP_SVC_MORE && next <= was)
            break;
    } while (rc == OFTP_SVC_MORE && ++rounds < 20);

    CHECK_EQ(rc, OFTP_SVC_OK, "STATUS in pieces ends with a complete one");
    CHECK(rounds >= 1, "STATUS into a 256-byte buffer really did page");
    CHECK_EQ(rows, 5, "the pieces carry every row exactly once");
    printf("    STATUS paged: %d rows in %d piece(s)\n", rows, rounds + 1);

    /* 6. An unknown request is answered, not dropped: the transport
    ** has to carry a refusal as reliably as an answer.
    */
    memset(buf, 0, sizeof(buf));
    len = 0;
    rc = oftp_svc_call("NOSUCHREQUEST", buf, sizeof(buf), 0, &len, NULL);
    CHECK_EQ(rc, OFTP_SVC_OK, "an unknown request comes back answered");
    CHECK(has_line(buf, len, "500 "), "and the answer is the 500");

    /* 7. A reply buffer this job may not store into is the routine's
    ** to refuse (TPRNG, xmem.md section 6), with a return code and not
    ** a program check -- and refusing it must leak nothing. Low core
    ** is key 0, so a key-8 store there fails TPROT. Twice the pool's
    ** worth of such calls: a slot or an in-flight count left behind by
    ** each would have filled the pool by the fifth, and the STATUS
    ** after them would answer BUSY. In flight is read from the anchor,
    ** a key-8 fetch of subpool 241 that #15 measured legal; nothing
    ** else is calling, so it has to be back at 0 (#34).
    */
    {
        char    *lowcore = (char *)0x200;
        SSCT    *ssct    = ssct_find(OFTP_SVC_SSNAME);
        unsigned k, refused = 0, wrote = 0;

        for (k = 0; k < 2 * OFTP_SVC_NSLOT; k++) {
            len = 99;
            rc = oftp_svc_call("STATUS", lowcore, OFTP_SVC_REPMIN, 0, &len,
                               NULL);
            if (rc == OFTP_SVC_BADREQ)
                refused++;
            if (len != 0)
                wrote++;
        }
        CHECK_EQ((int)refused, 2 * OFTP_SVC_NSLOT,
                 "a reply buffer in key-0 storage is refused, every time");
        CHECK_EQ((int)wrote, 0, "and nothing is reported written");
        memset(buf, 0, sizeof(buf));
        len = 0;
        rc = oftp_svc_call("STATUS", buf, sizeof(buf), 0, &len, NULL);
        CHECK_EQ(rc, OFTP_SVC_OK, "STATUS still answers after them: no slot lost");
        CHECK(ssct != NULL
              && ((OFTP_ANCHOR *)ssct->ssctsuse)->inflight == 0,
              "and no call is left in flight");
    }

    /* 8. **What the routine leaves in the slots** (#121). A request can
    ** carry a password and a reply holds whatever the handler said; the
    ** slots are CSA, readable from key 8. Every exit of the routine --
    ** the normal one and the refusals after the claim -- wipes request
    ** and reply before it frees the slot, so after calls that used the
    ** pool more than once over, no slot holds a byte of either. The
    ** anchor's count of answers must have moved, so the check cannot
    ** pass on slots that were never used.
    */
    {
        SSCT        *ssct = ssct_find(OFTP_SVC_SSNAME);
        OFTP_ANCHOR *anc  = (OFTP_ANCHOR *)ssct->ssctsuse;
        unsigned     i, k, dirty = 0, served = anc->served;

        for (k = 0; k < 2 * OFTP_SVC_NSLOT; k++) {
            memset(buf, 0, sizeof(buf));
            len = 0;
            rc = oftp_svc_call("NOSUCHREQUEST PASSWORD=SECRET8", buf,
                               sizeof(buf), 0, &len, NULL);
            if (rc != OFTP_SVC_OK)
                break;
        }
        memset(buf, 0, sizeof(buf));
        len = 0;
        rc = oftp_svc_call("STATUS", buf, sizeof(buf), 0, &len, NULL);
        CHECK_EQ(rc, OFTP_SVC_OK, "STATUS answers once more");
        CHECK(anc->served > served, "and the pool was used");
        CHECK(pool_idle(anc), "every slot is FREE again");
        for (i = 0; i < OFTP_SVC_NSLOT; i++) {
            unsigned j;

            for (j = 0; j < sizeof(anc->slot[i].req); j++)
                if (anc->slot[i].req[j] != 0)
                    dirty++;
            for (j = 0; j < sizeof(anc->slot[i].reply); j++)
                if (anc->slot[i].reply[j] != 0)
                    dirty++;
        }
        CHECK_EQ((int)dirty, 0, "and none holds a byte of a request or a reply");
    }

    return mbt_test_summary("TSTSVC");
}
