/* TSTPTN - maintaining the partner table in memory: apply, check, text,
** gaps (src/oftp#cfg.c; the monitor adds the spool and the locks)
*/
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpcfg.h"

static OFTP_CONFIG c;

/* What the monitor does for ADD: a new entry from text, checked, then in */
static int
add(const char *text, char *why, int whylen)
{
    OFTP_PARTNER n;
    int slot;

    oftp_cfg_newptn(&n, "");
    /* the name is the first keyword, as in a request */
    if (strncmp(text, "PARTNER=", 8) == 0) {
        char name[9];
        int i = 0;

        for (text += 8; *text && *text != ' ' && i < 8; )
            name[i++] = *text++;
        name[i] = '\0';
        oftp_cfg_newptn(&n, name);
    }
    if (oftp_cfg_apply(&n, text, why, whylen) != 0)
        return -1;
    oftp_cfg_complete(&c, &n);
    if (oftp_cfg_check(&c, &n, -1, why, whylen) != 0)
        return -1;
    if ((slot = oftp_cfg_alloc(&c)) < 0) {
        snprintf(why, (size_t)whylen, "table full");
        return -1;
    }
    c.ptn[slot] = n;
    return slot;
}

int
main(void)
{
    char why[80], text[300];
    OFTP_PARTNER n, back;
    int a, b, i, cnt, on;

    printf("=== TSTPTN partner maintenance ===\n");
    oftp_cfg_defaults(&c);
    oftp_cfg_line(&c, 0, "ODETTEID=O0013000000MVS1 SDEB=2048 CREDIT=7", 1, NULL, NULL);

    /* ADD */
    a = add("PARTNER=MVS2 ODETTEID=O0013000000MVS2 PASSWORD=pw2 HOST=Mvs2.Example.org",
            why, sizeof(why));
    CHECK(a == 0, "a first partner goes into slot 0");
    CHECK(oftp_cfg_find(&c, "MVS2") != NULL, "and is found by name");
    CHECK(strcmp(c.ptn[0].host, "Mvs2.Example.org") == 0, "HOST keeps its case");
    CHECK(strcmp(c.ptn[0].peer.pswd_in, "PW2") == 0, "the password is upper-cased");
    CHECK(c.ptn[0].call == 1 && c.ptn[0].port == 3305, "callable, default port");
    CHECK(c.ptn[0].peer.sdeb == 2048 && c.ptn[0].peer.cred == 7,
          "buffer and credit from the monitor settings");
    CHECK_EQ(add("PARTNER=MVS2 ODETTEID=O0013000000XXXX", why, sizeof(why)), -1,
             "the same name again is refused");
    CHECK(strstr(why, "exists") != NULL, "and says why");
    CHECK_EQ(add("PARTNER=OTHER ODETTEID=O0013000000MVS2", why, sizeof(why)), -1,
             "the same Odette ID is refused");
    CHECK(strstr(why, "MVS2") != NULL, "and names the partner that has it");
    CHECK_EQ(add("PARTNER=NOID HOST=x", why, sizeof(why)), -1,
             "no ODETTEID is refused");
    CHECK(strstr(why, "ODETTEID") != NULL, "and says so");
    CHECK_EQ(add("PARTNER=BAD ODETTEID=O1 LEVEL=3", why, sizeof(why)), -1,
             "a bad value is refused");
    CHECK(strstr(why, "LEVEL") != NULL, "and names the keyword");
    CHECK_EQ(add("PARTNER=BAD ODETTEID=O1 BOGUS=1", why, sizeof(why)), -1,
             "an unknown keyword is refused");
    CHECK(strstr(why, "BOGUS") != NULL, "and names it");
    CHECK_EQ(add("PARTNER=BAD ODETTEID=O1 FOO", why, sizeof(why)), -1,
             "a word without = is refused");
    CHECK_EQ(c.nptn, 1, "nothing refused left a slot behind");

    /* VIA */
    b = add("PARTNER=ACME ODETTEID=O0013000000ACME VIA=MVS2", why, sizeof(why));
    CHECK(b == 1, "a partner behind MVS2");
    CHECK(c.ptn[1].call == 0, "never called itself");
    CHECK_EQ(add("PARTNER=GHOST ODETTEID=O0013000000GH VIA=NOBODY", why,
                 sizeof(why)), -1, "VIA to an unknown partner refused");
    CHECK_EQ(add("PARTNER=LOOP ODETTEID=O0013000000LP VIA=ACME", why,
                 sizeof(why)), -1, "VIA to a routed partner refused");
    n = c.ptn[0];
    oftp_cfg_apply(&n, "VIA=ACME", why, sizeof(why));
    CHECK_EQ(oftp_cfg_check(&c, &n, 0, why, sizeof(why)), -1,
             "a partner others are reached through cannot get a VIA");

    /* CHANGE: the copy is changed, checked, then replaces the slot */
    n = c.ptn[0];
    CHECK_EQ(oftp_cfg_apply(&n, "PORT=13305 LEVEL=4 MODE=S", why, sizeof(why)), 0,
             "keywords onto an existing partner");
    CHECK_EQ(oftp_cfg_check(&c, &n, 0, why, sizeof(why)), 0,
             "it still passes the check as itself");
    oftp_cfg_complete(&c, &n);
    c.ptn[0] = n;
    CHECK(c.ptn[0].port == 13305 && c.ptn[0].peer.level == 4
          && c.ptn[0].peer.mode == 'S', "the change is in");
    CHECK(strcmp(c.ptn[0].peer.pswd_in, "PW2") == 0, "what was not named is kept");
    n = c.ptn[0];
    CHECK_EQ(oftp_cfg_apply(&n, "PARTNER=OTHER", why, sizeof(why)), -1,
             "a partner cannot be renamed");
    CHECK_EQ(oftp_cfg_apply(&n, "PARTNER=MVS2 SESSIONS=0", why, sizeof(why)), -1,
             "SESSIONS=0 refused");
    n = c.ptn[0];
    oftp_cfg_apply(&n, "ODETTEID=O0013000000ACME", why, sizeof(why));
    CHECK_EQ(oftp_cfg_check(&c, &n, 0, why, sizeof(why)), -1,
             "an Odette ID another partner has is refused on a change as well");

    /* a HOST given later makes a partner that had none callable */
    a = add("PARTNER=LATE ODETTEID=O0013000000LATE", why, sizeof(why));
    CHECK(a == 2 && c.ptn[2].call == 0, "no HOST: not called");
    n = c.ptn[2];
    oftp_cfg_apply(&n, "HOST=late.example.org", why, sizeof(why));
    oftp_cfg_complete(&c, &n);
    CHECK_EQ(n.call, 1, "a HOST given later: called");
    oftp_cfg_apply(&n, "CALL=N", why, sizeof(why));
    oftp_cfg_complete(&c, &n);
    CHECK_EQ(n.call, 0, "CALL=N is kept");
    oftp_cfg_apply(&n, "CALL=Y HOST=", why, sizeof(why));
    oftp_cfg_complete(&c, &n);
    CHECK(n.call == 0 && n.host[0] == '\0', "an empty HOST= takes the host away");

    /* disable and enable */
    n = c.ptn[0];
    oftp_cfg_apply(&n, "ENABLED=N", why, sizeof(why));
    c.ptn[0] = n;
    CHECK(oftp_cfg_find(&c, "MVS2") == NULL, "a disabled partner is not found");
    CHECK_EQ(oftp_cfg_slot(&c, "MVS2"), 0, "but its slot is");
    CHECK(oftp_cfg_byid(&c, "O0013000000MVS2") == NULL, "nor by Odette ID");
    CHECK(oftp_cfg_route(&c, "O0013000000ACME", (OFTP_PARTNER *[]){ NULL }) == NULL,
          "nothing is routed through a disabled gateway");

    /* text: what the spool holds, and back */
    n = c.ptn[0];
    n.address[0] = '\0';
    oftp_cfg_apply(&n, "ENABLED=Y COMPRESS=Y XLATE=Y SESSIONS=3 ADDRESS=10.0.0.9 "
                   "OURID=O0013000000ALT OURPASSWORD=mine", why, sizeof(why));
    oftp_cfg_complete(&c, &n);
    i = oftp_cfg_text(&c, &n, text, sizeof(text));
    CHECK(i > 0 && (size_t)i == strlen(text), "the text has its length");
    CHECK(strncmp(text, "PARTNER=MVS2 ODETTEID=O0013000000MVS2", 37) == 0,
          "the name and the Odette ID come first");
    CHECK(strstr(text, "SDEB") == NULL && strstr(text, "CREDIT") == NULL,
          "what is the monitor's default is not written down");
    CHECK(strstr(text, "ENABLED") == NULL, "enabled is the default");
    oftp_cfg_newptn(&back, "MVS2");
    CHECK_EQ(oftp_cfg_apply(&back, text, why, sizeof(why)), 0,
             "the text is read back");
    oftp_cfg_complete(&c, &back);
    CHECK(memcmp(&back.peer, &n.peer, sizeof(back.peer)) == 0
          && strcmp(back.host, n.host) == 0 && back.port == n.port
          && back.sessions == n.sessions && back.xlate == n.xlate
          && strcmp(back.address, n.address) == 0 && back.call == n.call
          && back.enabled == n.enabled, "and gives the same partner");
    n.enabled = 0;
    n.callwant = 0;
    oftp_cfg_text(&c, &n, text, sizeof(text));
    CHECK(strstr(text, "ENABLED=N") != NULL && strstr(text, "CALL=N") != NULL,
          "disabled and not called are written down");
    CHECK_EQ(oftp_cfg_text(&c, &n, text, 20), -1, "a buffer too small is told");

    /* the monitor settings change: a partner without its own value follows */
    oftp_cfg_newptn(&back, "MVS2");
    oftp_cfg_apply(&back, "ODETTEID=O1", why, sizeof(why));
    c.sdeb = 8192;
    oftp_cfg_complete(&c, &back);
    CHECK_EQ(back.peer.sdeb, 8192, "SDEB from the monitor as it is now");

    /* gaps */
    cnt = oftp_cfg_count(&c, &on);
    CHECK(cnt == 3 && on == 2, "three partners, MVS2 off");
    oftp_cfg_free(&c, 1);
    CHECK_EQ(c.nptn, 3, "a freed slot stays in the count of slots");
    CHECK_EQ(oftp_cfg_count(&c, &on), 2, "but is no partner");
    CHECK(oftp_cfg_find(&c, "ACME") == NULL && oftp_cfg_slot(&c, "ACME") < 0,
          "and is not found");
    CHECK_EQ(oftp_cfg_slot(&c, ""), -1, "an empty name finds no gap");
    a = add("PARTNER=NEW ODETTEID=O0013000000NEW", why, sizeof(why));
    CHECK_EQ(a, 1, "the gap is used again");
    CHECK_EQ(c.nptn, 3, "and the table did not grow");

    /* VIA through a gap or a partner that went: disabled, said once */
    {
        OFTP_PARTNER *nw = &c.ptn[oftp_cfg_slot(&c, "NEW")];
        int slot = oftp_cfg_slot(&c, "NEW");

        strcpy(nw->via, "GONE");
        nw->enabled = 1;
        CHECK_EQ(oftp_cfg_vias(&c, NULL, NULL), 1, "a VIA with no partner is disabled");
        CHECK(!c.ptn[slot].enabled, "and off");
        CHECK_EQ(oftp_cfg_vias(&c, NULL, NULL), 0, "nothing more to say");
        nw->via[0] = '\0';
        nw->enabled = 1;
    }

    /* full: gaps are used, a full table says so */
    while (c.nptn < OFTP_MAXPTN) {
        char l[80];

        snprintf(l, sizeof(l), "PARTNER=P%d ODETTEID=O00130000P%d", c.nptn, c.nptn);
        CHECK(add(l, why, sizeof(why)) >= 0, "fill the table");
    }
    CHECK_EQ(add("PARTNER=ONEMORE ODETTEID=O0013000000ONE", why, sizeof(why)), -1,
             "a full table refuses");
    oftp_cfg_free(&c, 5);
    CHECK_EQ(add("PARTNER=ONEMORE ODETTEID=O0013000000ONE", why, sizeof(why)), 5,
             "until a partner is deleted");

    return mbt_test_summary("TSTPTN");
}
