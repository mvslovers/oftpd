#ifndef OFTPMON_H
#define OFTPMON_H

/* oftpmon.h - the monitor: sessions, listener, scheduler, requests,
** console (src/mvs/oftp#mon.c). internals/design.md 2.
*/

#include <ibm/mvs/ihascvt.h>
#include <mvs/ecb.h>
#include <mvs/subsys.h>
#include <mvs/thread.h>

#include "oftpcfg.h"
#include "oftplog.h"
#include "oftprcv.h"
#include "oftpspl.h"
#include "oftpsvc.h"

typedef struct oftp_mon {
    char        eye[8];
    OFTP_CONFIG cfg;
    OFTP_RULES  rules;                  /* OFTPRCV0; replaced as a whole
                                        ** under lock(m, 0)              */
    char        ptnmember[57];          /* DSN(MEMBER) behind DD OFTPPTN,
                                        ** "" when it cannot be read    */
    OFTP_SPOOL *spool;
    OFTP_LOG    log;                    /* DD OFTPLOG (oftplog.h)        */
    volatile int stop;                  /* 1: P OFTPD received           */
    int         lsock;                  /* OFTP listener                 */
    volatile int nsess;
    volatile char busy[OFTP_MAXPTN];    /* sessions with the partner now */
    volatile char calling[OFTP_MAXPTN]; /* of them, our calls that have
                                        ** not yet claimed anything      */
    volatile char callreq[OFTP_MAXPTN]; /* operator asked for a call     */
    /* F OFTPD,TRACE: protocol trace for every session, or for the
    ** sessions of the partners flagged (same index as cfg.ptn). Each
    ** session asks before every command, so a change reaches the
    ** sessions already running.
    */
    volatile char trace_all;
    volatile char trace[OFTP_MAXPTN];
    long        tcall[OFTP_MAXPTN];     /* not reached: no call before   */
    unsigned    counter;                /* time counter of virtual files */
    long        tpurge;                 /* last housekeeping (KEEPDAYS=) */
    ECB         wake;                   /* wakes the scheduler           */
    CTHDTASK   *t_listen, *t_sched;
    /* the session subtasks, one array per task that attaches them: only
    ** the attaching task may DETACH, so each reaps its own (#3)
    */
    CTHDTASK   *lsess[OFTP_MAXSESS * 2];  /* attached by the listener     */
    CTHDTASK   *ssess[OFTP_MAXSESS * 2];  /* attached by the scheduler    */
    /* every open connection, for the abend: sockets are the system's,
    ** not the program's, and outlive it unless closed (0 = none)
    */
    volatile int ssock[OFTP_MAXSESS];   /* sessions                      */
    /* The EERP or NERP each session is sending, by spool key, in the
    ** session's ssock slot (all zeros: none). A received file stays
    ** RECEIVED until its EERP is sent, so this is what keeps a second
    ** session from sending it too (#18). It lives only as long as the
    ** run: a claim is given back with the session, abend or not.
    */
    char        erpheld[OFTP_MAXSESS][8];
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
    /* Piece two. The wake ECB is OFTPD's own private key-8 storage and
    ** its address is published in the anchor, because the SVC routine
    ** branch-POSTs it from the caller's address space (__xmpost,
    ** ECO-0004). It has to outlive every call, which is why it is a
    ** member here and not a local of the slot thread.
    */
    ECB          svcwake;               /* the slot thread's wake ECB    */
    CTHDTASK    *t_svc;                 /* the slot thread               */
    /* The slot thread has a stop flag of its own, separate from
    ** m->stop: it has to be joined BEFORE oftp_svc_stop() frees the
    ** anchor, and m->stop is what ends the sessions, which run on
    ** through the drain.
    */
    volatile int svcstop;
    SSCT        *ssct;                  /* OFT1, ours to unchain at stop */
    /* The sweep's pass counters, one per slot. OFTPD-private on
    ** purpose: they are not part of the protocol the routine knows,
    ** and a counter in CSA would be one more key-0 write per pass for
    ** nothing.
    */
    unsigned     sw_claimed[OFTP_SVC_NSLOT];
    unsigned     sw_copyout[OFTP_SVC_NSLOT];
} OFTP_MON;

/* One message line, already carrying its id: the console and the log,
** or the log only for class P (oftplog.h). Cut at OFTP_MSG_MAX.
*/
extern void oftp_mon_msg(const char *fmt, ...)          asm("OFTPMMSG");
/* The same for the ESTAE: if the log cannot be locked at once, the line
** goes to the console and the log is left alone
*/
extern void oftp_mon_msg_nowait(const char *fmt, ...)   asm("OFTPMMSN");

/* Opens DD OFTPLOG and makes the monitor block the one oftp_mon_msg()
** writes through. -1 (said on the console) when it will not open: the
** messages then go to the console. Call it before the first message
** that should reach the log.
*/
extern int  oftp_mon_logopen(OFTP_MON *m)               asm("OFTPMLGO");
/* Closes the log; later messages go to the console */
extern void oftp_mon_logclose(OFTP_MON *m)              asm("OFTPMLGC");

/* Starts listener and scheduler; 0 or -1 */
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
** on either way, for its partners and the operator but with no way in
** for the utilities, and every refusal says why.
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

/* Piece two (internals/xmem.md section 6). The division is by storage
** key, not by subject: everything below touches the key-0 anchor and
** therefore lives in oftp#svc.c, while the slot thread in oftp#mon.c
** runs the ordinary request handlers over the key-8 copy these hand
** it. No handler ever sees a CSA address.
**
** oftp_svc_activate() turns the transport on. It is the LAST step of
** the start: the SSCT has to be findable and the slot thread has to
** be waiting before a caller can claim a slot, or the first request
** parks with nobody to answer it.
**
** oftp_svc_take() copies slot i out when it is PENDING; 1 when there
** was one, 0 otherwise.
**
** oftp_svc_answer() writes the answer back, checks that the owner
** address space is still the one that claimed the slot, and posts it.
** When the owner is gone it frees the slot instead and posts nothing:
** a POST into a reused ASID would wake a stranger. Returns 1 when the
** caller was posted, 0 when the slot was dropped.
**
** oftp_svc_sweep() is one pass of the state table. Call it at most
** once a second -- OFTP_SW_* are counted in passes.
*/
extern void oftp_svc_activate(OFTP_MON *m)              asm("OFTPSACT");
extern void oftp_svc_deactivate(OFTP_MON *m)            asm("OFTPSDAC");
extern int  oftp_svc_take(OFTP_MON *m, unsigned i, OFTP_SVCJOB *j)
                                                        asm("OFTPSTAK");
extern int  oftp_svc_answer(OFTP_MON *m, unsigned i, const char *reply,
                            unsigned len, unsigned next, unsigned rc)
                                                        asm("OFTPSANS");
extern void oftp_svc_sweep(OFTP_MON *m)                 asm("OFTPSSWP");

/* oftp_svc_scrub() wipes the request and the reply of every slot. The
** stop calls it before the anchor is freed; nothing may be inside the
** routine by then.
*/
extern void oftp_svc_scrub(OFTP_MON *m)                 asm("OFTPSSCR");

/* The slot thread itself lives with the request handlers, because
** that is all it does: take a request out of a slot, run the same
** ctl_* handlers every request goes through, write the answer back.
**
** oftp_mon_svc_start() starts it and then turns the transport on, in
** that order. Call it only after oftp_svc_start() returned 0.
**
** oftp_mon_svc_stop() turns the transport off and joins the thread,
** and it must run BEFORE oftp_svc_stop(): that one frees the anchor,
** and a slot thread still looking at it would read freed CSA.
*/
extern int  oftp_mon_svc_start(OFTP_MON *m)             asm("OFTPMSVS");
extern void oftp_mon_svc_stop(OFTP_MON *m)              asm("OFTPMSVP");

/* Reads a configuration member through DD dd into c (partner 0: the
** monitor member, 1: the partner member); -1 when the DD cannot be opened
*/
extern int  oftp_mon_load(OFTP_CONFIG *c, const char *dd, int partner)
                                                        asm("OFTPMLOD");

/* Reads DD OFTPRCV (receive rules, include/oftprcv.h) into m->rules, as a
** whole: a member with any error is not used and the rules stay as they
** were (none, at start). Needs the partners loaded. refresh 0 at start,
** 1 from REFRESH, which only changes what the console says. 0, or -1 when
** nothing was replaced.
*/
extern int  oftp_mon_rules(OFTP_MON *m, int refresh)    asm("OFTPMRUL");

/* Reads the JFCB of DD OFTPPTN and keeps its data set and member name in
** m->ptnmember, for the ISPF dialog's Configure panel (request MEMBER)
*/
extern void oftp_mon_ptnmember(OFTP_MON *m)             asm("OFTPMPMB");

/* Reads DD OFTPPTN again and merges it (internals/design.md section 4).
** Only the parmlib layer moves: a partner kept by command is not touched,
** and wins over an OFTPPTN entry of its name. One no longer in the member
** is removed when no session and no transfer in the spool names it, else
** disabled. Returns the partners enabled, or -1 when the member cannot be
** read.
*/
extern int  oftp_mon_refresh(OFTP_MON *m)               asm("OFTPMRFR");

/* At the start, once the spool is open: puts the partners kept by command
** (the spool's P records) over the table read from OFTPPTN. Returns how
** many were loaded, or -1 when the spool cannot be read.
*/
extern int  oftp_mon_maint(OFTP_MON *m)                 asm("OFTPMPTN");

/* A MODIFY command's text */
extern void oftp_mon_command(OFTP_MON *m, const char *cmd)
                                                        asm("OFTPMCMD");

#endif /* OFTPMON_H */
