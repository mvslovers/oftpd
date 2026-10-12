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
    char        rsv1;
    unsigned    lrecl;
    unsigned    attempts;
    int         reas;                   /* last reason code               */
    int         pprc;
    unsigned long recs;
    unsigned long octets;
    long        tqueued, tlast, tnext, tdone;
    char        text[80];               /* last reason, readable          */
} OFTP_SPREC;

typedef struct oftp_spool OFTP_SPOOL;

/* Opens the cluster behind DD ddname and starts the spool thread. An
** empty cluster gets its control record. NULL on failure, *why says.
*/
extern OFTP_SPOOL *oftp_spl_open(const char *ddname, char *why, int whylen)
                                                        asm("OFTPSPOP");
extern void oftp_spl_close(OFTP_SPOOL *sp)              asm("OFTPSPCL");

/* 0 or a negative error. add assigns rec->key. */
extern int  oftp_spl_add(OFTP_SPOOL *sp, OFTP_SPREC *rec) asm("OFTPSPAD");
extern int  oftp_spl_get(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
                                                        asm("OFTPSPGT");
extern int  oftp_spl_put(OFTP_SPOOL *sp, const OFTP_SPREC *rec)
                                                        asm("OFTPSPPT");
extern int  oftp_spl_del(OFTP_SPOOL *sp, const char *key) asm("OFTPSPDL");
/* The first transfer with a key above key ("" for the first of all) */
extern int  oftp_spl_next(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
                                                        asm("OFTPSPNX");

#define OFTP_SPL_NOTFOUND   (-1)
#define OFTP_SPL_ERROR      (-2)

#endif /* OFTPSPL_H */
