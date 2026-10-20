/* TSTKEEP - the daily KEEPDAYS run, seen on MVS (#59)
**
** Two steps around a start of OFTPD (test/mvs/tstkeep.jcl). OFTPD owns
** the spool, so it must be stopped while this program has DD OFTPSPL.
**
**   PARM='SEED'        adds transfers whose last change lies in the past
**                      by writing their times; the ages are chosen for
**                      KEEPDAYS=7, the default
**   PARM='CHECK'       reads the spool back and expects the old finished
**                      ones gone, everything else still there
**
** Between the two OFTPD is started with KEEPDAYS=7 and its first tick
** is the daily run: nothing here calls the housekeeping, and the age is
** written into the records, not simulated in OFTPD.
*/
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <mbtcheck.h>

#include "oftpspl.h"

#define DAY 86400L

/* dsn, state, age in days (the KEEPDAYS= days is 7), whether it must
** survive the run */
static const struct {
    const char *dsn;
    char        state;
    long        age;
    int         kept;
} seeds[] = {
    { "KEEP.YOUNG.CONFIRMED",    SPL_CONFIRMED,  1, 1 },
    { "KEEP.EDGE.DELIVERED",     SPL_DELIVERED,  6, 1 },
    { "KEEP.OLD.HELD",           SPL_HELD,      30, 1 },
    { "KEEP.OLD.SENT",           SPL_SENT,      30, 1 },
    { "KEEP.OLD.QUEUED",         SPL_QUEUED,    30, 1 },
    { "GONE.OLD.CONFIRMED",      SPL_CONFIRMED, 30, 0 },
    { "GONE.OLD.DELIVERED",      SPL_DELIVERED, 10, 0 },
    { "GONE.OLD.REJECTED",       SPL_REJECTED,  20, 0 },
    { "GONE.OLD.FAILED",         SPL_FAILED,    60, 0 },
};
#define NSEED ((int)(sizeof(seeds) / sizeof(seeds[0])))

static int
seed(OFTP_SPOOL *sp)
{
    long now = (long)time(NULL);
    int i, rc;

    for (i = 0; i < NSEED; i++) {
        OFTP_SPREC r;
        long t = now - seeds[i].age * DAY;

        memset(&r, 0, sizeof(r));
        r.dir = SPL_SEND;
        r.state = seeds[i].state;
        strcpy(r.partner, "KEEPTEST");
        strcpy(r.dsn, seeds[i].dsn);
        r.tqueued = t;
        r.tlast = t;
        r.tdone = t;
        rc = oftp_spl_add(sp, &r);
        CHECK_EQ(rc, 0, "add a transfer with an old last change");
        printf("    %.8s %-22s state %c, last change %ld days ago\n",
               r.key, seeds[i].dsn, r.state, seeds[i].age);
    }
    return 0;
}

static int
check(OFTP_SPOOL *sp)
{
    int i, n, seen[NSEED];
    OFTP_SPREC g;
    char last[8];

    memset(seen, 0, sizeof(seen));
    last[0] = '\0';
    n = 0;
    while (oftp_spl_next(sp, last, &g) == 0) {
        memcpy(last, g.key, 8);
        if (strcmp(g.partner, "KEEPTEST") != 0)
            continue;
        for (i = 0; i < NSEED; i++)
            if (strcmp(g.dsn, seeds[i].dsn) == 0)
                seen[i]++;
        n++;
    }
    for (i = 0; i < NSEED; i++) {
        printf("    %-22s %s\n", seeds[i].dsn, seen[i] ? "in the spool" : "gone");
        CHECK_EQ(seen[i], seeds[i].kept ? 1 : 0,
                 seeds[i].kept ? "kept" : "deleted by the daily run");
    }
    return n;
}

int
main(int argc, char **argv)
{
    OFTP_SPOOL *sp;
    char why[80];

    printf("=== TSTKEEP the daily KEEPDAYS run ===\n");
    if (argc < 2 || (strcmp(argv[1], "SEED") != 0 && strcmp(argv[1], "CHECK") != 0)) {
        printf("  SKIP: needs DD OFTPSPL, OFTPD stopped and PARM='SEED' or"
               " 'CHECK' (test/mvs/tstkeep.jcl)\n");
        return 0;
    }
    sp = oftp_spl_open("OFTPSPL", why, sizeof(why));
    CHECK(sp != NULL, "open the spool");
    if (sp == NULL) {
        printf("    (%s)\n", why);
        return mbt_test_summary("TSTKEEP");
    }
    if (strcmp(argv[1], "SEED") == 0)
        seed(sp);
    else
        check(sp);
    oftp_spl_close(sp);
    return mbt_test_summary("TSTKEEP");
}
