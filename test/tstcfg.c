/* TSTCFG - OFTPPRM and OFTPPTN parsing */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpcfg.h"

static int msgs;
static char lastmsg[256];

static void
count(const void *ctx, const char *line)
{
    (void)ctx;
    snprintf(lastmsg, sizeof(lastmsg), "%s", line);
    printf("    (%s)\n", line);
    msgs++;
}

static OFTP_CONFIG c;

int
main(void)
{
    static const char *prm[] = {
        "* monitor",
        "ODETTEID=o0013000000mvs1 PORT=3305",
        "INPREFIX=OFTP.IN TMPPREFIX=OFTP.TMP MAXSESS=2",
        "SDEB=2048,CREDIT=7",
        NULL };
    static const char *ptn[] = {
        "* partners",
        "PARTNER=MVS2 ODETTEID=O0013000000MVS2 PASSWORD=pw2",
        "  OURPASSWORD=PW1 HOST=mvs2.Example.org PORT=13305 LEVEL=4",
        "PARTNER=PC ODETTEID=O0013000000PC MODE=R COMPRESS=Y SESSIONS=3",
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
    CHECK_EQ(c.svc, 0, "SVC has no default: no number, no transport");
    CHECK_EQ(oftp_cfg_line(&c, 0, "PORT=0 BOGUS=1", 9, count, NULL), 2,
             "a bad value and an unknown keyword");
    /* the TCP control port is gone: a member that still names it gets
    ** the warning any unknown keyword gets (OFTP010W), and nothing else
    */
    CHECK_EQ(oftp_cfg_line(&c, 0, "CONTROLPORT=3306", 10, count, NULL), 1,
             "CONTROLPORT= is an unknown keyword");

    /* SVC= takes 200-255 only. Outside it has to be an error and must
    ** leave the field alone: a typo that silently left the transport
    ** off would say nothing at all.
    */
    CHECK_EQ(oftp_cfg_line(&c, 0, "SVC=237", 10, count, NULL), 0,
             "SVC=237 accepted");
    CHECK_EQ(c.svc, 237, "SVC=237 stored");
    CHECK_EQ(oftp_cfg_line(&c, 0, "SVC=199", 11, count, NULL), 1,
             "SVC below the installation range refused");
    CHECK_EQ(oftp_cfg_line(&c, 0, "SVC=256", 12, count, NULL), 1,
             "SVC above the installation range refused");
    CHECK_EQ(oftp_cfg_line(&c, 0, "SVC=0", 13, count, NULL), 1,
             "SVC=0 refused, not a silent off");
    CHECK_EQ(c.svc, 237, "a refused SVC leaves the number alone");

    /* ADMIN= takes one id each time and the list grows: a comma
    ** separates keywords here, so it cannot separate ids
    */
    CHECK_EQ(c.nadmin, 0, "no ADMIN= by default");
    CHECK_EQ(oftp_cfg_admin(&c, "IBMUSER"), 0, "and nobody is an administrator");
    CHECK_EQ(oftp_cfg_line(&c, 0, "ADMIN=ibmuser ADMIN=OPER1", 14, count, NULL),
             0, "two ADMIN= on one line");
    CHECK_EQ(oftp_cfg_line(&c, 0, "ADMIN=OPER1", 15, count, NULL), 0,
             "an id named again is accepted");
    CHECK_EQ(c.nadmin, 2, "and kept once");
    CHECK_EQ(oftp_cfg_admin(&c, "IBMUSER"), 1, "an ADMIN= id, upper-cased");
    CHECK_EQ(oftp_cfg_admin(&c, "OPER1"), 1, "the second one");
    CHECK_EQ(oftp_cfg_admin(&c, "OPER"), 0, "a prefix is not the id");
    CHECK_EQ(oftp_cfg_admin(&c, ""), 0, "no userid is never an administrator");
    CHECK_EQ(oftp_cfg_line(&c, 0, "ADMIN=TOOLONGID", 16, count, NULL), 1,
             "an id over 8 characters refused");
    CHECK_EQ(oftp_cfg_line(&c, 0, "ADMIN=", 17, count, NULL), 1,
             "an empty ADMIN= refused");
    for (i = 0; i < OFTP_MAXADM - 2; i++) {
        char l[20];

        snprintf(l, sizeof(l), "ADMIN=ADM%d", i);
        oftp_cfg_line(&c, 0, l, 18, count, NULL);
    }
    CHECK_EQ(c.nadmin, OFTP_MAXADM, "the list full");
    CHECK_EQ(oftp_cfg_line(&c, 0, "ADMIN=ONEMORE", 19, count, NULL), 1,
             "one more than OFTP_MAXADM refused");

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
        CHECK_EQ(p->sessions, 1, "one session at a time by default");
    }
    p = oftp_cfg_byid(&c, "O0013000000PC");
    CHECK(p != NULL && p->peer.mode == 'R' && p->peer.cmpr == 1 && p->call == 0,
          "PC by ID: mode R, compression, no HOST so not called");
    CHECK(p != NULL && p->sessions == 3, "PC: SESSIONS=3");
    CHECK(oftp_cfg_find(&c, "BAD") == NULL, "level 3 disables the partner");
    CHECK(oftp_cfg_find(&c, "NOID") == NULL, "no ODETTEID disables the partner");
    CHECK(oftp_cfg_find(&c, "DUP") == NULL, "a second ODETTEID disables the partner");
    CHECK(oftp_cfg_find(&c, "OFF") == NULL, "ENABLED=N");
    p = oftp_cfg_find(&c, "ACME");
    CHECK(p != NULL && strcmp(p->via, "MVS2") == 0 && p->call == 0,
          "ACME via MVS2, never called itself");
    CHECK(oftp_cfg_byid(&c, "O0013000000ACME") == NULL,
          "a partner behind another is not accepted as a caller");
    {
        char where[32];

        oftp_cfg_where(p, where, sizeof(where));
        CHECK(strcmp(where, "VIA MVS2") == 0, "D P names the partner ACME goes through");
        oftp_cfg_where(oftp_cfg_find(&c, "MVS2"), where, sizeof(where));
        CHECK(strcmp(where, "mvs2.Example.org:13305") == 0, "a direct partner shows host:port");
        oftp_cfg_where(oftp_cfg_byid(&c, "O0013000000PC"), where, sizeof(where));
        CHECK(strcmp(where, "-:3305") == 0, "a partner without HOST shows -:port");
    }
    CHECK(oftp_cfg_find(&c, "LOOP") == NULL, "VIA to a routed partner disabled");
    CHECK(oftp_cfg_find(&c, "GHOST") == NULL, "VIA to an unknown partner disabled");
    CHECK(msgs == 5, "five messages for the partner member");
    CHECK_EQ(oftp_cfg_line(&c, 1, "PARTNER=S0 SESSIONS=0", 20, count, NULL), 1,
             "SESSIONS=0 refused");
    CHECK_EQ(oftp_cfg_line(&c, 1, "PARTNER=S17 SESSIONS=17", 21, count, NULL), 1,
             "SESSIONS above 16 refused");
    CHECK_EQ(oftp_cfg_line(&c, 1, "PARTNER=S16 SESSIONS=16", 22, count, NULL), 0,
             "SESSIONS=16 accepted");

    /* retries: progressive, a day at most, MAXRETRY ends them */
    {
        long t = 0;

        CHECK(c.retry == 300 && c.maxretry == 10, "RETRY and MAXRETRY defaults");
        CHECK(oftp_cfg_retry(&c, 1, 1000, &t) == 0 && t == 1300, "after the first: RETRY");
        CHECK(oftp_cfg_retry(&c, 3, 1000, &t) == 0 && t == 1900, "after the third: 3 x RETRY");
        CHECK_EQ(oftp_cfg_retry(&c, 10, 1000, &t), 1, "the tenth was the last");
        CHECK_EQ(oftp_cfg_line(&c, 0, "MAXRETRY=0 RETRY=86400", 1, count, NULL), 0,
                 "MAXRETRY=0 and the longest RETRY");
        CHECK(oftp_cfg_retry(&c, 200, 0, &t) == 0 && t == 86400,
              "no limit, and never more than a day");
        CHECK_EQ(oftp_cfg_line(&c, 0, "MAXRETRY=256", 1, count, NULL), 1,
                 "MAXRETRY above 255 refused");
    }

    /* housekeeping: finished transfers are kept KEEPDAYS days */
    {
        OFTP_CONFIG k;

        oftp_cfg_defaults(&k);
        CHECK_EQ(k.keepdays, 7, "KEEPDAYS default 7");
        CHECK_EQ(oftp_cfg_line(&k, 0, "KEEPDAYS=0", 1, count, NULL), 0,
                 "KEEPDAYS=0: keep for ever");
        CHECK_EQ(k.keepdays, 0, "KEEPDAYS=0 taken");
        CHECK_EQ(oftp_cfg_line(&k, 0, "KEEPDAYS=9999", 1, count, NULL), 0,
                 "KEEPDAYS=9999 accepted");
        CHECK_EQ(oftp_cfg_line(&k, 0, "KEEPDAYS=10000", 1, count, NULL), 1,
                 "KEEPDAYS above 9999 refused");
        CHECK_EQ(oftp_cfg_line(&k, 0, "KEEPDAYS=X", 1, count, NULL), 1,
                 "KEEPDAYS not a number refused");
    }
    /* a word of more than 99 characters turns its entry off: cut short,
    ** PORT=0...0799 would have been PORT=0...07 (#113)
    */
    {
        OFTP_CONFIG k;
        char l[260];

        oftp_cfg_defaults(&k);
        strcpy(l, "PARTNER=LONG ODETTEID=O0013000000LONG ");
        memset(l + strlen(l), '0', 98);
        memcpy(l + strlen(l) - 98, "PORT=", 5);
        strcpy(l + strlen(l), "799");
        CHECK_EQ(oftp_cfg_line(&k, 1, l, 1, count, NULL), 1,
                 "a word of 101 characters is one error");
        oftp_cfg_finish(&k, count, NULL);
        CHECK(oftp_cfg_find(&k, "LONG") == NULL,
              "and its partner is off, not running on a cut value");
    }
    /* a word that is not KEYWORD=VALUE is named by its place, not shown:
    ** after PASSWORD=ab the word cd is the rest of a password (#121)
    */
    {
        OFTP_CONFIG k;

        oftp_cfg_defaults(&k);
        lastmsg[0] = '\0';
        CHECK_EQ(oftp_cfg_line(&k, 1, "PARTNER=PW ODETTEID=O1 PASSWORD=ab cd",
                               7, count, NULL), 1, "a word without '=' is one error");
        CHECK(strstr(lastmsg, "cd") == NULL && strstr(lastmsg, "ab") == NULL,
              "the message shows none of the words");
        CHECK(strcmp(lastmsg, "line 7: word 4 is not KEYWORD=VALUE") == 0,
              "but where it is");
    }
    /* a relayed file: the route was found without the lock, and the check
    ** under it must refuse a partner disabled since, and read the final
    ** partner's name only then (#121)
    */
    {
        OFTP_CONFIG k;
        OFTP_PARTNER *next, *fin;
        char nextname[9], tname[9];

        oftp_cfg_defaults(&k);
        oftp_cfg_line(&k, 1, "PARTNER=GW ODETTEID=O0013000000GW", 1, count, NULL);
        oftp_cfg_line(&k, 1, "PARTNER=FAR ODETTEID=O0013000000FAR VIA=GW", 2,
                      count, NULL);
        oftp_cfg_finish(&k, count, NULL);
        next = oftp_cfg_route(&k, "O0013000000FAR", &fin);
        CHECK(next != NULL && fin != NULL && next != fin, "FAR is reached through GW");
        if (next != NULL && fin != NULL && next != fin) {
            snprintf(nextname, sizeof(nextname), "%s", next->peer.name);
            tname[0] = 'x';
            CHECK_EQ(oftp_cfg_relay_live(next, nextname, fin, "O0013000000FAR",
                                         tname, sizeof(tname)), 1,
                     "both partners enabled: the route stands");
            CHECK(strcmp(tname, "FAR") == 0, "and the final name is copied");
            fin->enabled = 0;
            tname[0] = 'x';
            CHECK_EQ(oftp_cfg_relay_live(next, nextname, fin, "O0013000000FAR",
                                         tname, sizeof(tname)), 0,
                     "the final partner disabled since: no route");
            CHECK(tname[0] == '\0', "and no name is handed out");
            fin->enabled = 1;
            CHECK_EQ(oftp_cfg_relay_live(next, nextname, fin, "O0013000000OTHER",
                                         tname, sizeof(tname)), 0,
                     "the slot no longer holds the partner of that ID: no route");
            next->enabled = 0;
            CHECK_EQ(oftp_cfg_relay_live(next, nextname, fin, "O0013000000FAR",
                                         tname, sizeof(tname)), 0,
                     "the gateway disabled since: no route");
            next->enabled = 1;
            CHECK_EQ(oftp_cfg_relay_live(next, "OTHER", fin, "O0013000000FAR",
                                         tname, sizeof(tname)), 0,
                     "the gateway's slot holds another partner now: no route");
            CHECK_EQ(oftp_cfg_relay_live(fin, fin->peer.name, fin,
                                         "O0013000000FAR", tname, sizeof(tname)),
                     1, "a direct partner is its own route");
            CHECK(tname[0] == '\0', "with no separate final name");
            CHECK_EQ(oftp_cfg_relay_live(fin, fin->peer.name, fin,
                                         "O0013000000OTHER", tname, sizeof(tname)),
                     0, "a direct partner whose ID changed since the lookup: no route");
        }
    }
    return mbt_test_summary("TSTCFG");
}
