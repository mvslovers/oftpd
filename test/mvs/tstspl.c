/* TSTSPL - the spool: thread, control record, add/get/put/next/del (MVS)
**
** Needs DD OFTPSPL: a KSDS KEYS(8 0) RECORDSIZE(512 512), never loaded
** (test/mvs/tstspl.jcl). Without PARM='RUN' it reports itself skipped.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpspl.h"

#ifdef OFTP_DEBUG_ABEND
/* ---- the spool thread's recovery (#49) -----------------------------
** Needs a build with -DOFTP_DEBUG_ABEND: a lookup of key TABEND00 then
** ends in an S0C4 inside the spool thread. The region's free storage in
** total is measured as the monitor's former debug display did: the
** largest piece left is taken again and again, then all are given back.
** A bare GETMAIN RC: libc370's getmain() writes a WTO on each failure.
*/
static void *
raw_get(unsigned lv)
{
    unsigned rc;
    void *a;

    __asm__ __volatile__("GETMAIN RC,LV=(%2),SP=0\n\t"
                         "LR\t%0,15\n\t"
                         "LR\t%1,1"
                         : "=r"(rc), "=r"(a)
                         : "r"(lv)
                         : "0", "1", "14", "15", "memory");
    return rc == 0 ? a : NULL;
}

static void
raw_free(void *a, unsigned lv)
{
    __asm__ __volatile__("FREEMAIN RU,LV=(%0),A=(%1),SP=0"
                         :
                         : "r"(lv), "r"(a)
                         : "0", "1", "14", "15", "memory");
}

typedef struct piece {
    struct piece *next;
    unsigned      len;
} PIECE;

static unsigned
largest_piece(void)
{
    unsigned lo = 0;
    unsigned hi = 16u * 1024 * 1024;

    while (hi - lo > 8) {
        unsigned mid = (lo + (hi - lo) / 2) & ~7u;
        void *q = raw_get(mid);

        if (q != NULL) {
            raw_free(q, mid);
            lo = mid;
        } else
            hi = mid;
    }
    return lo;
}

static unsigned
total_free(void)
{
    PIECE *head = NULL;
    PIECE *p;
    unsigned sum = 0;
    unsigned n = 0;

    while (n < 4096) {
        unsigned lo = largest_piece();

        if (lo < 64 || (p = raw_get(lo)) == NULL)
            break;
        p->next = head;
        p->len = lo;
        head = p;
        sum += lo;
        n++;
    }
    while (head != NULL) {
        p = head;
        head = p->next;
        raw_free(p, p->len);
    }
    return sum;
}

static void
abend_recovery(OFTP_SPOOL *sp)
{
    OFTP_SPREC g;
    unsigned before, after, held;
    void *hold;
    int i, bad = 0;

    CHECK_EQ(oftp_spl_get(sp, "TABEND00", &g), OFTP_SPL_ERROR,
             "the request that abends is answered with an error");
    CHECK_EQ(oftp_spl_get(sp, "T0000002", &g), 0,
             "and the next request is served");
    CHECK_EQ(oftp_spl_abends(sp), 1, "one abend counted");

    before = total_free();
    for (i = 0; i < 50; i++) {
        if (oftp_spl_get(sp, "TABEND00", &g) != OFTP_SPL_ERROR)
            bad++;
        if (oftp_spl_get(sp, "T0000002", &g) != 0)
            bad++;
    }
    after = total_free();
    CHECK_EQ(bad, 0, "50 more abends, each answered, each followed by a good get");
    CHECK_EQ(oftp_spl_abends(sp), 51, "51 abends counted");
    printf("    region free storage: %u after the first abend, %u after 51\n",
           before, after);
    CHECK(before == after, "the same free storage after 51 abends as after 1");

    /* the measure itself: 100 KB held must show */
    hold = malloc(100u * 1024);
    held = total_free();
    free(hold);
    CHECK(hold != NULL && held < after && after - held >= 100u * 1024,
          "calibration: 100 KB held lowers the total by at least 100 KB");
    printf("    held 100 KB: %u free (%u less)\n", held, after - held);

    CHECK_EQ(oftp_spl_put(sp, &g), 0, "an update after the abends");
    CHECK_EQ(oftp_spl_del(sp, "T0000099"), OFTP_SPL_NOTFOUND,
             "a miss is still NOTFOUND, not an error");
}
#endif

int
main(int argc, char **argv)
{
    OFTP_SPOOL *sp;
    OFTP_SPREC r, g;
    char why[80], keys[3][9], last[8];
    int i, n, rc;

    printf("=== TSTSPL spool ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs DD OFTPSPL and PARM='RUN' (test/mvs/tstspl.jcl)\n");
        return 0;
    }

    sp = oftp_spl_open("OFTPSPL", why, sizeof(why));
    CHECK(sp != NULL, "open a never-loaded spool");
    if (sp == NULL) {
        printf("    (%s)\n", why);
        return mbt_test_summary("TSTSPL");
    }
    for (i = 0; i < 3; i++) {
        memset(&r, 0, sizeof(r));
        r.dir = SPL_SEND;
        r.state = SPL_QUEUED;
        sprintf(r.partner, "P%d", i);
        sprintf(r.dsn, "FILE%d", i);
        rc = oftp_spl_add(sp, &r);
        CHECK_EQ(rc, 0, "add a transfer");
        memcpy(keys[i], r.key, 8);
        keys[i][8] = '\0';
    }
    CHECK(strcmp(keys[0], "T0000001") == 0 && strcmp(keys[2], "T0000003") == 0,
          "keys T0000001.. from the control record");

    CHECK_EQ(oftp_spl_get(sp, keys[1], &g), 0, "get by key");
    CHECK(strcmp(g.dsn, "FILE1") == 0 && g.state == SPL_QUEUED, "fields back");
    g.state = SPL_SENT;
    strcpy(g.text, "EFPA received");
    CHECK_EQ(oftp_spl_put(sp, &g), 0, "update");
    memset(&g, 0, sizeof(g));
    oftp_spl_get(sp, keys[1], &g);
    CHECK(g.state == SPL_SENT && strcmp(g.text, "EFPA received") == 0, "update visible");

    CHECK_EQ(oftp_spl_del(sp, keys[0]), 0, "delete");
    CHECK_EQ(oftp_spl_get(sp, keys[0], &g), OFTP_SPL_NOTFOUND, "deleted: not found");
    CHECK_EQ(oftp_spl_del(sp, keys[0]), OFTP_SPL_NOTFOUND, "delete again: not found");

    n = 0;
    last[0] = '\0';
    while (oftp_spl_next(sp, last, &g) == 0) {
        memcpy(last, g.key, 8);
        n++;
    }
    CHECK_EQ(n, 2, "next walks the two left");
    oftp_spl_close(sp);

    /* the number goes on after a reopen */
    sp = oftp_spl_open("OFTPSPL", why, sizeof(why));
    CHECK(sp != NULL, "reopen");
    if (sp != NULL) {
        unsigned cnt = 99;
        static const char st[6] = { SPL_DELIVERED, SPL_CONFIRMED, SPL_HELD,
                                    SPL_REJECTED, SPL_SENT, SPL_FAILED };
        static const long tm[6] = { 100, 100, 100, 100, 100, 5000 };

        memset(&r, 0, sizeof(r));
        r.dir = SPL_RECV;
        CHECK(oftp_spl_add(sp, &r) == 0 && memcmp(r.key, "T0000004", 8) == 0,
              "the next key is T0000004");

        /* housekeeping: T0000002 (SENT) and T0000003 (QUEUED) stay,
        ** T0000004 (state 0, a receive just added) stays; of six more,
        ** the finished ones older than 1000 go, HELD, SENT and the
        ** young FAILED one stay
        */
        for (i = 0; i < 6; i++) {
            memset(&r, 0, sizeof(r));
            r.dir = SPL_SEND;
            r.state = st[i];
            r.tqueued = tm[i];
            oftp_spl_add(sp, &r);
        }
        CHECK_EQ(oftp_spl_purge(sp, 1000, &cnt), 0, "purge");
        CHECK_EQ(cnt, 3, "three finished and old: DELIVERED, CONFIRMED, REJECTED");
        n = 0;
        last[0] = '\0';
        while (oftp_spl_next(sp, last, &g) == 0) {
            memcpy(last, g.key, 8);
            CHECK(!oftp_spl_purgeable(&g, 1000), "nothing purgeable left");
            n++;
        }
        CHECK_EQ(n, 6, "six left: T2, T3, T4, HELD, SENT, the young FAILED");
        CHECK_EQ(oftp_spl_purge(sp, 1000, &cnt), 0, "purge again");
        CHECK_EQ(cnt, 0, "and nothing more goes");
        memset(&r, 0, sizeof(r));
        CHECK(oftp_spl_add(sp, &r) == 0 && memcmp(r.key, "T0000011", 8) == 0,
              "numbering goes on after a purge: T0000011");

        /* a conditional put: only while the state is the one read */
        CHECK_EQ(oftp_spl_get(sp, "T0000002", &g), 0, "get T0000002 (SENT)");
        g.state = SPL_HELD;
        CHECK_EQ(oftp_spl_putif(sp, &g, SPL_QUEUED), OFTP_SPL_CHANGED,
                 "putif against the wrong state: CHANGED");
        oftp_spl_get(sp, "T0000002", &r);
        CHECK(r.state == SPL_SENT, "and the record is untouched");
        CHECK_EQ(oftp_spl_putif(sp, &g, SPL_SENT), 0,
                 "putif against the state read: written");
        oftp_spl_get(sp, "T0000002", &r);
        CHECK(r.state == SPL_HELD, "and the change is there");
        memcpy(g.key, "T0000099", 8);
        CHECK_EQ(oftp_spl_putif(sp, &g, SPL_SENT), OFTP_SPL_NOTFOUND,
                 "putif of a key not there: NOTFOUND");
#ifdef OFTP_DEBUG_ABEND
        abend_recovery(sp);
        oftp_spl_close(sp);
        printf("  the spool thread ended and closed after the abends\n");
#else
        printf("  SKIP: the abend recovery needs -DOFTP_DEBUG_ABEND\n");
        oftp_spl_close(sp);
#endif
    }
    return mbt_test_summary("TSTSPL");
}
