#ifndef OFTPSCH_H
#define OFTPSCH_H

/* oftpsch.h - when a send may start (src/oftp#sch.c, portable)
**
** After rvsMVS's STRTDATE, STRTTIME and SENDWIN (User Manual A.2 #26-28):
**   STRTDATE  "*" or "" today, "*+n" n days on (0-9), "ddd" that day of
**             the year (001-366), next year when it is before today
**   STRTTIME  "*" or "" the time of now, "hh:mm" 00:00-23:59
**   SENDWIN   "hh:mm-hh:mm": a send starts only inside it, every day;
**             it may run past midnight (22:00-06:00); "00:00-24:00" is
**             no window. A transfer already running is not stopped.
*/

#include <time.h>

/* The earliest start as a time_t value into *t: 0 when neither is given
** (no limit). now_tm is now as local time (localtime of now), now the
** same instant as a number; the result is now plus the local difference,
** so it does not depend on how mktime() reads a struct tm.
** 0, or -1 for an operand that is not valid.
*/
extern int oftp_sch_start(const char *date, const char *time,
                          const struct tm *now_tm, long now, long *t)
                                                        asm("OFTPSCST");

/* "hh:mm-hh:mm" into minutes of the day; both 0 for no window. 0 or -1. */
extern int oftp_sch_window(const char *s, unsigned short *from,
                           unsigned short *to)          asm("OFTPSCWN");

/* Whether minute (of the day, local) is inside from-to; no window: 1 */
extern int oftp_sch_inwin(unsigned from, unsigned to, unsigned minute)
                                                        asm("OFTPSCIW");

#endif /* OFTPSCH_H */
