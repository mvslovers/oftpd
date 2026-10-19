#ifndef OFTPSVC_H
#define OFTPSVC_H

/* oftpsvc.h - the cross-memory transport: anchor, slots and the request
** block a client passes to the SVC routine OFTPSVC (internals/xmem.md).
**
** asm/oftpsvc.asm mirrors every offset below in its EQUs, and the two
** are checked in two places for two different reasons. The SIZES are
** a cc370 compile-time assert -- in src/mvs/oftp#svc.c and again in
** test/mvs/tstsvc.c, because the first only builds into OFTPD. The
** OFFSETS cannot be: libc370's offsetof() is the old
** (size_t)&((T*)0)->m form and is not an integer constant expression,
** and cc370 has no __builtin_offsetof. They are checked at run time
** instead, in mirror_ok(), before anything is allocated -- a field
** that moved stops the transport with a message naming it rather than
** corrupting a slot in CSA.
**
** Neither check can be a host test: void* is 8 bytes on a 64-bit
** host, which moves every field from slot.ascb on and makes
** sizeof(OFTP_SLOT) 2376 instead of 2368. See internals/xmem.md
** section 10.
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
#define OFTP_SL_DONE      3             /* answered, not yet collected   */
#define OFTP_SL_STOP      4             /* OFTPD stops: no answer        */
#define OFTP_SL_COPYOUT   5             /* collected: the routine is in
                                        ** the copy-out. Set by OFTPSVC
                                        ** with CS from DONE, so a slot
                                        ** the sweep took is detected.
                                        ** It is what separates "woke
                                        ** and died" from "not woken
                                        ** yet" -- see xmem.md s6     */

/* the routine's R15 (R0 = reply bytes, R1 = resume index for rc 4) */
#define OFTP_SVC_OK       0             /* complete answer               */
#define OFTP_SVC_MORE     4             /* ask again with R1 as resume   */
#define OFTP_SVC_BUSY     8             /* every slot taken              */
#define OFTP_SVC_DOWN     12            /* OFTPD not active or stopping  */
#define OFTP_SVC_BADREQ   16            /* not a request block           */
#define OFTP_SVC_STOPPED  20            /* OFTPD stopped before serving
                                        ** this request: not carried out */

#define OFTP_ANC_ACTIVE   0x80000000U

/* The sweep's clocks (internals/xmem.md section 6). Two of the three
** are counted in sweep passes, not in wall time, and the slot thread
** makes at most one pass a second -- so the pass count IS the clock,
** and a busy OFTPD cannot shorten it by serving more requests. That
** distinction is load-bearing: the CLAIMED window is a handful of
** instructions, but a caller preempted inside it is perfectly
** legitimate, and three passes taken in three milliseconds would free
** a slot out from under it.
**
** DONE is the one that needs wall time, because it covers a caller
** whose task was purged while parked and nothing else -- see the
** sweep in oftp#svc.c.
*/
#define OFTP_SW_CLAIMED   3             /* passes before a claim is dead */
#define OFTP_SW_COPYOUT   2             /* passes; a copy-out is micros  */
#define OFTP_SW_DONE_SEC  300           /* seconds; the purged caller    */

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
    unsigned    rplyptr;                /* 34 caller's reply buffer, set
                                        **    and read by OFTPSVC only;
                                        **    OFTPD must not clear it   */
    char        rsv[8];                 /* 38                            */
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

/* What OFTPD's slot thread takes out of a slot, in OFTPD's own key-8
** storage. Everything the request handlers see comes from here and
** never from CSA: the slot is read once, under the key-0 bracket, and
** the answer is written back the same way. That is what keeps a
** handler -- which knows nothing about storage keys -- out of key-0
** storage entirely, and it is why piece two does not depend on the
** key-8 read #15 measures.
*/
typedef struct oftp_svcjob {
    unsigned    slot;                   /* index in the pool             */
    unsigned    resume;                 /* lines the caller already has  */
    unsigned    cap;                    /* the caller's buffer           */
    char        user[9];                /* NUL-terminated, may be blank  */
    char        req[OFTP_SVC_REQMAX + 1];
} OFTP_SVCJOB;

/* The client side (src/mvs/oftp#svcc.c): find the SSCT, build the
** request block, issue the SVC. Returns the routine's R15
** (OFTP_SVC_*), or OFTP_SVC_DOWN when there is no SSCT to find and
** OFTP_SVC_BADREQ, without issuing the SVC, for a request that is empty
** or longer than OFTP_SVC_REQMAX.
** *outlen gets R0 (bytes placed in reply) and *outnext R1 (the resume
** index to pass back when the answer was OFTP_SVC_MORE).
*/
extern int oftp_svc_call(const char *req, char *reply, unsigned cap,
                         unsigned resume, unsigned *outlen,
                         unsigned *outnext)             asm("OFTPSCAL");

#endif /* OFTPSVC_H */
