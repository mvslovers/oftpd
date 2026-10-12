/* TSTCFG - OFTPPRM and OFTPPTN parsing */
#include <string.h>

#include <mbtcheck.h>

#include "oftpcfg.h"

static int msgs;

static void
count(void *ctx, const char *line)
{
    (void)ctx;
    printf("    (%s)\n", line);
    msgs++;
}

static OFTP_CONFIG c;

int
main(void)
{
    static const char *prm[] = {
        "* monitor",
        "ODETTEID=o0013000000mvs1 PORT=3305 CONTROLPORT=3306",
        "INPREFIX=OFTP.IN TMPPREFIX=OFTP.TMP MAXSESS=2",
        "SDEB=2048,CREDIT=7",
        NULL };
    static const char *ptn[] = {
        "* partners",
        "PARTNER=MVS2 ODETTEID=O0013000000MVS2 PASSWORD=pw2",
        "  OURPASSWORD=PW1 HOST=mvs2.Example.org PORT=13305 LEVEL=4",
        "PARTNER=PC ODETTEID=O0013000000PC MODE=R COMPRESS=Y",
        "PARTNER=BAD ODETTEID=O0013000000BAD LEVEL=3",
        "PARTNER=NOID HOST=x",
        "PARTNER=DUP ODETTEID=O0013000000MVS2",
        "PARTNER=OFF ODETTEID=O0013000000OFF ENABLED=N",
        "PARTNER=ACME ODETTEID=O0013000000ACME VIA=MVS2",
        "PARTNER=LOOP ODETTEID=O0013000000LOOP VIA=ACME",
        "PARTNER=GHOST ODETTEID=O0013000000GHOST VIA=NOBODY",
        NULL };
    OFTP_PARTNER *p;
    int i, on;

    printf("=== TSTCFG configuration ===\n");
    oftp_cfg_defaults(&c);
    for (i = 0; prm[i]; i++)
        oftp_cfg_line(&c, 0, prm[i], i + 1, count, NULL);
    CHECK_EQ(c.errors, 0, "monitor member: no errors");
    CHECK(strcmp(c.odetteid, "O0013000000MVS1") == 0, "values upper-cased");
    CHECK(c.maxsess == 2 && c.sdeb == 2048 && c.cred == 7, "numbers, comma as separator");
    CHECK_EQ(c.timeout, 60, "defaults kept");
    CHECK_EQ(oftp_cfg_line(&c, 0, "PORT=0 BOGUS=1", 9, count, NULL), 2,
             "a bad value and an unknown keyword");

    msgs = 0;
    for (i = 0; ptn[i]; i++)
        oftp_cfg_line(&c, 1, ptn[i], i + 1, count, NULL);
    on = oftp_cfg_finish(&c, count, NULL);
    CHECK_EQ(c.nptn, 9, "nine entries");
    CHECK_EQ(on, 3, "three enabled: MVS2, PC and ACME");

    p = oftp_cfg_find(&c, "MVS2");
    CHECK(p != NULL, "MVS2 by name");
    if (p != NULL) {
        CHECK(strcmp(p->host, "mvs2.Example.org") == 0, "HOST keeps its case");
        CHECK(strcmp(p->peer.pswd_in, "PW2") == 0, "password upper-cased");
        CHECK(p->port == 13305 && p->peer.level == 4, "port and level");
        CHECK(strcmp(p->peer.ourcode, "O0013000000MVS1") == 0, "our ID defaults to ODETTEID");
        CHECK(p->peer.sdeb == 2048 && p->peer.cred == 7, "buffer and credit from the monitor");
        CHECK(p->call == 1 && p->peer.mode == 'B', "callable, mode B");
    }
    p = oftp_cfg_byid(&c, "O0013000000PC");
    CHECK(p != NULL && p->peer.mode == 'R' && p->peer.cmpr == 1 && p->call == 0,
          "PC by ID: mode R, compression, no HOST so not called");
    CHECK(oftp_cfg_find(&c, "BAD") == NULL, "level 3 disables the partner");
    CHECK(oftp_cfg_find(&c, "NOID") == NULL, "no ODETTEID disables the partner");
    CHECK(oftp_cfg_find(&c, "DUP") == NULL, "a second ODETTEID disables the partner");
    CHECK(oftp_cfg_find(&c, "OFF") == NULL, "ENABLED=N");
    p = oftp_cfg_find(&c, "ACME");
    CHECK(p != NULL && strcmp(p->via, "MVS2") == 0 && p->call == 0,
          "ACME via MVS2, never called itself");
    CHECK(oftp_cfg_byid(&c, "O0013000000ACME") == NULL,
          "a partner behind another is not accepted as a caller");
    CHECK(oftp_cfg_find(&c, "LOOP") == NULL, "VIA to a routed partner disabled");
    CHECK(oftp_cfg_find(&c, "GHOST") == NULL, "VIA to an unknown partner disabled");
    CHECK(msgs == 5, "five messages for the partner member");
    return mbt_test_summary("TSTCFG");
}
