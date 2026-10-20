/* TSTRCV - receive rules: the member, matching, symbols, names */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftprcv.h"

static int msgs;

static void
count(void *ctx, const char *line)
{
    (void)ctx;
    printf("    (%s)\n", line);
    msgs++;
}

static OFTP_CONFIG cfg;
static OFTP_RULES rs;

static void
partners(void)
{
    static const char *ptn[] = {
        "PARTNER=PARTNER1 ODETTEID=O0013000000P1",
        "PARTNER=PARTNER2 ODETTEID=O0013000000P2",
        "PARTNER=OFF ODETTEID=O0013000000OFF ENABLED=N",
        NULL };
    int i;

    oftp_cfg_defaults(&cfg);
    for (i = 0; ptn[i]; i++)
        oftp_cfg_line(&cfg, 1, ptn[i], i + 1, count, NULL);
    oftp_cfg_finish(&cfg, count, NULL);
}

static int
load(const char **lines)
{
    int i;

    oftp_rcv_clear(&rs);
    msgs = 0;
    for (i = 0; lines[i]; i++)
        oftp_rcv_line(&rs, lines[i], i + 1, count, NULL);
    return oftp_rcv_finish(&rs, &cfg, count, NULL);
}

static const char *
rule_for(const char *partner, const char *vfn)
{
    const OFTP_RULE *r = oftp_rcv_match(&rs, partner, vfn);

    return r == NULL ? "-" : r->name;
}

static char out[45];

/* the name rule r makes of vfn from PARTNER1 on 2026-10-20 14:05:09 */
static const char *
named(const char *rule, const char *vfn)
{
    OFTP_RCVCTX x;
    const OFTP_RULE *r = NULL;
    int i;

    x.vfn = vfn;
    x.partner = "PARTNER1";
    x.key = "T0000042";
    x.arrival = "20261020140509";
    x.sfidtime = "1405090007";
    for (i = 0; i < rs.n; i++)
        if (strcmp(rs.r[i].name, rule) == 0)
            r = &rs.r[i];
    if (r == NULL || oftp_rcv_name(r, &x, out) != 0)
        return "invalid";
    return out;
}

int
main(void)
{
    static const char *member[] = {
        "* receive rules",
        "RULE=ORDERS  FROM=partner1  VFN=orders.*",
        "             DSN=PROD.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS",
        "RULE=INVOICE FROM=*,VFN=INV?????.DATA REPLACE=Y",
        "             DSN=ACCT.IN.%VFN UNIT=SYSDA VOLSER=work01 SPACE=30",
        "RULE=Q       FROM=PARTNER2 VFN=*.TXT XLATE=N",
        "             DSN=%ID.%Q2.%Q1.C%CTR.%KEY",
        "RULE=ALL     DSN=PUBLIC.%Q1",
        "RULE=PLAIN   FROM=OFF VFN=PLAIN REPLACE=Y",
        NULL };
    const OFTP_RULE *r;

    printf("=== TSTRCV receive rules ===\n");
    partners();

    CHECK(oftp_rcv_wild("*", "ANYTHING"), "* matches anything");
    CHECK(oftp_rcv_wild("*", ""), "* matches nothing as well");
    CHECK(oftp_rcv_wild("ORD*", "ORD"), "* matches no characters");
    CHECK(oftp_rcv_wild("ORD*", "ORDERS.X"), "trailing *");
    CHECK(oftp_rcv_wild("*.DATA", "A.B.DATA"), "leading *");
    CHECK(oftp_rcv_wild("A*B*C", "AXXBYYBZC"), "two * need backtracking");
    CHECK(!oftp_rcv_wild("A*B*C", "AXXBYYBZ"), "and fail when the end differs");
    CHECK(oftp_rcv_wild("INV?????.DATA", "INV12345.DATA"), "? is exactly one");
    CHECK(!oftp_rcv_wild("INV?????.DATA", "INV1234.DATA"), "too short for ?");
    CHECK(!oftp_rcv_wild("INV?????.DATA", "INV123456.DATA"), "too long for ?");
    CHECK(oftp_rcv_wild("orders.*", "ORDERS.1"), "case does not matter");
    CHECK(!oftp_rcv_wild("ORDERS", "ORDERS1"), "no wildcard, no prefix match");
    CHECK(!oftp_rcv_wild("", "X"), "an empty pattern matches only nothing");

    CHECK_EQ(load(member), 0, "the member loads without errors");
    CHECK_EQ(msgs, 0, "and says nothing");
    CHECK_EQ(rs.n, 5, "five rules");
    r = &rs.r[1];
    CHECK(strcmp(r->from, "*") == 0 && strcmp(r->vfn, "INV?????.DATA") == 0,
          "a comma separates keywords; values upper-cased");
    CHECK(r->replace == 1 && strcmp(r->unit, "SYSDA") == 0
          && strcmp(r->volser, "WORK01") == 0 && r->space == 30,
          "REPLACE, UNIT, VOLSER, SPACE");
    CHECK(r->xlate == -1, "XLATE defaults to the partner's");
    CHECK(rs.r[2].xlate == 0, "XLATE=N");
    CHECK(rs.r[0].replace == 0 && rs.r[0].space == 0, "REPLACE=N and no SPACE by default");
    CHECK(rs.r[0].line == 2, "the line of the rule");

    CHECK(strcmp(rule_for("PARTNER1", "ORDERS.JAN"), "ORDERS") == 0, "partner and name");
    CHECK(strcmp(rule_for("PARTNER2", "ORDERS.JAN"), "ALL") == 0,
          "another partner does not match ORDERS and falls to ALL");
    CHECK(strcmp(rule_for("PARTNER2", "INV00001.DATA"), "INVOICE") == 0, "FROM=* takes any partner");
    CHECK(strcmp(rule_for("PARTNER1", "INV00001.DATA"), "INVOICE") == 0, "the same for PARTNER1");
    CHECK(strcmp(rule_for("PARTNER2", "NOTES.TXT"), "Q") == 0, "PARTNER2 and *.TXT");
    CHECK(strcmp(rule_for("PARTNER1", "NOTES.TXT"), "ALL") == 0, "PARTNER1's .TXT is not rule Q");
    CHECK(strcmp(rule_for("PARTNER1", "ORDERS.TXT"), "ORDERS") == 0,
          "the first match wins, in member order");
    CHECK(strcmp(rule_for("OFF", "PLAIN"), "ALL") == 0,
          "ALL is before PLAIN and takes everything");

    r = oftp_rcv_match(&rs, "PARTNER1", "ORDERS.JAN");
    CHECK(r != NULL && strcmp(r->name, "ORDERS") == 0, "match returns the rule");

    CHECK(strcmp(named("ORDERS", "ORDERS.JAN"), "PROD.ORDERS.Y2026.D1020.T140509") == 0,
          "date and time of arrival");
    CHECK(strcmp(named("INVOICE", "INV00001.DATA"), "ACCT.IN.INV00001.DATA") == 0, "%VFN");
    CHECK(strcmp(named("INVOICE", "inv00001.data"), "ACCT.IN.INV00001.DATA") == 0,
          "%VFN is upper-cased");
    CHECK(strcmp(named("Q", "SECOND.FIRST.TXT"), "PARTNER1.FIRST.SECOND.C0007.T0000042") == 0,
          "%ID, %Q1, %Q2, %CTR, %KEY");
    CHECK(strcmp(named("ALL", "ONE"), "PUBLIC.ONE") == 0, "%Q1 of a name with one qualifier");
    CHECK(strcmp(named("Q", "ONE"), "invalid") == 0,
          "a qualifier the name does not have leaves an empty one: refused");
    CHECK(strcmp(named("INVOICE", "AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD"),
                 "ACCT.IN.AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD") == 0,
          "43 characters fit");
    CHECK(strcmp(named("ALL", "NOT-CATEGORIZED"), "invalid") == 0,
          "a qualifier of 15 characters: refused");
    CHECK(strcmp(named("ALL", "2026"), "invalid") == 0, "a qualifier starting with a digit: refused");
    CHECK(strcmp(named("ALL", "A_B"), "invalid") == 0, "a character no name may hold: refused");
    CHECK(strcmp(named("INVOICE", "AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD.E"), "invalid") == 0,
          "more than 44 characters: refused");
    CHECK(strcmp(named("PLAIN", "PLAIN"), "invalid") == 0, "a rule without DSN= makes no name");

    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%QQ", NULL }), 1, "an unknown symbol");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%", NULL }), 1, "a lone %");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%YY", NULL }), 1, "%YY is not a symbol");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%JJJ", NULL }), 1, "nor is %JJJ");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%Q0", NULL }), 1, "nor %Q0");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X_Y", NULL }), 1, "a literal no name may hold");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=X.%YYYY%MM%DD.%HH%MI%SS", NULL }), 0,
             "all six date and time symbols");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=%VFN", NULL }), 1, "DSN=%VFN: first qualifier is the partner's");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=PROD%VFN", NULL }), 1, "DSN=PROD%VFN");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=%Q1.X", NULL }), 1, "DSN=%Q1.X");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=P%Q9.X", NULL }), 1, "DSN=P%Q9.X");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=%CTR.X", NULL }), 1, "DSN=%CTR.X");
    CHECK_EQ(msgs, 1, "refused with a message");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=PROD.IN.%VFN", NULL }), 0, "DSN=PROD.IN.%VFN");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=PROD.%Q1.%CTR", NULL }), 0, "symbols after the first qualifier");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=%ID.IN.%VFN", NULL }), 0, "%ID may lead");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=%KEY.X", NULL }), 0, "so may %KEY");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=OFTP.TMP.X", NULL }), 1, "a name under TMPPREFIX");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=OFTP.%Q1", NULL }), 1, "a name that may reach TMPPREFIX");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=OFTP.TMP", NULL }), 1, "TMPPREFIX itself");
    CHECK_EQ(load((const char *[]){ "RULE=A DSN=OFTP.IN.%VFN", NULL }), 0, "next to TMPPREFIX");
    CHECK_EQ(load((const char *[]){ "VFN=X", NULL }), 1, "a keyword before the first RULE=");
    CHECK_EQ(load((const char *[]){ "RULE=A BOGUS=1", NULL }), 1, "an unknown keyword");
    CHECK_EQ(load((const char *[]){ "RULE=A REPLACE=MAYBE", NULL }), 1, "REPLACE=MAYBE");
    CHECK_EQ(load((const char *[]){ "RULE=A SPACE=0", NULL }), 1, "SPACE=0");
    CHECK_EQ(load((const char *[]){ "RULE=A SPACE=10000", NULL }), 1, "SPACE=10000");
    CHECK_EQ(load((const char *[]){ "RULE=A SPACE=1,5", NULL }), 1,
             "SPACE=1,5: the comma ends the keyword and 5 is no keyword");
    CHECK_EQ(load((const char *[]){ "RULE=A VOLSER=TOOLONG1", NULL }), 1, "VOLSER over 6");
    CHECK_EQ(load((const char *[]){ "RULE=TOOLONGNAME", NULL }), 1, "a rule name over 8");
    CHECK_EQ(load((const char *[]){ "RULE=", NULL }), 1, "an empty rule name");
    CHECK_EQ(load((const char *[]){ "RULE=A FROM=NOBODY", NULL }), 1, "FROM= names no partner");
    CHECK_EQ(load((const char *[]){ "RULE=A FROM=OFF", NULL }), 0,
             "a disabled partner is still a partner");
    CHECK_EQ(load((const char *[]){ "RULE=A", "RULE=A", NULL }), 1, "a rule defined twice");
    CHECK_EQ(load((const char *[]){ "RULE=A X", NULL }), 1, "a word that is no KEYWORD=VALUE");
    CHECK(load((const char *[]){ "RULE=A DSN=X", "RULE=B BOGUS=1", "RULE=C DSN=Z", NULL }) > 0,
          "one bad rule makes the member unusable as a whole");
    CHECK(msgs == 1, "and is told once");

    {
        char l[40];
        int i;

        oftp_rcv_clear(&rs);
        for (i = 0; i < OFTP_MAXRULE; i++) {
            snprintf(l, sizeof(l), "RULE=R%d", i);
            oftp_rcv_line(&rs, l, i + 1, count, NULL);
        }
        CHECK_EQ(rs.n, OFTP_MAXRULE, "the table full");
        CHECK_EQ(oftp_rcv_line(&rs, "RULE=ONEMORE", 99, count, NULL), 1,
                 "one more than OFTP_MAXRULE refused");
        CHECK_EQ(rs.n, OFTP_MAXRULE, "and the table unchanged");
    }
    {
        OFTP_RCVCTX nx;
        char out[64];

        memset(&nx, 0, sizeof(nx));
        oftp_rcv_clear(&rs);
        oftp_rcv_line(&rs, "RULE=N DSN=A.%Q2.B.%VFN", 1, count, NULL);
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1,
                 "a context with nothing in it gives no name, and no crash");

        oftp_rcv_clear(&rs);
        oftp_rcv_line(&rs, "RULE=C DSN=PROD.X%CTR.DATA", 1, count, NULL);
        oftp_rcv_line(&rs, "RULE=D DSN=PROD.Y%YYYY%MM", 2, count, NULL);
        nx.vfn = "F";
        nx.partner = "P";
        nx.key = "T0000001";
        nx.arrival = "20261020140509";
        nx.sfidtime = "1405090007";
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), 0, "%CTR of a full time field");
        CHECK(strcmp(out, "PROD.X0007.DATA") == 0, "is its last four digits");
        nx.sfidtime = "      ";
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1, "a blank time field has no counter");
        nx.sfidtime = "1405";
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1, "nor a short one");
        nx.sfidtime = "14050900 7";
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1, "nor one with a blank in the counter");
        nx.sfidtime = "14050900AB";
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1, "nor letters");
        nx.sfidtime = NULL;
        CHECK_EQ(oftp_rcv_name(&rs.r[0], &nx, out), -1, "nor a missing one");
        nx.arrival = "2026";
        CHECK_EQ(oftp_rcv_name(&rs.r[1], &nx, out), -1, "a short arrival time is refused too");
    }
    return mbt_test_summary("TSTRCV");
}
