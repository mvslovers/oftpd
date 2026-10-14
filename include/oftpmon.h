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
    long        tcall[OFTP_MAXPTN];     /* not reached: no call before   */
    unsigned    counter;                /* time counter of virtual files */
    ECB         wake;                   /* wakes the scheduler           */
    CTHDTASK   *t_listen, *t_sched, *t_ctl;
    /* every open connection, for the abend: sockets are the system's,
    ** not the program's, and outlive it unless closed (0 = none)
    */
    volatile int ssock[OFTP_MAXSESS];   /* sessions                      */
    volatile int cconn;                 /* the control-port request      */
} OFTP_MON;

/* Console and log: one line, already carrying its message id */
extern void oftp_mon_msg(const char *fmt, ...)          asm("OFTPMMSG");

/* Starts listener, scheduler and control port; 0 or -1 */
extern int  oftp_mon_start(OFTP_MON *m)                 asm("OFTPMSTR");
/* Stops them and waits for the sessions to end */
extern void oftp_mon_stop(OFTP_MON *m)                  asm("OFTPMSTP");

/* After an abend, from the ESTAE: closes the listeners and every open
** connection, takes no lock and frees nothing. Returns the sockets closed.
*/
extern int  oftp_mon_abend(OFTP_MON *m)                 asm("OFTPMABD");

/* Puts transfers left ACTIVE by an earlier run back in order */
extern void oftp_mon_recover(OFTP_MON *m)               asm("OFTPMREC");

/* Reads a configuration member through DD dd into c (partner 0: the
** monitor member, 1: the partner member); -1 when the DD cannot be opened
*/
extern int  oftp_mon_load(OFTP_CONFIG *c, const char *dd, int partner)
                                                        asm("OFTPMLOD");

/* Reads DD OFTPPTN again and merges it: partners are updated in place
** and new ones appended; one no longer in the member is disabled, never
** removed (a session may still use it). Returns the partners enabled, or
** -1 when the member cannot be read.
*/
extern int  oftp_mon_refresh(OFTP_MON *m)               asm("OFTPMRFR");

/* A MODIFY command's text */
extern void oftp_mon_command(OFTP_MON *m, const char *cmd)
                                                        asm("OFTPMCMD");

#endif /* OFTPMON_H */
