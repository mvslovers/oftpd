/* TSTSPL - the spool: thread, control record, add/get/put/next/del (MVS)
**
** Needs DD OFTPSPL: a KSDS KEYS(8 0) RECORDSIZE(512 512), never loaded
** (test/mvs/tstspl.jcl). Without PARM='RUN' it reports itself skipped.
*/
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpspl.h"

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

        /* partners kept by command: 'P' keys, apart from the transfers */
        {
            OFTP_SPPTN pp, qq;
            int before = 0, after = 0;

            last[0] = '\0';
            while (oftp_spl_next(sp, last, &g) == 0) {
                memcpy(last, g.key, 8);
                before++;
            }
            memset(last, 0, 8);
            CHECK_EQ(oftp_spl_ptn_next(sp, last, &qq), OFTP_SPL_NOTFOUND,
                     "no partner record yet");
            for (i = 0; i < 3; i++) {
                memset(&pp, 0, sizeof(pp));
                sprintf(pp.text, "PARTNER=PTN%d ODETTEID=O00130000PTN%d", i, i);
                CHECK_EQ(oftp_spl_ptn_add(sp, &pp), 0, "add a partner record");
                CHECK(memcmp(pp.key, "P000000", 7) == 0 && pp.key[7] == '1' + i,
                      "keys P0000001.. in order");
            }
            n = 0;
            last[0] = '\0';
            while (oftp_spl_ptn_next(sp, last, &qq) == 0) {
                memcpy(last, qq.key, 8);
                n++;
            }
            CHECK_EQ(n, 3, "next walks the three partner records");
            CHECK(strcmp(qq.text, "PARTNER=PTN2 ODETTEID=O00130000PTN2") == 0,
                  "the text is back");
            last[0] = '\0';
            while (oftp_spl_next(sp, last, &g) == 0) {
                memcpy(last, g.key, 8);
                after++;
            }
            CHECK_EQ(after, before, "the walk over transfers does not see them");
            CHECK_EQ(oftp_spl_purge(sp, 1000000, &cnt), 0, "purge");
            memcpy(last, "P0000002", 8);
            CHECK_EQ(oftp_spl_ptn_next(sp, last, &qq), 0,
                     "and does not take them");
            CHECK(memcmp(qq.key, "P0000003", 8) == 0, "next is above the key given");

            strcpy(pp.text, "PARTNER=PTN2 ODETTEID=O00130000PTN2 HOST=x.example.org");
            memcpy(pp.key, "P0000003", 8);
            CHECK_EQ(oftp_spl_ptn_put(sp, &pp), 0, "replace a partner record");
            memcpy(pp.key, "P0000009", 8);
            CHECK_EQ(oftp_spl_ptn_put(sp, &pp), OFTP_SPL_NOTFOUND,
                     "replace of a record not there: NOTFOUND");
            CHECK_EQ(oftp_spl_ptn_del(sp, "P0000002"), 0, "delete the middle one");
            CHECK_EQ(oftp_spl_ptn_del(sp, "P0000002"), OFTP_SPL_NOTFOUND,
                     "delete again: NOTFOUND");
            memset(&pp, 0, sizeof(pp));
            strcpy(pp.text, "PARTNER=PTN3 ODETTEID=O00130000PTN3");
            CHECK(oftp_spl_ptn_add(sp, &pp) == 0 && memcmp(pp.key, "P0000004", 8) == 0,
                  "a new record goes above the highest, into no gap");
        }
        oftp_spl_close(sp);

        /* they are there after a reopen */
        sp = oftp_spl_open("OFTPSPL", why, sizeof(why));
        CHECK(sp != NULL, "reopen again");
        if (sp != NULL) {
            OFTP_SPPTN qq;

            n = 0;
            last[0] = '\0';
            while (oftp_spl_ptn_next(sp, last, &qq) == 0) {
                memcpy(last, qq.key, 8);
                n++;
            }
            CHECK_EQ(n, 3, "three partner records survive");
            CHECK(strstr(qq.text, "PTN3") != NULL, "the last one is PTN3");
            oftp_spl_close(sp);
        }
    }
    return mbt_test_summary("TSTSPL");
}
