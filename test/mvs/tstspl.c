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
        memset(&r, 0, sizeof(r));
        r.dir = SPL_RECV;
        CHECK(oftp_spl_add(sp, &r) == 0 && memcmp(r.key, "T0000004", 8) == 0,
              "the next key is T0000004");
        oftp_spl_close(sp);
    }
    return mbt_test_summary("TSTSPL");
}
