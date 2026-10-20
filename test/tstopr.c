/* TSTOPR - the operator command line: verb, operands, and a command
** that was given more than it takes (F OFTPD,PURGE 30,x) */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpopr.h"

static void
split(const char *cmd, const char *verb, const char *a1, const char *a2,
      int extra)
{
    char v[16], a[40], b[40], msg[120];

    oftp_opr_split(cmd, v, sizeof(v), a, sizeof(a), b, sizeof(b));
    snprintf(msg, sizeof(msg), "'%s': verb %s, operands '%s' '%s', extra %d",
             cmd, verb, a1, a2, extra);
    CHECK(strcmp(v, verb) == 0 && strcmp(a, a1) == 0 && strcmp(b, a2) == 0
          && oftp_opr_extra(v, b) == extra, msg);
}

int
main(void)
{
    char v[16], a[8], b[8];

    printf("=== TSTOPR operator command line ===\n");
    split("PURGE", "PURGE", "", "", 0);
    split("PURGE 30", "PURGE", "30", "", 0);
    split("  PURGE   30  ", "PURGE", "30", "", 0);
    split("PURGE,30", "PURGE", "30", "", 0);
    split("PURGE 30,x", "PURGE", "30", "x", 1);
    split("PURGE 30 x", "PURGE", "30", "x", 1);
    split("HOLD P1", "HOLD", "P1", "", 0);
    split("HOLD P1,P2", "HOLD", "P1", "P2", 1);
    split("RELEASE ALL", "RELEASE", "ALL", "", 0);
    split("RELEASE P1 P2", "RELEASE", "P1", "P2", 1);
    split("CALL MVS2", "CALL", "MVS2", "", 0);
    split("CALL MVS2,MVS3", "CALL", "MVS2", "MVS3", 1);
    /* TRACE takes two operands, DISPLAY a selector */
    split("TRACE ON MVS2", "TRACE", "ON", "MVS2", 0);
    split("TRACE OFF", "TRACE", "OFF", "", 0);
    split("D P", "D", "P", "", 0);
    split("REFRESH", "REFRESH", "", "", 0);

    /* a field longer than its buffer is cut, and what is left over is
    ** not lost: it shows as a second operand, so the command is refused */
    oftp_opr_split("HOLD ABCDEFGHIJKLMNOP", v, sizeof(v), a, sizeof(a), b,
                   sizeof(b));
    CHECK(strcmp(a, "ABCDEFG") == 0 && strcmp(b, "HIJKLMN") == 0
          && oftp_opr_extra("HOLD", b) == 1,
          "an operand too long for its field: the rest is a second operand");
    return mbt_test_summary("TSTOPR");
}
