/* oftp#opr.c - the operator command line of F OFTPD,... */
#include <string.h>

#include "oftpopr.h"

static const char *
field(const char *p, char *d, size_t sz)
{
    size_t n = 0;

    while (*p == ' ' || *p == ',')
        p++;
    while (*p && *p != ' ' && *p != ',' && n + 1 < sz)
        d[n++] = *p++;
    d[n] = '\0';
    return p;
}

void
oftp_opr_split(const char *cmd, char *verb, size_t vsz, char *arg, size_t asz,
               char *arg2, size_t a2sz)
{
    const char *p = cmd;

    while (*p == ' ')
        p++;
    p = field(p, verb, vsz);
    p = field(p, arg, asz);
    field(p, arg2, a2sz);
}

int
oftp_opr_extra(const char *verb, const char *arg2)
{
    static const char *const one[] = { "PURGE", "HOLD", "RELEASE", "CALL" };
    size_t i;

    if (arg2[0] == '\0')
        return 0;
    for (i = 0; i < sizeof(one) / sizeof(one[0]); i++)
        if (strcmp(verb, one[i]) == 0)
            return 1;
    return 0;
}
