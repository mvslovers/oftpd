#include <string.h>

#include <mbtcheck.h>

#include "oftp.h"

int
main(void)
{
    const char *v = oftp_version();

    CHECK(v != NULL, "version string present");
    CHECK(strncmp(v, "oftpd ", 6) == 0, "starts with the project name");
    return mbt_test_summary("TSTVER");
}
