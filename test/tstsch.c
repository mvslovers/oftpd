/* TSTSCH - earliest start and the sending window of a send (rvs STRTTIME,
** STRTDATE, SENDWIN) */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <mbtcheck.h>

#include "oftpsch.h"

static struct tm now_tm;
static long now;

/* the local time t falls on, as "YYYY-MM-DD HH:MM" */
static const char *
at(long t)
{
    static char buf[64];
    time_t tt = (time_t)t;
    struct tm tm;

    localtime_r(&tt, &tm);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    return buf;
}

static long
start(const char *date, const char *time)
{
    long t = -1;

    if (oftp_sch_start(date, time, &now_tm, now, &t) != 0)
        return -1;
    return t;
}

int
main(void)
{
    unsigned short f, to;

    printf("=== TSTSCH send time ===\n");
    /* now: 2026-10-18 21:30:15 local, day 291 */
    memset(&now_tm, 0, sizeof(now_tm));
    now_tm.tm_year = 126;
    now_tm.tm_mon = 9;
    now_tm.tm_mday = 18;
    now_tm.tm_hour = 21;
    now_tm.tm_min = 30;
    now_tm.tm_sec = 15;
    now_tm.tm_isdst = -1;
    now = (long)mktime(&now_tm);

    CHECK_EQ(start("", ""), 0, "nothing given: no limit");
    CHECK_EQ(start("*", "*"), 0, "* and *: no limit either");
    CHECK(strcmp(at(start("*", "22:00")), "2026-10-18 22:00") == 0, "today 22:00");
    CHECK(start("*", "08:00") <= now, "today 08:00 is past: due now");
    CHECK(strcmp(at(start("*+1", "08:00")), "2026-10-19 08:00") == 0, "tomorrow 08:00");
    CHECK(strcmp(at(start("*+1", "")), "2026-10-19 21:30") == 0,
          "tomorrow, no time: the time of now");
    CHECK(strcmp(at(start("*+9", "00:00")), "2026-10-27 00:00") == 0, "*+9");
    CHECK(strcmp(at(start("300", "06:15")), "2026-10-27 06:15") == 0, "day 300 this year");
    CHECK(strcmp(at(start("001", "00:00")), "2027-01-01 00:00") == 0,
          "day 1 is before today: next year");
    CHECK(strcmp(at(start("291", "23:00")), "2026-10-18 23:00") == 0, "today by its number");
    CHECK(strcmp(at(start("", "23:59")), "2026-10-18 23:59") == 0, "a time alone: today");

    CHECK_EQ(start("*", "24:00"), -1, "24:00 refused");
    CHECK_EQ(start("*", "12:60"), -1, "12:60 refused");
    CHECK_EQ(start("*", "1200"), -1, "no colon refused");
    CHECK_EQ(start("*+10", ""), -1, "*+10 refused");
    CHECK_EQ(start("000", ""), -1, "day 0 refused");
    CHECK_EQ(start("367", ""), -1, "day 367 refused");
    CHECK_EQ(start("12X", ""), -1, "not a number refused");

    /* windows */
    CHECK_EQ(oftp_sch_window("22:00-06:00", &f, &to), 0, "a window over midnight");
    CHECK(f == 22 * 60 && to == 6 * 60, "kept as minutes");
    CHECK(oftp_sch_inwin(f, to, 23 * 60), "23:00 is in it");
    CHECK(oftp_sch_inwin(f, to, 5 * 60 + 59), "05:59 is in it");
    CHECK(!oftp_sch_inwin(f, to, 6 * 60), "06:00 is not");
    CHECK(!oftp_sch_inwin(f, to, 12 * 60), "noon is not");
    CHECK_EQ(oftp_sch_window("08:00-17:30", &f, &to), 0, "a window by day");
    CHECK(oftp_sch_inwin(f, to, 8 * 60) && !oftp_sch_inwin(f, to, 17 * 60 + 30),
          "from 08:00 up to 17:30");
    CHECK_EQ(oftp_sch_window("00:00-24:00", &f, &to), 0, "the whole day");
    CHECK(f == 0 && to == 0, "is no window at all");
    CHECK(oftp_sch_inwin(0, 0, 3 * 60), "no window: always");
    CHECK_EQ(oftp_sch_window("08:00-08:00", &f, &to), -1, "an empty window refused");
    CHECK_EQ(oftp_sch_window("08:00", &f, &to), -1, "half a window refused");
    CHECK_EQ(oftp_sch_window("25:00-26:00", &f, &to), -1, "hours past 24 refused");

    return mbt_test_summary("TSTSCH");
}
