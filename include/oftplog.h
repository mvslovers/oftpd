#ifndef OFTPLOG_H
#define OFTPLOG_H

/* oftplog.h - where a message goes: the console, the log, or the log
** only (src/oftp#log.c, internals/design.md section 8).
**
** A message id is OFTP, three digits and a class letter. I, W and E go
** to the console and to the log; P goes to the log only and never to the
** console. With no log (the DD is missing, or a write to it failed) every
** message goes to the console instead, and that is said once.
*/

#include <stdarg.h>
#include <stddef.h>

/* A message is one line of at most this many characters (the longest
** text a WTO takes); one that is longer is cut and ends in '>'.
*/
#define OFTP_MSG_MAX    123

/* A log record is "YYYY-MM-DD HH:MM:SS " and the message */
#define OFTP_LOG_STAMP  20
#define OFTP_LOG_RECMAX (OFTP_LOG_STAMP + OFTP_MSG_MAX)

#define OFTP_DEST_CONSOLE   1
#define OFTP_DEST_LOG       2

typedef struct oftp_log {
    void       *file;       /* the open log, NULL: none                   */
    unsigned    reclen;     /* its record length                          */
    int         failed;     /* a write failed: reported, log given up     */
} OFTP_LOG;

/* The two places a message can go. put() writes one record of len bytes
** and answers 0, or the error number; console() writes one line.
*/
typedef struct oftp_log_io {
    int     (*put)(void *file, const char *rec, unsigned len);
    void    (*console)(const char *line);
} OFTP_LOG_IO;

/* OFTP_DEST_* of a message line, by its class letter */
extern int  oftp_log_dest(const char *msg)                  asm("OFTPLDST");

/* vsnprintf() into buf (size OFTP_MSG_MAX + 1) with the end of a cut
** line marked; 1 when the line was cut, else 0
*/
extern int  oftp_log_vformat(char *buf, size_t size, const char *fmt,
                             va_list ap)                    asm("OFTPLFMT");

/* Sends a formatted message where its class says. now is the time of the
** log record (time()). The caller serializes calls on one OFTP_LOG.
*/
extern void oftp_log_route(OFTP_LOG *lg, const OFTP_LOG_IO *io, long now,
                           const char *msg)                 asm("OFTPLRTE");

#endif /* OFTPLOG_H */
