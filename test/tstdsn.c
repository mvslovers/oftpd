/* TSTDSN - data set names from virtual file names */
#include <string.h>

#include <mbtcheck.h>

#include "oftpdsn.h"

static char out[45];

static int
maps(const char *vfn, const char *suffix, const char *want)
{
    if (oftp_dsn_map("OFTP.IN", vfn, suffix, out) != 0)
        return 0;
    if (strcmp(out, want) != 0) {
        printf("    (%s -> %s, want %s)\n", vfn, out, want);
        return 0;
    }
    return 1;
}

int
main(void)
{
    printf("=== TSTDSN data set names ===\n");
    CHECK(oftp_dsn_valid("OFTP.IN.PAYROLL"), "valid name");
    CHECK(oftp_dsn_valid("A@#$.B-1"), "national characters and hyphen");
    CHECK(!oftp_dsn_valid("OFTP.IN.NOT-CATEGORIZED"), "qualifier of 15 refused");
    CHECK(!oftp_dsn_valid("OFTP..X"), "empty qualifier refused");
    CHECK(!oftp_dsn_valid("OFTP.1X"), "qualifier starting with a digit refused");
    CHECK(!oftp_dsn_valid(""), "empty name refused");

    CHECK(maps("PAYROLL.DATA", "", "OFTP.IN.PAYROLL.DATA"), "a valid name stays");
    CHECK(maps("NOT-CATEGORIZED", "", "OFTP.IN.NOT-CATE"), "cut to 8");
    CHECK(maps("hello.txt", "", "OFTP.IN.HELLO.TXT"), "upper-cased");
    CHECK(maps("report_2026 (1).csv", "", "OFTP.IN.REPORT20.CSV"), "invalid characters dropped");
    CHECK(maps("2026.LOG", "", "OFTP.IN.X2026.LOG"), "X before a digit");
    CHECK(maps("12345678", "", "OFTP.IN.X1234567"), "X, then cut again");
    CHECK(maps("a..b.", "", "OFTP.IN.A.B"), "empty qualifiers left out");
    CHECK(maps("...", "", "OFTP.IN.DATA"), "nothing left: DATA");
    CHECK(maps("NOT-CATEGORIZED", "T0000042", "OFTP.IN.NOT-CATE.T0000042"), "with suffix");
    CHECK(maps("AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD", "T0000001",
               "OFTP.IN.AAAAAAAA.BBBBBBBB.CCCCCCCC.T0000001"), "last qualifier dropped to fit 44");
    CHECK(oftp_dsn_map("A.VERY.LONG.PREFIX.WITH.MANY.QUALIFIERS.X", "B", "T0000001", out) == -1,
          "prefix and suffix alone too long");

    {
        char m[57];

        oftp_dsn_member("SYS1.PARMLIB                                ",
                        "OFTPPTN0", m);
        CHECK(strcmp(m, "SYS1.PARMLIB(OFTPPTN0)") == 0, "DSN(MEMBER)");
        oftp_dsn_member("SYS2.PARMLIB                                ",
                        "        ", m);
        CHECK(strcmp(m, "SYS2.PARMLIB") == 0, "no member: the DSN alone");
        oftp_dsn_member("A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.VW",
                        "ABCDEFGH", m);
        CHECK(strcmp(m, "A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.VW(ABCDEFGH)") == 0,
              "44 + 8 characters fill the buffer exactly");
        oftp_dsn_member("                                            ",
                        "OFTPPTN0", m);
        CHECK(m[0] == '\0', "blank DSN: empty");
    }
    return mbt_test_summary("TSTDSN");
}
