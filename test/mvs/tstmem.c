/* TSTMEM - the partner member OFTPD reports for the ISPF dialog (MVS)
**
** Asks a RUNNING OFTPD (SVC= in OFTPPRM) for request MEMBER and compares
** the answer with the name given in PARM, the data set and member that
** DD OFTPPTN of the monitor's job names (test/mvs/tstmem.jcl):
**
**   PARM='RUN SYS1.PARMLIB(OFTPPTN0)'   the member is behind the DD
**   PARM='RUN -'                        no member readable: no 1xx line
*/
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpcli.h"

static void
line(void *ctx, const char *text)
{
    char *got = ctx;

    if (got[0] == '\0')
        strncpy(got, text, 79);
    else
        strcpy(got, "(more than one line)");
}

int
main(int argc, char **argv)
{
    char got[80], final[200];
    int rc, code = 0;

    printf("=== TSTMEM partner member ===\n");
    if (argc < 3 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs a running OFTPD with SVC= and PARM='RUN dsn(member)'"
               " (test/mvs/tstmem.jcl)\n");
        return 0;
    }
    got[0] = '\0';
    rc = oftp_cli_request("MEMBER", line, got, final, sizeof(final), &code);
    printf("  rc %d, code %d, \"%s\", member \"%s\"\n", rc, code, final, got);
    CHECK_EQ(rc, 0, "MEMBER is answered");
    CHECK_EQ(code, 200, "with 200");
    if (strcmp(argv[2], "-") == 0)
        CHECK(got[0] == '\0', "no member: no line");
    else
        CHECK(strcmp(got, argv[2]) == 0, "the data set and member of DD OFTPPTN");
    return mbt_test_summary("TSTMEM");
}
