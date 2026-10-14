#ifndef OFTPSVC_H
#define OFTPSVC_H

/* oftpsvc.h - the cross-memory transport: anchor, slots and the request
** block a client passes to the SVC routine OFTPSVC (internals/xmem.md).
**
** asm/oftpsvc.asm mirrors every offset below in its EQUs; test/tstsvc.c
** checks them against offsetof(), so a change here that is not made
** there fails the host tests.
*/

#define OFTP_SVC_SSNAME   "OFT1"        /* the SSCT, discovery only      */
#define OFTP_SVC_MODULE   "OFTPSVC"
#define OFTP_SVC_ANCH_OFF 4             /* the patched anchor word       */
#define OFTP_SVC_VER      1             /* anchor and request layout     */
#define OFTP_SVC_NSLOT    4
#define OFTP_SVC_REQMAX   255           /* request line, one MVCK        */
#define OFTP_SVC_REPMAX   2048          /* reply area of a slot          */
#define OFTP_SVC_REPMIN   256           /* smallest reply buffer taken   */

/* slot states */
#define OFTP_SL_FREE      0
#define OFTP_SL_CLAIMED   1             /* the routine is filling it     */
#define OFTP_SL_PENDING   2             /* OFTPD's to answer             */
#define OFTP_SL_DONE      3             /* answered, the routine copies  */
#define OFTP_SL_STOP      4             /* OFTPD stops: no answer        */

/* the routine's R15 (R0 = reply bytes, R1 = resume index for rc 4) */
#define OFTP_SVC_OK       0             /* complete answer               */
#define OFTP_SVC_MORE     4             /* ask again with R1 as resume   */
#define OFTP_SVC_BUSY     8             /* every slot taken              */
#define OFTP_SVC_DOWN     12            /* OFTPD not active or stopping  */
#define OFTP_SVC_BADREQ   16            /* not a request block           */

#define OFTP_ANC_ACTIVE   0x80000000U

typedef struct oftp_slot {
    volatile unsigned state;            /* 00 OFTP_SL_*                  */
    void       *ascb;                   /* 04 owner, from the ASCB       */
    unsigned    asid;                   /* 08                            */
    unsigned    recb;                   /* 0C reply ECB, key 0           */
    char        user[8];                /* 10 ACEEUSER, blank if none    */
    unsigned    reqlen;                 /* 18                            */
    unsigned    resume;                 /* 1C lines already received     */
    unsigned    cap;                    /* 20 caller's reply buffer      */
    unsigned    replen;                 /* 24 bytes in reply[]           */
    unsigned    next;                   /* 28 resume index, rc MORE      */
    unsigned    rc;                     /* 2C OFTP_SVC_*                 */
    unsigned    stamp;                  /* 30 OFTPD's time of the answer */
    char        rsv[12];                /* 34                            */
    char        req[256];               /* 40 the request line           */
    char        reply[OFTP_SVC_REPMAX]; /* 140 lines, each ending '\n'   */
} OFTP_SLOT;                            /* 940                           */

typedef struct oftp_anchor {
    char        eye[8];                 /* 00 "OFTPANCH"                 */
    unsigned    ver;                    /* 08 OFTP_SVC_VER               */
    volatile unsigned flags;            /* 0C OFTP_ANC_ACTIVE            */
    unsigned    svc;                    /* 10 the SVC number             */
    void       *ascb;                   /* 14 OFTPD's ASCB               */
    void       *ecb;                    /* 18 OFTPD's wake ECB, key 8    */
    volatile unsigned inflight;         /* 1C callers inside the routine */
    unsigned    nslot;                  /* 20                            */
    volatile unsigned full;             /* 24 answered BUSY              */
    volatile unsigned served;           /* 28 answers given              */
    unsigned char saved[8];             /* 2C the SVC entry taken over   */
    char        rsv[12];                /* 34                            */
    OFTP_SLOT   slot[OFTP_SVC_NSLOT];   /* 40                            */
} OFTP_ANCHOR;

/* The block a client passes in R1, in its own storage */
typedef struct oftp_svcreq {
    char        eye[4];                 /* 00 "OFTQ"                     */
    unsigned    ver;                    /* 04 OFTP_SVC_VER               */
    char       *reply;                  /* 08 reply buffer               */
    unsigned    cap;                    /* 0C its size, >= REPMIN        */
    unsigned    resume;                 /* 10 0, or R1 of an rc MORE     */
    unsigned    reqlen;                 /* 14 1..REQMAX                  */
    char        req[OFTP_SVC_REQMAX + 1]; /* 18                          */
} OFTP_SVCREQ;

#endif /* OFTPSVC_H */
