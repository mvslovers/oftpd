/* TSTLAT - how long a dialog request takes over the SVC (MVS)
**
** Times, with the TOD clock, the requests the ISPF dialog and the OFTP
** utility make through oftp_cli_request(): STATUS, LIST, SHOW and SEND
** (held, so nothing is called), and DELETE to clean up. Wall clock, not
** CPU time: what a user waits for. Each request is repeated and the
** minimum, median and maximum are printed, together with every sample,
** so a number can be re-derived from the output.
**
** The spool is grown in steps (PARM max), because LIST asks OFTPD for
** the answer again for every piece and skips the rows already given
** (oftp#cli.c), so its cost depends on how many rows there are.
**
** Needs a RUNNING OFTPD with SVC=, a partner in OFTPPTN and a data set
** the job may read. PARM='RUN,partner,dsn[,max[,label]]'. Not
** APF-authorized, as TSTSVC: the dialog is not either. Measures only;
** it fails when the clock cannot be trusted or a request is refused,
** never because a request was slow. Slow ones are counted and printed.
**
** The clock is checked before anything is measured: STCK must advance
** and a usleep() of known length must come out about that long.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/subsys.h>

#include "oftpcli.h"
#include "oftpsvc.h"

#define NSAMP   256
#define SLOW_US 1000000u                /* the issue's limit: one second */

/* TOD clock in microseconds, modulo 2**32 (71 minutes): differences of
** two readings of one request are what is used. Bit 51 of the TOD is
** one microsecond, so the 64-bit value shifted right by 12.
*/
static unsigned
now_us(void)
{
    unsigned t[2] __attribute__((aligned(8)));

    __asm__ __volatile__("         STCK  0(%0)\n"
                         :
                         : "r"(t)
                         : "memory", "cc");
    return (t[0] << 20) | (t[1] >> 12);
}

static unsigned samp[NSAMP];
static int      nsamp;
static int      slow;
static unsigned lastus;

static void
sort_samp(void)
{
    int i, j;

    for (i = 1; i < nsamp; i++) {
        unsigned v = samp[i];

        for (j = i - 1; j >= 0 && samp[j] > v; j--)
            samp[j + 1] = samp[j];
        samp[j + 1] = v;
    }
}

/* "123.4": microseconds as milliseconds, no floating point (libc370's
** printf has none to spare)
*/
static const char *
ms(unsigned us)
{
    static char buf[4][16];
    static int  k;
    char       *b = buf[k++ & 3];

    snprintf(b, 16, "%u.%u", us / 1000, (us % 1000) / 100);
    return b;
}

/* min / median / max in ms, then the sorted samples in ms */
static void
report(const char *what, int showall)
{
    int i;

    if (nsamp == 0)
        return;
    sort_samp();
    printf("  %-26s n=%3d  min %7s  median %7s  max %7s ms",
           what, nsamp, ms(samp[0]), ms(samp[nsamp / 2]),
           ms(samp[nsamp - 1]));
    if (samp[nsamp - 1] > SLOW_US)
        printf("  ** OVER 1 S **");
    printf("\n");
    if (showall) {
        printf("    samples (ms):");
        for (i = 0; i < nsamp; i++)
            printf("%s%s", i % 10 == 0 ? "\n     " : " ", ms(samp[i]));
        printf("\n");
    }
}

static void
start(void)
{
    nsamp = 0;
}

static void
add(unsigned us)
{
    if (nsamp < NSAMP)
        samp[nsamp++] = us;
    if (us > SLOW_US)
        slow++;
}

static int rows;

static void
count_row(void *ctx, const char *text)
{
    (void)ctx;
    (void)text;
    rows++;
}

/* One request through the path the dialog takes, timed. Returns the
** return code; the final line goes to fin. The time (us) is in lastus.
*/
static int
timed(const char *req, char *fin, int cap)
{
    unsigned t0, t1;
    int rc;

    rows = 0;
    t0 = now_us();
    rc = oftp_cli_request(req, count_row, NULL, fin, cap, NULL);
    t1 = now_us();
    lastus = t1 - t0;
    return rc;
}

/* How many pieces a request comes in, and the slowest of them */
static void
pieces(const char *req, int *np, unsigned *worst)
{
    char     buf[OFTP_SVC_REPMAX];
    unsigned len, next = 0, was, t0;
    int      rc, n = 0;

    *worst = 0;
    do {
        was = next;
        t0 = now_us();
        rc = oftp_svc_call(req, buf, sizeof(buf), next, &len, &next);
        t0 = now_us() - t0;
        n++;
        if (t0 > *worst)
            *worst = t0;
    } while (rc == OFTP_SVC_MORE && next > was && n < 1000);
    *np = n;
}

#define MAXKEY 512
static char keys[MAXKEY][9];
static int  nkeys;

int
main(int argc, char **argv)
{
    char     parm[200], *partner, *dsn, *label = "";
    char     fin[200], req[256];
    int      max = 200, steps[] = { 0, 10, 50, 100, 200, 400 };
    int      s, i, rc, np, delfail = 0;
    unsigned worst, w0, w1, a, b;
    time_t   tw0, tw1;
    char    *p;

    printf("=== TSTLAT request time over the SVC ===\n");
    if (argc < 2 || strncmp(argv[1], "RUN", 3) != 0) {
        printf("  SKIP: needs a running OFTPD with SVC= and"
               " PARM='RUN,partner,dsn' (test/mvs/tstlat.jcl)\n");
        return 0;
    }
    snprintf(parm, sizeof(parm), "%s", argv[1]);
    strtok(parm, ",");
    partner = strtok(NULL, ",");
    dsn     = strtok(NULL, ",");
    if ((p = strtok(NULL, ",")) != NULL) {
        max = atoi(p);
        if ((p = strtok(NULL, ",")) != NULL)
            label = p;
    }
    if (partner == NULL || dsn == NULL || max < 1 || max > MAXKEY) {
        printf("  PARM is RUN,partner,dsn[,max[,label]], max 1..%d\n", MAXKEY);
        return 8;
    }
    printf("  label: %s   partner %s   dsn %s   up to %d held sends\n",
           label, partner, dsn, max);

    CHECK_EQ(__isauth(), 0, "the client job is NOT APF-authorized");
    if (__isauth())
        return mbt_test_summary("TSTLAT");
    CHECK(ssct_find(OFTP_SVC_SSNAME) != NULL,
          "subsystem " OFTP_SVC_SSNAME " is installed");
    if (ssct_find(OFTP_SVC_SSNAME) == NULL)
        return mbt_test_summary("TSTLAT");

    /* The clock first: a number from a clock that is wrong is the
    ** failure that looks like a result.
    */
    a = now_us();
    b = now_us();
    CHECK(b - a < 1000000u, "STCK reads twice within a second");
    start();
    for (i = 0; i < 5; i++) {
        w0 = now_us();
        usleep(200000);
        add(now_us() - w0);
    }
    sort_samp();
    printf("  clock check: usleep(200000) took %s .. %s ms\n",
           ms(samp[0]), ms(samp[nsamp - 1]));
    CHECK(samp[0] >= 190000u && samp[nsamp - 1] <= 400000u,
          "usleep(200 ms) reads as about 200 ms on the TOD clock");
    slow = 0;

    tw0 = time(NULL);
    w0  = now_us();

    /* STATUS: the floor, the transport and a handler that reads nothing */
    start();
    for (i = 0; i < 30; i++) {
        rc = timed("STATUS", fin, sizeof(fin));
        if (rc != 0)
            break;
        add(lastus);
    }
    CHECK_EQ(rc, 0, "STATUS answers 0");
    report("STATUS", 0);

    /* LIST at the size the spool has now, whatever else it holds */
    rc = timed("LIST", fin, sizeof(fin));
    CHECK_EQ(rc, 0, "LIST answers 0");
    printf("  spool at the start: %d transfer(s) visible to this user\n",
           rows);

    for (s = 0; s < (int)(sizeof(steps) / sizeof(steps[0])); s++) {
        int target = steps[s] > max ? max : steps[s];
        char what[40];
        int  queued = 0;

        /* queue held sends up to the step, each timed */
        if (nkeys < target)
            start();
        while (nkeys < target) {
            snprintf(req, sizeof(req),
                     "SEND PARTNER=%s DSN=%s VFN=LAT.N%04d HOLD=Y", partner,
                     dsn, nkeys);
            rc = timed(req, fin, sizeof(fin));
            if (rc != 0) {
                printf("    SEND refused: rc %d: %s\n", rc, fin);
                break;
            }
            add(lastus);
            snprintf(keys[nkeys], sizeof(keys[0]), "%.8s", fin);
            nkeys++;
            queued++;
        }
        if (queued > 0 && nkeys == target) {
            snprintf(what, sizeof(what), "SEND (%d..%d)",
                     nkeys - nsamp, nkeys - 1);
            report(what, 0);
        }
        if (nkeys < target)
            break;

        /* LIST: the whole request, then its pieces */
        start();
        for (i = 0; i < 10; i++) {
            rc = timed("LIST", fin, sizeof(fin));
            if (rc != 0)
                break;
            add(lastus);
        }
        CHECK_EQ(rc, 0, "LIST answers 0");
        snprintf(what, sizeof(what), "LIST %d rows", rows);
        report(what, 1);
        pieces("LIST", &np, &worst);
        printf("    LIST comes in %d piece(s), slowest %s ms\n", np,
               ms(worst));
        CHECK(target == 0 || rows >= target, "LIST shows every held send");

        start();
        for (i = 0; i < 10; i++) {
            rc = timed("LIST MINE", fin, sizeof(fin));
            if (rc != 0)
                break;
            add(lastus);
        }
        CHECK_EQ(rc, 0, "LIST MINE answers 0");
        snprintf(what, sizeof(what), "LIST MINE %d rows", rows);
        report(what, 0);

        /* SHOW: by key, the first and the last of ours */
        if (nkeys > 0) {
            for (i = 0; i < 2; i++) {
                const char *k = keys[i == 0 ? 0 : nkeys - 1];

                start();
                for (a = 0; a < 10; a++) {
                    snprintf(req, sizeof(req), "SHOW KEY=%s", k);
                    rc = timed(req, fin, sizeof(fin));
                    if (rc != 0)
                        break;
                    add(lastus);
                }
                CHECK_EQ(rc, 0, "SHOW answers 0");
                snprintf(what, sizeof(what), "SHOW %s (%s)", k,
                         i == 0 ? "first" : "last");
                report(what, 0);
            }
        }
    }

    /* Clean up what was queued; timed, as one more request */
    start();
    for (i = 0; i < nkeys; i++) {
        snprintf(req, sizeof(req), "DELETE KEY=%s", keys[i]);
        rc = timed(req, fin, sizeof(fin));
        if (rc != 0) {
            if (delfail++ < 3)
                printf("    DELETE %s: rc %d: %s\n", keys[i], rc, fin);
            continue;
        }
        add(lastus);
    }
    CHECK_EQ(delfail, 0, "every held send is deleted again");
    report("DELETE", 0);
    rc = timed("LIST", fin, sizeof(fin));
    printf("  spool at the end: %d transfer(s) visible to this user\n", rows);

    w1  = now_us();
    tw1 = time(NULL);
    printf("  elapsed: %u s by the TOD clock, %ld s by time()\n",
           (w1 - w0) / 1000000u, (long)(tw1 - tw0));
    CHECK(((w1 - w0) / 1000000u) + 2 >= (unsigned)(tw1 - tw0)
          && (unsigned)(tw1 - tw0) + 2 >= ((w1 - w0) / 1000000u),
          "the TOD clock and time() agree on how long the run took");
    printf("  requests over one second: %d\n", slow);
    return mbt_test_summary("TSTLAT");
}
