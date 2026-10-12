#ifndef OFTPMON_H
#define OFTPMON_H

/* oftpmon.h - the monitor: sessions, listener, scheduler, control port,
** console (src/mvs/oftp#mon.c). internals/design.md 2.
*/

#include <mvs/ecb.h>
#include <mvs/thread.h>

#include "oftpcfg.h"
#include "oftpspl.h"

typedef struct oftp_mon {
    char        eye[8];
    OFTP_CONFIG cfg;
    OFTP_SPOOL *spool;
    volatile int stop;                  /* 1: P OFTPD received           */
    int         lsock;                  /* OFTP listener                 */
    int         csock;                  /* control port listener         */
    volatile int nsess;
    volatile char busy[OFTP_MAXPTN];    /* partner in a session          */
    volatile char callreq[OFTP_MAXPTN]; /* operator asked for a call     */
    unsigned    counter;                /* time counter of virtual files */
    ECB         wake;                   /* wakes the scheduler           */
    CTHDTASK   *t_listen, *t_sched, *t_ctl;
} OFTP_MON;

/* Console and log: one line, already carrying its message id */
extern void oftp_mon_msg(const char *fmt, ...)          asm("OFTPMMSG");

/* Starts listener, scheduler and control port; 0 or -1 */
extern int  oftp_mon_start(OFTP_MON *m)                 asm("OFTPMSTR");
/* Stops them and waits for the sessions to end */
extern void oftp_mon_stop(OFTP_MON *m)                  asm("OFTPMSTP");

/* Puts transfers left ACTIVE by an earlier run back in order */
extern void oftp_mon_recover(OFTP_MON *m)               asm("OFTPMREC");

/* A MODIFY command's text */
extern void oftp_mon_command(OFTP_MON *m, const char *cmd)
                                                        asm("OFTPMCMD");

#endif /* OFTPMON_H */
