/* TSTSUB - the job after receipt: ?symbols in a JCL record, card width */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpsub.h"

static OFTP_SUBCTX ctx = {
    "PROD.ORDERS.IN", "ORDERS.JAN", "PARTNER1", "T0000042"
};

static char card[OFTP_SUB_CARD + 1];

/* the card as text, trailing blanks cut */
static const char *
sub(const char *rec, int *rc)
{
    static char t[OFTP_SUB_CARD + 1];
    int n;

    *rc = oftp_sub_card(rec, &ctx, card);
    memcpy(t, card, sizeof(t));
    n = (int)strlen(t);
    while (n > 0 && t[n - 1] == ' ')
        t[--n] = '\0';
    return t;
}

static void
spaces(char *dst, int n, char c)
{
    memset(dst, c, (size_t)n);
    dst[n] = '\0';
}

int
main(void)
{
    char rec[200], pad[100];
    const char *s;
    int rc;

    printf("=== TSTSUB symbols in a job ===\n");

    s = sub("//STEP1    EXEC PGM=IEFBR14", &rc);
    CHECK_EQ(rc, 0, "a line without a symbol goes through");
    CHECK(strcmp(s, "//STEP1    EXEC PGM=IEFBR14") == 0, "unchanged");
    CHECK_EQ((int)strlen(card), OFTP_SUB_CARD, "padded to the card width");

    s = sub("//IN       DD DSN=?PFILE,DISP=SHR", &rc);
    CHECK_EQ(rc, 0, "?PFILE");
    CHECK(strcmp(s, "//IN       DD DSN=PROD.ORDERS.IN,DISP=SHR") == 0,
          "is the data set the file was received as");

    s = sub("//* ?VFILE ?ID ?KEY", &rc);
    CHECK_EQ(rc, 0, "?VFILE ?ID ?KEY");
    CHECK(strcmp(s, "//* ORDERS.JAN PARTNER1 T0000042") == 0,
          "the virtual file name, the partner, the spool key");

    s = sub("//  PARM='?ID,?ID'", &rc);
    CHECK(rc == 0 && strcmp(s, "//  PARM='PARTNER1,PARTNER1'") == 0,
          "a symbol twice on a line");

    s = sub("//  DD DSN=?PFILE.BAK", &rc);
    CHECK(rc == 0 && strcmp(s, "//  DD DSN=PROD.ORDERS.IN.BAK") == 0,
          "text follows a symbol directly");

    s = sub("//  PARM='WHAT?,?FOO,??ID,?'", &rc);
    CHECK(rc == 0 && strcmp(s, "//  PARM='WHAT?,?FOO,?PARTNER1,?'") == 0,
          "a ? that is no symbol stays, so does an unknown name");

    s = sub("//  PARM='?id,?pfile'", &rc);
    CHECK(rc == 0 && strcmp(s, "//  PARM='?id,?pfile'") == 0,
          "symbols are upper case");

    /* the width of a // line is 72, 73-80 are the sequence numbers */
    strcpy(rec, "//X DD DSN=?PFILE");
    spaces(pad, 72 - (int)strlen(rec), ' ');
    strcat(rec, pad);
    strcat(rec, "00000100");
    s = sub(rec, &rc);
    CHECK(rc == 0 && strcmp(s, "//X DD DSN=PROD.ORDERS.IN") == 0,
          "columns 73 to 80 of a // line are not text, and go when it changes");

    strcpy(rec, "//X DD DSN=PROD.ORDERS.IN,DISP=SHR");
    spaces(pad, 72 - (int)strlen(rec), ' ');
    strcat(rec, pad);
    strcat(rec, "00000100");
    s = sub(rec, &rc);
    CHECK(rc == 0 && strcmp(s, rec) == 0 && strlen(card) == 80,
          "a // line without a symbol keeps its sequence numbers");

    /* ?ID is 3 characters, PARTNER1 is 8: five more */
    spaces(rec, 64, 'A');
    memcpy(rec, "//", 2);
    strcat(rec, "?ID");
    s = sub(rec, &rc);
    CHECK(rc == 0 && strlen(s) == 72, "a // line ends in column 72: fits");

    spaces(rec, 65, 'A');
    memcpy(rec, "//", 2);
    strcat(rec, "?ID");
    s = sub(rec, &rc);
    CHECK_EQ(rc, OFTP_SUB_LONG, "column 73: too long");

    ctx.pfile = "AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD.EEEEEEEE";
    s = sub("//SYSUT1   DD DSN=?PFILE,DISP=SHR", &rc);
    CHECK(rc == 0 && strlen(s) == 71, "a data set name of 44 fits where it fits");
    s = sub("//SYSUT1   DD DSN=?PFILE,DISP=(OLD,KEEP)", &rc);
    CHECK_EQ(rc, OFTP_SUB_LONG, "and not where it does not");
    ctx.pfile = "PROD.ORDERS.IN";

    /* anything else goes to column 80 */
    spaces(rec, 72, 'D');
    strcat(rec, "?ID");
    s = sub(rec, &rc);
    CHECK(rc == 0 && strlen(s) == 80, "a data line ends in column 80: fits");

    spaces(rec, 73, 'D');
    strcat(rec, "?ID");
    s = sub(rec, &rc);
    CHECK_EQ(rc, OFTP_SUB_LONG, "column 81 of a data line: too long");

    s = sub("/* ?ID", &rc);
    CHECK(rc == 0 && strcmp(s, "/* PARTNER1") == 0, "a /* line is a data line");

    s = sub("", &rc);
    CHECK(rc == 0 && s[0] == '\0' && strlen(card) == OFTP_SUB_CARD,
          "an empty record is a blank card");

    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK_EQ(rc, 0, "a virtual file name of letters, digits and . - _");

    ctx.vfile = "A'B";
    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK_EQ(rc, OFTP_SUB_UNSAFE, "a quote in the virtual file name: no job");
    ctx.vfile = "A B";
    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK_EQ(rc, OFTP_SUB_UNSAFE, "a blank as well");
    ctx.vfile = "A,B";
    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK_EQ(rc, OFTP_SUB_UNSAFE, "a comma");
    ctx.vfile = "A&B";
    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK_EQ(rc, OFTP_SUB_UNSAFE, "an ampersand");
    ctx.vfile = "A'B";
    s = sub("//  DD DSN=?PFILE", &rc);
    CHECK_EQ(rc, 0, "a job that does not use ?VFILE does not care");
    ctx.vfile = "a-b_c@d#e$f.9";
    s = sub("//  DD DSN=?VFILE", &rc);
    CHECK(rc == 0 && strcmp(s, "//  DD DSN=a-b_c@d#e$f.9") == 0,
          "lower case, - _ @ # $ are fine");
    ctx.vfile = "";
    s = sub("//  DD DSN=X.?VFILE", &rc);
    CHECK(rc == 0 && strcmp(s, "//  DD DSN=X.") == 0, "an empty value is empty");

    return mbt_test_summary("TSTSUB");
}
