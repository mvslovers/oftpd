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
    (sizeof(OFTP_SLOT)   == 2368
  && sizeof(OFTP_ANCHOR) == 64 + OFTP_SVC_NSLOT * 2368) ? 1 : -1];

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
    ** the request is dropped and the slot's userid put in its place,
    ** so a request naming somebody else must not be answered as that
    ** somebody -- there is nothing in the STATUS reply to read it off,
    ** so this only checks that the substitution does not break the
    ** request.
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

    /* 6. An unknown request is answered, not dropped: the transport
    ** has to carry a refusal as reliably as an answer.
    */
    memset(buf, 0, sizeof(buf));
    len = 0;
    rc = oftp_svc_call("NOSUCHREQUEST", buf, sizeof(buf), 0, &len, NULL);
    CHECK_EQ(rc, OFTP_SVC_OK, "an unknown request comes back answered");
    CHECK(has_line(buf, len, "500 "), "and the answer is the 500");

    return mbt_test_summary("TSTSVC");
}
