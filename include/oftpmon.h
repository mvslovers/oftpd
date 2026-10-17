#ifndef OFTPMON_H
#define OFTPMON_H

/* oftpmon.h - the monitor: sessions, listener, scheduler, control port,
** console (src/mvs/oftp#mon.c). internals/design.md 2.
*/

#include <ibm/mvs/ihascvt.h>
#include <mvs/ecb.h>
#include <mvs/thread.h>

#include "oftpcfg.h"
#include "oftpspl.h"
#include "oftpsvc.h"

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
    /* the session subtasks, one array per task that attaches them: only
    ** the attaching task may DETACH, so each reaps its own (#3)
    */
    CTHDTASK   *lsess[OFTP_MAXSESS * 2];  /* attached by the listener     */
    CTHDTASK   *ssess[OFTP_MAXSESS * 2];  /* attached by the scheduler    */
    /* every open connection, for the abend: sockets are the system's,
    ** not the program's, and outlive it unless closed (0 = none)
    */
    volatile int ssock[OFTP_MAXSESS];   /* sessions                      */
    volatile int cconn;                 /* the control-port request      */
    /* The cross-memory transport (internals/xmem.md, src/mvs/oftp#svc.c).
    ** It lives here and nowhere else: OFTPD is AC(1) and reentrant, so
    ** the ESTAE has to reach this state through the monitor block.
    ** svc_taken is the single flag that says the SVC entry is ours and
    ** therefore has to be given back -- the one step whose absence
    ** costs an IPL.
    */
    OFTP_ANCHOR *anchor;                /* the CSA anchor, key 0         */
    void        *svc_lpa;               /* OFTPSVC in CSA, for FREEMAIN  */
    void        *svc_epa;               /* its entry point               */
    unsigned     svc_size;
    SVCENTRY    *svc_slot;              /* the table entry reserved      */
    unsigned char svc_saved[8];         /* the 8 bytes it held before    */
    volatile int svc_taken;             /* 1: the entry is ours          */
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

/* The cross-memory transport (src/mvs/oftp#svc.c, internals/xmem.md).
**
** oftp_svc_start() installs it: the CSA anchor, OFTPSVC loaded into
** CSA, the anchor address patched into it and the SVC entry reserved.
** 0 when the transport is installed, -1 when it is not -- OFTPD runs
** on with TCP either way, and every refusal says why.
**
** oftp_svc_stop() gives the entry back, reads it back to prove it, and
** frees the CSA only when that read and the drain both agree.
**
** oftp_svc_restore() is the ESTAE's path to the one step that must not
** be skipped: a reserved entry nobody gave back makes that SVC number
** unavailable system-wide until the next IPL. It issues no message and
** returns 1 (given back), 0 (nothing to give back) or -1 (key 0 was
** unreachable, the entry is still ours).
*/
extern int  oftp_svc_start(OFTP_MON *m)                 asm("OFTPSSTR");
extern void oftp_svc_stop(OFTP_MON *m)                  asm("OFTPSSTP");
extern int  oftp_svc_restore(OFTP_MON *m)               asm("OFTPSRST");

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
