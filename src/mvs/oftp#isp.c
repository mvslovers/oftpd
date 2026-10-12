/* oftp#isp.c - ISPF dialog services for a C program (MVS only) */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <mvs/link.h>
#include <mvs/tso.h>

#include "oftpisp.h"

int
oftp_isp_init(OFTP_ISP *isp)
{
    isp->isplink = __load(NULL, "ISPLINK", NULL, NULL);
    return isp->isplink != NULL ? 0 : -1;
}

int
oftp_isp_vdef(OFTP_ISP *isp, const char *name, char *buf, int len)
{
    char service[9] = "VDEFINE ";
    char format[9] = "CHAR    ";
    char list[20];
    int flen = len;
    unsigned plist[5];

    if (isp->isplink == NULL)
        return 20;
    snprintf(list, sizeof(list), "(%s)", name);
    /* ISPLINK takes addresses, the last with its high-order bit set */
    plist[0] = (unsigned)service;
    plist[1] = (unsigned)list;
    plist[2] = (unsigned)buf;
    plist[3] = (unsigned)format;
    plist[4] = (unsigned)&flen | 0x80000000;
    return __call(isp->isplink, plist);
}

int
oftp_isp_exec(const char *fmt, ...)
{
    char req[300];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(req, sizeof(req), fmt, ap);
    va_end(ap);
    return tsocmd("ISPEXEC", req);
}

void
oftp_isp_put(char *field, int len, const char *s)
{
    int n = (int)strlen(s);

    if (n > len)
        n = len;
    memcpy(field, s, (size_t)n);
    memset(field + n, ' ', (size_t)(len - n));
}

void
oftp_isp_get(const char *field, int len, char *s)
{
    while (len > 0 && (field[len - 1] == ' ' || field[len - 1] == '\0'))
        len--;
    memcpy(s, field, (size_t)len);
    s[len] = '\0';
}
