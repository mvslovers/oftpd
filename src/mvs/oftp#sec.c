/* oftp#sec.c - the caller's own authority (MVS only); see oftpsec.h */
#include <stdio.h>
#include <string.h>

#include <ibm/mvs/cvt.h>
#include <mvs/racf.h>

#include "oftpsec.h"

int
oftp_sec_may_read(const char *user, const char *dsn, char *why, size_t cap)
{
    CVT  *cvt = CVTPTR;
    ACEE *acee;
    char  name[45];
    size_t n;
    int   rc = 0;

    if (cvt->cvtrac == NULL)
        return 0;                       /* no security product */
    if (user == NULL || user[0] == '\0') {
        snprintf(why, cap, "the caller has no userid");
        return 1;
    }
    /* the data set, not the member: profiles name data sets */
    n = strcspn(dsn, "(");
    if (n >= sizeof(name))
        n = sizeof(name) - 1;
    memcpy(name, dsn, n);
    name[n] = '\0';

    if ((acee = racf_login(user, NULL, NULL, &rc)) == NULL) {
        if (rc == 32)                   /* RACF is not active */
            return 0;
        snprintf(why, cap, "no security environment for %s (RACINIT rc %d)",
                 user, rc);
        return 1;
    }
    rc = racf_auth(acee, "DATASET", name, RACF_ATTR_READ);
    racf_logout(&acee);
    /* 0 a profile permits it, 4 none covers the data set: both SAF's
    ** "allowed", as OPEN itself would proceed (libc370 mvs/racf.h) */
    if (rc <= 4)
        return 0;
    snprintf(why, cap, "%s may not read %s", user, name);
    return 1;
}
