/* TSTLOG - where a message goes: console and log, or the log only
** (class P), the log record, and what happens when there is no log or a
** write to it fails (internals/design.md section 8) */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <mbtcheck.h>

#include "oftplog.h"

#define MAXREC 16

static char con[MAXREC][OFTP_MSG_MAX + 8];
static int  ncon;
static char rec[MAXREC][OFTP_LOG_RECMAX + 1];
static unsigned reclen[MAXREC];
static int  nrec;
static int  putrc;      /* what the next put() answers */

static int
put(void *file, const char *r, unsigned len)
{
    (void)file;
    if (putrc != 0)
        return putrc;
    if (nrec < MAXREC) {
        memcpy(rec[nrec], r, len);
        rec[nrec][len] = '\0';
        reclen[nrec++] = len;
    }
    return 0;
}

static void
console(const char *line)
{
    if (ncon < MAXREC)
        snprintf(con[ncon++], sizeof(con[0]), "%s", line);
}

static const OFTP_LOG_IO io = { put, console };

static void
reset(void)
{
    ncon = nrec = putrc = 0;
}

static long
at(int h, int m, int s)
{
    struct tm tm, l;
    time_t t;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;
    tm.tm_mon = 9;
    tm.tm_mday = 18;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    tm.tm_isdst = -1;
    t = mktime(&tm);
    /* The record is stamped with localtime(), so the epoch must be one
    ** whose localtime() is h:m:s. libc370's mktime() reads the struct as
    ** UTC, so on MVS it is off by the zone: shift by what localtime() adds
    ** (0 on the host). */
    localtime_r(&t, &l);
    return (long)t - ((long)mktime(&l) - (long)t);
}

static int
fmt(char *buf, size_t n, const char *f, ...)
{
    va_list ap;
    int t;

    va_start(ap, f);
    t = oftp_log_vformat(buf, n, f, ap);
    va_end(ap);
    return t;
}

int
main(void)
{
    OFTP_LOG lg;
    char buf[OFTP_MSG_MAX + 1], big[300];
    int i;

    printf("=== TSTLOG console and log ===\n");

    /* the class is the letter after the number */
    CHECK_EQ(oftp_log_dest("OFTP211P PARTNER: SENT"), OFTP_DEST_LOG,
             "class P: the log only");
    CHECK_EQ(oftp_log_dest("OFTP211I PARTNER: SENT"),
             OFTP_DEST_CONSOLE | OFTP_DEST_LOG, "class I: console and log");
    CHECK_EQ(oftp_log_dest("OFTP212W X"), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "class W: console and log");
    CHECK_EQ(oftp_log_dest("OFTP209E X"), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "class E: console and log");
    CHECK_EQ(oftp_log_dest("OFTP098E X"), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "a two-digit-led number");
    CHECK_EQ(oftp_log_dest("PARTNER P HAS NO ID"),
             OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "text without a message id: console and log");
    CHECK_EQ(oftp_log_dest("OFTP2P X"), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "an id too short to be one is not class P");
    CHECK_EQ(oftp_log_dest("OFTP211 P"), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "the letter has to follow the number at once");
    CHECK_EQ(oftp_log_dest(""), OFTP_DEST_CONSOLE | OFTP_DEST_LOG,
             "empty line");

    /* one line, 123 characters at most; a longer one says so */
    CHECK_EQ(fmt(buf, sizeof(buf), "OFTP211P %s", "SHORT"), 0, "fits");
    CHECK(strcmp(buf, "OFTP211P SHORT") == 0, "text as formatted");
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    big[OFTP_MSG_MAX] = '\0';
    CHECK_EQ(fmt(buf, sizeof(buf), "%s", big), 0,
             "exactly the limit: nothing lost");
    CHECK_EQ((int)strlen(buf), OFTP_MSG_MAX, "limit length kept");
    big[OFTP_MSG_MAX] = 'A';
    big[OFTP_MSG_MAX + 1] = '\0';
    CHECK_EQ(fmt(buf, sizeof(buf), "%s", big), 1, "one over: cut");
    CHECK_EQ((int)strlen(buf), OFTP_MSG_MAX, "a cut line is at the limit");
    CHECK_EQ(buf[OFTP_MSG_MAX - 1], '>', "and ends in the mark");
    CHECK_EQ(buf[0], 'A', "the start is whole");

    /* a log that is open: class P to the log only, the rest to both */
    memset(&lg, 0, sizeof(lg));
    lg.file = &lg;
    lg.reclen = 133;
    reset();
    oftp_log_route(&lg, &io, at(21, 30, 15), "OFTP211P PEER1: 00000001 A SENT");
    CHECK_EQ(ncon, 0, "class P: nothing on the console");
    CHECK_EQ(nrec, 1, "class P: one log record");
    CHECK(strcmp(rec[0], "2026-10-18 21:30:15 OFTP211P PEER1: 00000001 A SENT")
          == 0, "the record is stamped and carries the message");
    oftp_log_route(&lg, &io, at(21, 30, 16), "OFTP209E PEER1: ABEND");
    CHECK_EQ(ncon, 1, "class E: on the console");
    CHECK(strcmp(con[0], "OFTP209E PEER1: ABEND") == 0,
          "the console gets the message without the stamp");
    CHECK_EQ(nrec, 2, "class E: and in the log");
    CHECK(strcmp(rec[1], "2026-10-18 21:30:16 OFTP209E PEER1: ABEND") == 0,
          "the record of it");

    /* a record never longer than the data set's records */
    lg.reclen = 40;
    reset();
    oftp_log_route(&lg, &io, at(1, 2, 3),
                   "OFTP211P PEER1: 00000001 A.VERY.LONG.DSN SENT");
    CHECK_EQ((int)reclen[0], 40, "cut to the record length");
    CHECK_EQ(rec[0][39], '>', "and marked");
    CHECK(strncmp(rec[0], "2026-10-18 01:02:03 OFTP211P PEER1:", 35) == 0,
          "the cut keeps the stamp and the start");

    /* a message of full length fits a 160 byte record, whole */
    lg.reclen = 160;
    reset();
    memset(big, 'B', sizeof(big) - 1);
    big[OFTP_MSG_MAX] = '\0';
    oftp_log_route(&lg, &io, at(0, 0, 0), big);
    CHECK_EQ((int)reclen[0], OFTP_LOG_STAMP + OFTP_MSG_MAX,
             "stamp plus a full message");
    CHECK(rec[0][reclen[0] - 1] == 'B', "not cut: no mark");

    /* no log (DD missing, or it would not open): everything to the
    ** console, class P included, and nothing is lost */
    memset(&lg, 0, sizeof(lg));
    reset();
    oftp_log_route(&lg, &io, at(1, 1, 1), "OFTP211P PEER1: SENT");
    CHECK_EQ(ncon, 1, "no log: class P goes to the console");
    CHECK(strcmp(con[0], "OFTP211P PEER1: SENT") == 0, "as it is");
    CHECK_EQ(nrec, 0, "no log: no record");
    oftp_log_route(&lg, &io, at(1, 1, 2), "OFTP209E PEER1: ABEND");
    CHECK_EQ(ncon, 2, "no log: class E to the console, once");

    /* a write that fails: said once, then the console only */
    memset(&lg, 0, sizeof(lg));
    lg.file = &lg;
    lg.reclen = 133;
    reset();
    putrc = 28;
    oftp_log_route(&lg, &io, at(2, 0, 0), "OFTP211P PEER1: SENT");
    CHECK_EQ(lg.failed, 1, "a failed write is remembered");
    CHECK_EQ(ncon, 2, "the failure and the message are on the console");
    CHECK(strncmp(con[0], "OFTP023E", 8) == 0, "the failure is OFTP023E");
    CHECK(strstr(con[0], "28") != NULL, "with the error number");
    CHECK(strcmp(con[1], "OFTP211P PEER1: SENT") == 0,
          "and the message that was lost to the log");
    putrc = 0;
    oftp_log_route(&lg, &io, at(2, 0, 1), "OFTP211P PEER1: SENT AGAIN");
    CHECK_EQ(ncon, 3, "after a failure class P goes to the console");
    CHECK_EQ(nrec, 0, "nothing is written to a log that failed");
    oftp_log_route(&lg, &io, at(2, 0, 2), "OFTP212W PEER1: REFUSED");
    CHECK_EQ(ncon, 4, "class W: once");
    for (i = 0; i < ncon; i++)
        if (i > 0)
            CHECK(strncmp(con[i], "OFTP023E", 8) != 0,
                  "the failure is not repeated");

    return mbt_test_summary("TSTLOG");
}
