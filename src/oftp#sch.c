/* oftp#sch.c - when a send may start (portable); see oftpsch.h */
#include <stdlib.h>
#include <string.h>

#include "oftpsch.h"

/* "hh:mm" into minutes; hh up to maxh (23, or 24 for the end of a window,
** which then must be 24:00 exactly). -1 when it is not one. */
static int
hhmm(const char *s, int maxh)
{
    int h, m;

    if (strlen(s) != 5 || s[2] != ':')
        return -1;
    if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9'
        || s[3] < '0' || s[3] > '9' || s[4] < '0' || s[4] > '9')
        return -1;
    h = (s[0] - '0') * 10 + (s[1] - '0');
    m = (s[3] - '0') * 10 + (s[4] - '0');
    if (h > maxh || m > 59 || (h == 24 && m != 0))
        return -1;
    return h * 60 + m;
}

int
oftp_sch_start(const char *date, const char *time, const struct tm *now_tm,
               long now, long *t)
{
    struct tm tm = *now_tm, base = *now_tm;
    long a, b;
    int nodate = date[0] == '\0' || strcmp(date, "*") == 0;
    int notime = time[0] == '\0' || strcmp(time, "*") == 0;
    int mins = -1;

    *t = 0;
    if (!notime && (mins = hhmm(time, 23)) < 0)
        return -1;
    if (nodate && notime)
        return 0;
    if (!nodate) {
        if (date[0] == '*' && date[1] == '+') {
            if (date[2] < '0' || date[2] > '9' || date[3] != '\0')
                return -1;
            tm.tm_mday += date[2] - '0';
        } else {
            char *end;
            long d = strtol(date, &end, 10);

            if (*end != '\0' || strlen(date) != 3 || d < 1 || d > 366)
                return -1;
            if (d < now_tm->tm_yday + 1)
                tm.tm_year++;
            tm.tm_mon = 0;
            tm.tm_mday = (int)d;        /* mktime() makes it a date */
        }
    }
    if (mins >= 0) {
        tm.tm_hour = mins / 60;
        tm.tm_min = mins % 60;
        tm.tm_sec = 0;
    }
    /* Only the difference of two mktime() results is used, added to
    ** now. libc370's mktime() reads the struct as UTC -- the inverse of
    ** gmtime(), not of localtime(), which adds the zone -- so its result
    ** alone would be off by the zone on MVS (measured: a start 3 minutes
    ** ahead came out hours in the past). The difference is right either
    ** way.
    */
    tm.tm_isdst = -1;
    base.tm_isdst = -1;
    a = (long)mktime(&tm);
    b = (long)mktime(&base);
    if (a == -1 || b == -1)
        return -1;
    *t = now + (a - b);
    return 0;
}

int
oftp_sch_window(const char *s, unsigned short *from, unsigned short *to)
{
    char a[6], b[6];
    int f, e;

    *from = *to = 0;
    if (strlen(s) != 11 || s[5] != '-')
        return -1;
    memcpy(a, s, 5);
    a[5] = '\0';
    memcpy(b, s + 6, 5);
    b[5] = '\0';
    if ((f = hhmm(a, 23)) < 0 || (e = hhmm(b, 24)) < 0 || f == e)
        return -1;
    if (f == 0 && e == 24 * 60)
        return 0;                       /* the whole day: no window */
    *from = (unsigned short)f;
    *to = (unsigned short)(e == 24 * 60 ? 0 : e);
    /* 08:00-24:00 runs to midnight: kept as 08:00-00:00, a window that
    ** wraps, which inwin() reads the same way */
    return 0;
}

int
oftp_sch_inwin(unsigned from, unsigned to, unsigned minute)
{
    if (from == 0 && to == 0)
        return 1;
    if (from < to)
        return minute >= from && minute < to;
    return minute >= from || minute < to;
}
