/* oftp#log.c - message routing: console, log, or the log only
**
** Portable on purpose (the host tests link it): the data set and the
** console are reached through the OFTP_LOG_IO the caller hands in.
** No static data.
*/
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "oftplog.h"

int
oftp_log_dest(const char *msg)
{
    if (strncmp(msg, "OFTP", 4) == 0
        && isdigit((unsigned char)msg[4]) && isdigit((unsigned char)msg[5])
        && isdigit((unsigned char)msg[6]) && msg[7] == 'P')
        return OFTP_DEST_LOG;
    return OFTP_DEST_CONSOLE | OFTP_DEST_LOG;
}

int
oftp_log_vformat(char *buf, size_t size, const char *fmt, va_list ap)
{
    int n = vsnprintf(buf, size, fmt, ap);

    if (n < 0 || (size_t)n >= size) {
        if (size > 1)
            buf[size - 2] = '>';
        return 1;
    }
    return 0;
}

/* "YYYY-MM-DD HH:MM:SS " and msg, at most max bytes, the end marked when
** msg did not fit; the length
*/
static unsigned
record(char *rec, unsigned max, long now, const char *msg)
{
    time_t tt = (time_t)now;
    struct tm tm;
    unsigned n;

    localtime_r(&tt, &tm);
    snprintf(rec, OFTP_LOG_STAMP + 1, "%04u-%02u-%02u %02u:%02u:%02u ",
             ((unsigned)tm.tm_year + 1900u) % 10000u,
             ((unsigned)tm.tm_mon + 1u) % 100u, (unsigned)tm.tm_mday % 100u,
             (unsigned)tm.tm_hour % 100u, (unsigned)tm.tm_min % 100u,
             (unsigned)tm.tm_sec % 100u);
    n = (unsigned)strlen(msg);
    if (n > OFTP_MSG_MAX)
        n = OFTP_MSG_MAX;
    memcpy(rec + OFTP_LOG_STAMP, msg, n);
    n += OFTP_LOG_STAMP;
    if (n > max) {
        n = max;
        rec[n - 1] = '>';
    }
    return n;
}

void
oftp_log_route(OFTP_LOG *lg, const OFTP_LOG_IO *io, long now, const char *msg)
{
    int dest = oftp_log_dest(msg);

    if (lg->file != NULL && !lg->failed) {
        char rec[OFTP_LOG_RECMAX + 1];
        unsigned max = lg->reclen > 0 && lg->reclen < OFTP_LOG_RECMAX
                       ? lg->reclen : OFTP_LOG_RECMAX;
        unsigned n = record(rec, max, now, msg);
        int rc = io->put(lg->file, rec, n);

        if (rc == 0) {
            if (dest & OFTP_DEST_CONSOLE)
                io->console(msg);
            return;
        }
        /* The log is given up, not retried: every message after this
        ** one goes to the console, so that none is lost. */
        {
            char line[OFTP_MSG_MAX + 1];

            lg->failed = 1;
            snprintf(line, sizeof(line),
                     "OFTP023E LOG WRITE FAILED, ERRNO %d, MESSAGES GO TO"
                     " THE CONSOLE FROM NOW ON", rc);
            io->console(line);
        }
    }
    io->console(msg);
}
