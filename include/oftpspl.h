#ifndef OFTPSPL_H
#define OFTPSPL_H

/* oftpspl.h - the spool: one KSDS record per transfer (src/mvs/oftp#spl.c)
**
** One thread owns the cluster and is the only one that issues VSAM
** requests; every other thread calls the functions below, which queue a
** request and wait for it (internals/design.md 3.4). Keys are 8 octets:
** X'00'*8 the control record, 'T' + 7 digits a transfer.
**
** The record is this struct, fixed length; the cluster is defined with
** KEYS(8 0) RECORDSIZE(512 512).
*/

#include "oftpwire.h"

#define OFTP_SPLRECL    512

/* direction */
#define SPL_SEND        'S'
#define SPL_RECV        'R'

/* states, internals/design.md 3.3 */
#define SPL_QUEUED      'Q'             /* send: waiting for the partner  */
#define SPL_ACTIVE      'A'             /* in a session now               */
#define SPL_WAITING     'W'             /* send: retry after tnext        */
#define SPL_HELD        'H'             /* held by operator or user       */
#define SPL_SENT        'S'             /* send: EFPA, EERP outstanding   */
#define SPL_DELIVERED   'D'             /* send: EERP received            */
#define SPL_REJECTED    'J'             /* refused (SFNA/EFNA N, NERP)    */
#define SPL_FAILED      'F'             /* retries exhausted, local error */
#define SPL_RECEIVED    'R'             /* receive: kept, EERP owed       */
#define SPL_CONFIRMED   'C'             /* receive: EERP sent             */

typedef struct oftp_sprec {
    char        key[8];
    char        dir;                    /* SPL_SEND / SPL_RECV            */
    char        state;
    char        fmt;                    /* F V U T                        */
    char        reasby;                 /* 'P' partner's code, 'L' ours   */
    char        partner[9];
    char        owner[9];               /* userid who queued / receives   */
    char        dsn[OFTP_DSNLEN + 1];   /* virtual file identity          */
    char        date[OFTP_DATLEN + 1];
    char        time[OFTP_TIMLEN + 1];
    char        user[OFTP_USRLEN + 1];
    char        dest[OFTP_IDLEN + 1];
    char        orig[OFTP_IDLEN + 1];
    char        local[45];              /* the data set on this system    */
    char        member[9];
    char        xlate;                  /* 'Y' translate F/V/U data       */
    char        erp;                    /* send: 'E'/'N' arrived          */
    char        ppstate;                /* post-processing, later         */
    char        disp;                   /* send: 'D' delete the data set
                                           once the EERP is in; else keep */
    unsigned    lrecl;
    unsigned    attempts;
    int         reas;                   /* last reason code               */
    int         pprc;
    unsigned long recs;
    unsigned long octets;
    long        tqueued, tlast, tnext, tdone;
    char        text[80];               /* last reason, readable          */
    char        target[9];              /* send: the partner behind
                                           'partner' it is for (VIA), or "" */
    /* relaying (OFTPD as a node, internals/design.md 0) */
    char        relay;                  /* 'Y': not ours, passed through  */
    char        from[9];                /* relay send: the partner it came
                                           from, where its EERP goes back */
    char        crea[OFTP_IDLEN + 1];   /* relayed NERP: its creator      */
    /* when a send may start (oftpsch.h): not before tstart (0 = now),
    ** and only inside winfrom-winto each day (both 0 = any time) */
    long        tstart;
    unsigned short winfrom, winto;      /* minutes of the day, local      */
} OFTP_SPREC;

typedef struct oftp_spool OFTP_SPOOL;

/* Opens the cluster behind DD ddname and starts the spool thread. An
** empty cluster gets its control record. NULL on failure, *why says.
*/
extern OFTP_SPOOL *oftp_spl_open(const char *ddname, char *why, int whylen)
                                                        asm("OFTPSPOP");
/* Stops the spool thread and closes the cluster. Call it from the task
** that called oftp_spl_open(): the CLOSE has to run in the TCB that
** opened (#12).
*/
extern void oftp_spl_close(OFTP_SPOOL *sp)              asm("OFTPSPCL");
/* After an abend in the calling task: gives back the spool's lock if the
** abend came while this task held it (queueing a request). Without that
** the spool thread waits for it for good. A lock this task does not hold
** is left alone.
*/
extern void oftp_spl_release(OFTP_SPOOL *sp)            asm("OFTPSPRL");
/* Not 0 when the open had to VERIFY the cluster first (left open by an
** abend): the return code of the open that failed.
*/
extern int  oftp_spl_verified(const OFTP_SPOOL *sp)     asm("OFTPSPVF");

/* 0 or a negative error. add assigns rec->key. */
extern int  oftp_spl_add(OFTP_SPOOL *sp, OFTP_SPREC *rec) asm("OFTPSPAD");
extern int  oftp_spl_get(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
                                                        asm("OFTPSPGT");
extern int  oftp_spl_put(OFTP_SPOOL *sp, const OFTP_SPREC *rec)
                                                        asm("OFTPSPPT");
extern int  oftp_spl_del(OFTP_SPOOL *sp, const char *key) asm("OFTPSPDL");
/* put, but only while the record in the spool still has state `was`:
** CHANGED when something else changed it since it was read (a session
** that claimed it, say). Read, compare and write are one request.
*/
extern int  oftp_spl_putif(OFTP_SPOOL *sp, const OFTP_SPREC *rec, char was)
                                                        asm("OFTPSPPI");
/* The first transfer with a key above key ("" for the first of all) */
extern int  oftp_spl_next(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
                                                        asm("OFTPSPNX");

/* Housekeeping (KEEPDAYS= in OFTPPRM). Deletes every transfer that
** oftp_spl_purgeable() says may go, in one request of the spool thread,
** so nothing changes a record between the look and the delete (a RETRY
** of a finished send, say). The data sets are never touched. *n gets the
** number deleted; 0, or ERROR with *n those deleted before it.
*/
extern int  oftp_spl_purge(OFTP_SPOOL *sp, long before, unsigned *n)
                                                        asm("OFTPSPPG");

/* The rule, portable (src/oftp#prg.c): finished means DELIVERED,
** CONFIRMED, REJECTED or FAILED; changed is the latest of tqueued, tlast
** and tdone; purgeable is a finished transfer ('T' key) changed before
** `before`.
*/
extern int  oftp_spl_finished(const OFTP_SPREC *r)      asm("OFTPSPFI");
extern long oftp_spl_changed(const OFTP_SPREC *r)       asm("OFTPSPCH");
extern int  oftp_spl_purgeable(const OFTP_SPREC *r, long before)
                                                        asm("OFTPSPPB");
/* A HOLD or RELEASE by partner and name (portable as well): partner "*"
** or "" is any, vfn "" any, "ABC*" a prefix, else the exact name.
*/
extern int  oftp_spl_match(const OFTP_SPREC *r, const char *partner,
                           const char *vfn)             asm("OFTPSPMA");

#define OFTP_SPL_NOTFOUND   (-1)
#define OFTP_SPL_ERROR      (-2)
#define OFTP_SPL_CHANGED    (-3)

#endif /* OFTPSPL_H */
