/* oftp#svc.c - the cross-memory transport, OFTPD's side: the CSA anchor,
** the OFTPSVC module and the SVC table entry (internals/xmem.md
** sections 5 and 6).
**
** Piece one installs and restores; piece two (2026-10-18) adds the
** SSCT, the activation and the three operations the slot thread runs
** the transport with: take, answer, sweep.
**
** **This file is the whole of OFTPD's key-0 surface, and that is the
** design, not an accident.** Everything above it -- the slot thread,
** the request handlers, the reply formatting -- works on an
** OFTP_SVCJOB in OFTPD's own key-8 storage and never holds a CSA
** address. Two things follow. A handler cannot cause a key-0 store by
** being wrong about a pointer, which matters because the handlers are
** the part of OFTPD that changes. And whether a key-8 READ of subpool
** 241 is legal (oftpd#15) cannot affect the transport's correctness:
** every read of the anchor here is inside the bracket already, so the
** measurement can only ever let a bracket come off, never force one
** on.
**
** Every access to the anchor is therefore a key-0 access: the slot
** states, the in-flight count, the reply bytes, the SVC table entry.
** The subpool-241 GETMAIN is issued inside the same bracket on purpose
** -- CSA storage comes out in the PSW key of the requester, so
** allocating in key 8 would hand a problem-state client a writable
** anchor.
**
** **Who frees a slot owns its in-flight decrement.** The routine's
** exits in asm/oftpsvc.asm decide which side that is, and they were
** read rather than assumed: RETREL (a served caller), FREEDEC (every
** error exit) and STOPDEC (woken with ACTIVE off) all set FREE and
** decrement in the routine, so OFTPD must do neither for those. SWEPT
** -- the routine losing its CS from DONE -- stores nothing and
** decrements nothing, which is exactly the case where OFTPD's sweep
** took the slot and therefore owes the decrement. A second decrement
** would take the count below zero (the BCTR loops do not guard) and
** leave every later P OFTPD draining forever.
*/
#include <stddef.h>
#include <string.h>
#include <time.h>

#include <ibm/mvs/cvt.h>
#include <ibm/mvs/ihascvt.h>
#include <mvs/apf.h>
#include <mvs/ecb.h>
#include <mvs/link.h>
#include <mvs/storage.h>
#include <mvs/subsys.h>
#include <mvs/xmem.h>
#include <s370/atomic.h>

#include "oftpmon.h"
#include "oftpsvc.h"

/* The header and the assembler have to agree on every offset, and off
** MVS they cannot be compared at all: void* is 8 bytes on a 64-bit
** host, which moves every field from slot.ascb on. So the check belongs
** here, where only cc370 compiles.
**
** The sizes are checked at compile time. The field offsets cannot be:
** libc370's offsetof() is the old (size_t)&((T*)0)->m form, which is
** not an integer constant expression, and cc370 has no
** __builtin_offsetof (measured -- it is a gcc 3.4.6 fork and the
** builtin is absent). So they are checked at run time instead, in
** mirror_ok() below, before anything is allocated or installed: a
** field that moved stops the transport with a message naming it rather
** than corrupting a slot in CSA. That is a better place for it than a
** test anyway, because it is the install that depends on it.
*/
typedef char oftp_svc_sizes[
    (sizeof(OFTP_SLOT)   == 2368
  && sizeof(OFTP_ANCHOR) == 64 + OFTP_SVC_NSLOT * 2368
  && OFTP_SVC_NSLOT      == 4
  && OFTP_SVC_REQMAX     == 255
  && OFTP_SVC_REPMAX     == 2048) ? 1 : -1];

/* Each row is one EQU of asm/oftpsvc.asm. Keep the two in step. */
static int
mirror_ok(OFTP_MON *m)
{
    static const struct {
        const char *name;
        unsigned    want;
    } f[] = {
        { "SLSTATE",  0 }, { "SLASCB",   4 }, { "SLASID",   8 },
        { "SLRECB",  12 }, { "SLUSER",  16 }, { "SLREQLEN", 24 },
        { "SLRESUME", 28 }, { "SLCAP",  32 }, { "SLREPLEN", 36 },
        { "SLNEXT",  40 }, { "SLRC",    44 }, { "SLSTAMP", 48 },
        { "SLRPLYPT", 52 }, { "SLREQ",  64 }, { "SLREPLY", 320 },
        { "ANCEYE",   0 }, { "ANCVER",   8 }, { "ANCFLAG", 12 },
        { "ANCSVC",  16 }, { "ANCSASCB", 20 }, { "ANCECB", 24 },
        { "ANCINFL", 28 }, { "ANCNSLOT", 32 }, { "ANCFULL", 36 },
        { "ANCSERVD", 40 }, { "ANCSAVED", 44 }, { "ANCSLOTS", 64 }
    };
    unsigned got[27];
    unsigned i;

    got[0]  = offsetof(OFTP_SLOT, state);
    got[1]  = offsetof(OFTP_SLOT, ascb);
    got[2]  = offsetof(OFTP_SLOT, asid);
    got[3]  = offsetof(OFTP_SLOT, recb);
    got[4]  = offsetof(OFTP_SLOT, user);
    got[5]  = offsetof(OFTP_SLOT, reqlen);
    got[6]  = offsetof(OFTP_SLOT, resume);
    got[7]  = offsetof(OFTP_SLOT, cap);
    got[8]  = offsetof(OFTP_SLOT, replen);
    got[9]  = offsetof(OFTP_SLOT, next);
    got[10] = offsetof(OFTP_SLOT, rc);
    got[11] = offsetof(OFTP_SLOT, stamp);
    got[12] = offsetof(OFTP_SLOT, rplyptr);
    got[13] = offsetof(OFTP_SLOT, req);
    got[14] = offsetof(OFTP_SLOT, reply);
    got[15] = offsetof(OFTP_ANCHOR, eye);
    got[16] = offsetof(OFTP_ANCHOR, ver);
    got[17] = offsetof(OFTP_ANCHOR, flags);
    got[18] = offsetof(OFTP_ANCHOR, svc);
    got[19] = offsetof(OFTP_ANCHOR, ascb);
    got[20] = offsetof(OFTP_ANCHOR, ecb);
    got[21] = offsetof(OFTP_ANCHOR, inflight);
    got[22] = offsetof(OFTP_ANCHOR, nslot);
    got[23] = offsetof(OFTP_ANCHOR, full);
    got[24] = offsetof(OFTP_ANCHOR, served);
    got[25] = offsetof(OFTP_ANCHOR, saved);
    got[26] = offsetof(OFTP_ANCHOR, slot);

    for (i = 0; i < sizeof(f) / sizeof(f[0]); i++)
        if (got[i] != f[i].want) {
            oftp_mon_msg("OFTP034E NO CROSS MEMORY: %s IS %u, OFTPSVC"
                         " EXPECTS %u", f[i].name, got[i], f[i].want);
            return 0;
        }
    (void)m;
    return 1;
}

static SVCTABLE *
svc_table(void)
{
    CVT  *cvt  = CVTPTR;
    SCVT *scvt = (SCVT *)cvt->cvtabend;

    return (SVCTABLE *)scvt->scvtsvct;
}

/* ---- the key-0 primitives ---------------------------------------- */
/* Every one of these must be called with the key-0 bracket already
** held. They are separated out so the bracket is taken once per
** operation rather than once per field, and so that the CS loops --
** which are the protocol, not an optimisation -- exist exactly once.
*/

/* Move a slot from one state to another, atomically. The compare is
** the point: it is how OFTPD and the routine agree on who owns a
** slot. 1 when this side won, 0 when the slot had moved on.
*/
static int
state_cas(OFTP_SLOT *s, unsigned from, unsigned to)
{
    unsigned have = from;

    return __cas((unsigned *)&s->state, &have, to) == 0;
}

/* One caller less inside the routine. Only ever called after this side
** has WON a state_cas() that freed a slot -- see the file header. The
** floor is not defensive tidiness: the routine's own BCTR loops have
** no floor, so a count that ever goes below zero can only be brought
** back by an IPL.
*/
static void
dec_inflight(OFTP_ANCHOR *a)
{
    unsigned have = a->inflight;

    while (have != 0)
        if (__cas((unsigned *)&a->inflight, &have, have - 1) == 0)
            return;
}

/* Wake a parked caller. Guarded because __xmpost() WTOs on a NULL
** ASCB, and an ESTAE has no console budget to spend on that.
*/
static void
wake_caller(OFTP_SLOT *s)
{
    if (s->ascb != NULL)
        __xmpost(s->ascb, (void *)&s->recb, 0);
}

/* Tell every parked caller that there will be no answer. The routine
** tests ANCFLAG before it looks at the state, so with ACTIVE already
** off any wake-up takes its STOPDEC exit -- which frees the slot and
** decrements the count itself. So this must NOT free and must NOT
** decrement; the state store is for the reader of a dump, and for the
** case where the flag is somehow still on.
**
** **A slot can be PENDING before its caller has reached the WAIT**:
** the routine publishes PENDING, posts OFTPD and only then waits. So
** this posts an ECB nobody is waiting on yet, and that is safe only
** because the routine zeroes SLRECB BEFORE it publishes PENDING
** (asm/oftpsvc.asm, the XC ahead of the ST of STPEND). A post that
** arrives first simply makes the WAIT return at once. Reorder those
** two in the routine and this hangs the caller instead.
**
** Called from the stop (repeatedly, through the drain) and from the
** ESTAE. It takes no lock, allocates nothing and issues no message.
*/
static void
stop_parked(OFTP_ANCHOR *a)
{
    unsigned i, n = a->nslot;

    if (n > OFTP_SVC_NSLOT)
        n = OFTP_SVC_NSLOT;
    for (i = 0; i < n; i++) {
        OFTP_SLOT *s = &a->slot[i];

        if (s->state == OFTP_SL_PENDING
            && state_cas(s, OFTP_SL_PENDING, OFTP_SL_STOP))
            wake_caller(s);
    }
}

/* The entry point an unused slot carries: the one shared by the most of
** 200-255. Not zero -- a free slot holds the invalid-SVC routine's
** address, which differs per distribution (00CCC8 on MVS/CE, 017068 on
** TK5, internals/xmem.md section 3) -- and not a constant either, for
** the same reason. Determined exactly as nsf370 does it (its
** src/nsfsx.c, measured on MVS/CE), so a stand this has never run on
** cannot be wrong about what "free" means.
*/
static void *
unused_ep(SVCTABLE *svct)
{
    void    *ep = svct->svcentry[255].svcepa;
    unsigned i, j, best = 0;

    for (i = 200; i <= 255; i++) {
        void    *e = svct->svcentry[i].svcepa;
        unsigned c = 0;

        for (j = 200; j <= 255; j++)
            if (svct->svcentry[j].svcepa == e)
                c++;
        if (c > best) {
            best = c;
            ep   = e;
        }
    }
    return ep;
}

/* The table write itself: no key switch, no message, idempotent. The
** clean stop and the ESTAE share this one encoding of it, because this
** is the step whose absence costs an IPL -- a reserved slot nobody gave
** back makes that SVC number unavailable to every program on the system
** until the next one. m->svc_taken is the flag both callers read
** afterwards to learn whether it happened.
*/
static void
restore_locked(OFTP_MON *m)
{
    if (!m->svc_taken || m->svc_slot == NULL)
        return;
    memcpy(m->svc_slot, m->svc_saved, 8);
    m->svc_taken = 0;
}

/* Restore from anywhere, the ESTAE included. 1 if the slot was given
** back here, 0 if there was nothing to give back, -1 if key 0 was
** unreachable and the slot is still ours. No message: the ESTAE has one
** WTO to spend, so its caller folds this into it.
**
** __super skips the MODESET when the task is already supervisor, but
** __prob MODESETs to problem state unconditionally -- so the plain
** __super/__prob pair would drop a recovery exit that RTM entered
** supervisor into problem state on the way back, a change RTM did not
** ask for. Capture what we found and return the task to it (nsf370's
** ORDERING note, src/nsfsx.c).
*/
int
oftp_svc_restore(OFTP_MON *m)
{
    unsigned char savekey;
    int           was_sup;

    if (m == NULL || !m->svc_taken || m->svc_slot == NULL)
        return 0;
    was_sup = __issup();
    if (__super(PSWKEY0, &savekey) != 0)
        return -1;
    restore_locked(m);
    if (m->anchor != NULL) {
        m->anchor->flags &= ~OFTP_ANC_ACTIVE;
        /* A parked caller WAITs in key 0 in its own address space. If
        ** OFTPD dies without waking it, that task never comes back --
        ** a cancelled TSO user who cannot even be cancelled again.
        ** The restore stays first because it is the step that costs an
        ** IPL; this one costs somebody their session, which is worse
        ** than it sounds and still second.
        */
        stop_parked(m->anchor);
    }
    if (was_sup)
        __super(savekey, NULL);
    else
        __prob(savekey, NULL);
    return 1;
}

/* Reserve the slot. Called in key 0, so it only ever writes. */
static int
reserve(OFTP_MON *m, unsigned n, void *epa)
{
    SVCTABLE *svct    = svc_table();
    SVCENTRY *slot    = &svct->svcentry[n];
    void     *free_ep = unused_ep(svct);
    unsigned  found0, found1;

    memcpy(&found0, slot, 4);
    memcpy(&found1, (char *)slot + 4, 4);
    oftp_mon_msg("OFTP031I SVC %u ENTRY FOUND %08X %08X", n, found0, found1);

    /* "Free" is the modal entry point AND no lock request: the FLIH
    ** takes the locks named at +6 before it enters the routine, and
    ** asm/oftpsvc.asm is written for a type-3 entry that holds none.
    */
    if (slot->svcepa != free_ep || slot->svclock != 0) {
        oftp_mon_msg("OFTP033E SVC %u NOT FREE (EP %08X LOCK %02X, UNUSED EP"
                     " %08X), NO CROSS MEMORY", n, (unsigned)slot->svcepa,
                     (unsigned)slot->svclock, (unsigned)free_ep);
        return -1;
    }

    /* Save first, then claim ownership, then write. An abend between
    ** the flag and the write makes the ESTAE put back the bytes that
    ** are already there, which costs nothing; an abend between the
    ** write and the flag would leave the entry taken with nobody owning
    ** the restore, which costs an IPL.
    **
    ** Only byte +4 is written, as nsf370 does it: SVCTYPE3 (6 in the
    ** top three bits) with svcapf off, because an unauthorized caller
    ** has to be able to reach the routine -- that is the whole point of
    ** an SVC here. Bytes +5 to +7 stay as found and come back from
    ** svc_saved; svclock was checked above, and +5 is unused in OS/VS2.
    */
    memcpy(m->svc_saved, slot, 8);
    m->svc_slot  = slot;
    m->svc_taken = 1;
    slot->svcepa = epa;
    *((unsigned char *)slot + 4) = 0xC0;
    return 0;
}

/* Start: mirror check, anchor, module, patch, slot. 0 when the
** transport is installed, -1 when it is not -- OFTPD runs on with TCP
** either way, so every exit says why and none of them stops the
** monitor.
*/
int
oftp_svc_start(OFTP_MON *m)
{
    unsigned char savekey;
    unsigned      n    = (unsigned)m->cfg.svc;
    unsigned      size = 0;
    void         *lpa = NULL, *epa = NULL;
    int           rc;

    /* No SVC= is a legitimate configuration -- it is how the transport
    ** is turned off -- but it must not be silent. An absent install
    ** that says nothing is indistinguishable from one that worked, and
    ** the first thing anyone does on a new stand is forget the keyword.
    */
    if (m->cfg.svc == 0) {
        oftp_mon_msg("OFTP038I NO SVC= IN OFTPPRM, NO CROSS MEMORY");
        return -1;
    }
    if (!__isauth()) {
        oftp_mon_msg("OFTP035W NO CROSS MEMORY, NOT AUTHORIZED");
        return -1;
    }
    if (!mirror_ok(m))
        return -1;              /* mirror_ok() named the field        */
    if (__super(PSWKEY0, &savekey) != 0) {
        oftp_mon_msg("OFTP034E NO CROSS MEMORY: SUPERVISOR STATE REFUSED");
        return -1;
    }

    /* The anchor carries the slot pool inside it, so this is one
    ** contiguous subpool-241 GETMAIN of about 9.3 KB (section 5).
    */
    m->anchor = (OFTP_ANCHOR *)getmain((unsigned)sizeof(OFTP_ANCHOR), 241);
    if (m->anchor == NULL) {
        __prob(savekey, NULL);
        oftp_mon_msg("OFTP034E NO CROSS MEMORY: NO CSA FOR %u BYTES FROM"
                     " SUBPOOL 241", (unsigned)sizeof(OFTP_ANCHOR));
        return -1;
    }
    memset(m->anchor, 0, sizeof(OFTP_ANCHOR));
    memcpy(m->anchor->eye, "OFTPANCH", 8);
    m->anchor->ver  = OFTP_SVC_VER;
    m->anchor->svc  = n;
    m->anchor->ascb = __ascb(0);
    /* The wake ECB is OFTPD's own key-8 storage and the routine
    ** branch-POSTs it from the caller's address space. Publishing it
    ** here, before ACTIVE can ever be set, is what makes the POST in
    ** the routine unconditional: it tests the address for zero and
    ** takes POSTBAD if it is, so an anchor that is active and has no
    ** ECB would answer every caller OFTP_SVC_DOWN after claiming and
    ** releasing a slot.
    */
    m->anchor->ecb  = (void *)&m->svcwake;
    m->svcwake      = 0;
    /* The slot count is published by the party that allocated the
    ** storage: OFTPSVC is a separate load module and can be stale
    ** against this one, so it bounds its scan by this field and never
    ** by its own EQU (nsf370 ADR-0042).
    */
    m->anchor->nslot = OFTP_SVC_NSLOT;
    /* flags stays 0 -- installed, not active. The memset left every
    ** slot FREE, which is the value the claim's CS compares against.
    */

    rc = __loadhi(OFTP_SVC_MODULE, &lpa, &epa, &size);
    if (rc != 0 || epa == NULL) {
        freemain(m->anchor);
        m->anchor = NULL;
        __prob(savekey, NULL);
        oftp_mon_msg("OFTP034E NO CROSS MEMORY: %s NOT LOADED INTO CSA"
                     " (RC %d)", OFTP_SVC_MODULE, rc);
        return -1;
    }
    m->svc_lpa  = lpa;
    m->svc_epa  = epa;
    m->svc_size = size;
    /* The anchor address goes into the module's own word at EP+4, the
    ** only writable storage the routine has. It reads that word and
    ** never writes it (asm/oftpsvc.asm, OFTANCH).
    */
    *(void **)((char *)epa + OFTP_SVC_ANCH_OFF) = m->anchor;

    if (reserve(m, n, epa) != 0) {
        freemain(lpa);
        freemain(m->anchor);
        m->anchor  = NULL;
        m->svc_lpa = m->svc_epa = NULL;
        m->svc_size = 0;
        __prob(savekey, NULL);
        return -1;
    }
    __prob(savekey, NULL);

    oftp_mon_msg("OFTP030I CROSS MEMORY ON SVC %u, ANCHOR=%08X MODULE=%08X"
                 " (%u BYTES), NOT ACTIVE", n, (unsigned)m->anchor,
                 (unsigned)epa, size);
    return 0;
}

/* ---- the transport in operation ---------------------------------- */
/* Message ids: 030-039 are the install and the teardown, 060-069 the
** transport while it runs. The split is by lifetime, not by file --
** an operator reading a console wants the second group separated from
** the start-up chatter, because every one of them means a caller was
** affected.
*/

static int  ssct_put(OFTP_MON *m);
static void ssct_drop(OFTP_MON *m);

/* Turn the transport on: make the anchor findable, then set the flag.
**
** The LAST step of the start, and the order inside it matters in both
** directions. The SSCT has to be chained before ACTIVE, or a client
** that looked a moment too early concludes OFTPD is not running. And
** the slot thread has to be waiting before either, which is the
** monitor's job -- from the instruction after the flag goes on, any
** program on the system can claim a slot, and a request that parks
** with nobody to answer it waits until the stop.
**
** Without an SSCT the transport is not turned on at all. A client has
** no other way to find the anchor, and leaving the SVC entry live
** without one would serve only somebody who went looking for the
** number.
*/
void
oftp_svc_activate(OFTP_MON *m)
{
    unsigned char savekey;

    if (m == NULL || m->anchor == NULL)
        return;
    if (ssct_put(m) != 0) {
        oftp_mon_msg("OFTP063E SUBSYSTEM %s NOT INSTALLED, CROSS MEMORY"
                     " STAYS OFF", OFTP_SVC_SSNAME);
        return;
    }
    if (__super(PSWKEY0, &savekey) != 0) {
        ssct_drop(m);
        oftp_mon_msg("OFTP034E NO CROSS MEMORY: SUPERVISOR STATE REFUSED");
        return;
    }
    m->anchor->flags |= OFTP_ANC_ACTIVE;
    __prob(savekey, NULL);
    oftp_mon_msg("OFTP039I CROSS MEMORY ACTIVE ON SVC %u, SUBSYSTEM %s",
                 (unsigned)m->cfg.svc, OFTP_SVC_SSNAME);
}

/* Turn it off again. Idempotent, and deliberately separate from
** oftp_svc_stop(): the slot thread has to be joined before the anchor
** is freed, and nothing new may park while that is happening. The
** stop clears the flag a second time, which costs one store.
*/
void
oftp_svc_deactivate(OFTP_MON *m)
{
    unsigned char savekey;

    if (m == NULL || m->anchor == NULL)
        return;
    if (__super(PSWKEY0, &savekey) != 0)
        return;
    m->anchor->flags &= ~OFTP_ANC_ACTIVE;
    __prob(savekey, NULL);
}

/* Take slot i if it is waiting for an answer. Everything the request
** handlers need is copied out here, into OFTPD's own storage, and the
** slot is not touched again until oftp_svc_answer().
**
** The NUL terminators are ours. The routine writes one after the
** request text, but a request block is caller input and the length it
** declared is the only thing worth trusting -- and that only after it
** has been clamped here as well.
*/
int
oftp_svc_take(OFTP_MON *m, unsigned i, OFTP_SVCJOB *j)
{
    unsigned char savekey;
    OFTP_SLOT    *s;
    int           got = 0;

    if (m == NULL || m->anchor == NULL || j == NULL
        || i >= OFTP_SVC_NSLOT || i >= m->anchor->nslot)
        return 0;
    s = &m->anchor->slot[i];
    if (__super(PSWKEY0, &savekey) != 0)
        return 0;
    if (s->state == OFTP_SL_PENDING) {
        unsigned n = s->reqlen;

        if (n > OFTP_SVC_REQMAX)
            n = OFTP_SVC_REQMAX;
        j->slot   = i;
        j->resume = s->resume;
        j->cap    = s->cap;
        memcpy(j->user, s->user, 8);
        memcpy(j->req, s->req, n);
        j->user[8] = '\0';
        j->req[n]  = '\0';
        got = 1;
    }
    __prob(savekey, NULL);
    /* The reply can never exceed the slot's own area, whatever the
    ** caller's buffer is; and the routine answers BADREQ when replen
    ** is above the buffer it declared, so the smaller of the two is
    ** the only usable bound.
    */
    if (got && j->cap > OFTP_SVC_REPMAX)
        j->cap = OFTP_SVC_REPMAX;
    return got;
}

/* Write the answer back and wake the caller. 1 when it was posted, 0
** when the slot was dropped instead -- the caller issues the message
** for that, because this runs in key 0 and a WTO does not belong
** inside the bracket.
**
** The liveness check is the reason the slot records an ASCB as well
** as an ASID: an ASID is reused as soon as the address space that had
** it goes away, and a POST into a reused one wakes a stranger on an
** ECB that now means something else. So the test is not "is the ASID
** in use" but "does the ASID still hold the ASCB that claimed this
** slot". __ascb() answers 0 for a free ASVT entry (it filters on the
** eye catcher), and 0 is as conclusive as a different address.
*/
int
oftp_svc_answer(OFTP_MON *m, unsigned i, const char *reply, unsigned len,
                unsigned next, unsigned rc)
{
    unsigned char savekey;
    OFTP_SLOT    *s;
    void         *owner;
    int           posted = 0;

    if (m == NULL || m->anchor == NULL || i >= OFTP_SVC_NSLOT)
        return 0;
    s = &m->anchor->slot[i];
    if (len > OFTP_SVC_REPMAX)
        len = OFTP_SVC_REPMAX;
    if (__super(PSWKEY0, &savekey) != 0)
        return 0;
    if (len > s->cap)
        len = s->cap;
    /* ASID 0 is checked here and not left to __ascb(): that indexes
    ** asvtenty[asid-1] under an upper-bound test only, so a zero --
    ** which the routine cannot produce, but a corrupted slot can --
    ** would read the word before the table. A slot with no ASID has
    ** no owner to post, which is the same answer as a dead one.
    */
    owner = (s->asid == 0) ? NULL : __ascb(s->asid);
    if (owner == NULL || owner != s->ascb) {
        if (state_cas(s, OFTP_SL_PENDING, OFTP_SL_FREE))
            dec_inflight(m->anchor);
        __prob(savekey, NULL);
        return 0;
    }
    if (len != 0 && reply != NULL)
        memcpy(s->reply, reply, len);
    s->replen = len;
    s->next   = next;
    s->rc     = rc;
    s->stamp  = (unsigned)time(NULL);
    /* Reply, length and rc are stored BEFORE the state goes DONE, and
    ** that order is the routine's contract (xmem.md section 10): it
    ** reads the fields only after it has seen DONE. The CS both
    ** publishes the state and serialises everything above it, which is
    ** more than `volatile` on state would buy -- that pins the store
    ** of state, not the memcpy before it.
    */
    if (state_cas(s, OFTP_SL_PENDING, OFTP_SL_DONE)) {
        wake_caller(s);
        posted = 1;
    }
    __prob(savekey, NULL);
    return posted;
}

/* One pass of the state table in internals/xmem.md section 6.
**
** PENDING is never touched: it is OFTPD's own to-do list, and a slot
** sitting there means the slot thread has not got to it yet.
**
** DONE is never freed on age alone except after OFTP_SW_DONE_SEC, and
** that case covers one thing only: a caller whose task was purged
** while parked (a cancelled TSO command, S322) and that will
** therefore never wake to write COPYOUT. It does NOT cover a slow
** copy-out -- the routine says when it woke, which is what COPYOUT is
** for -- and it must not be shortened into one, because a swapped-out
** address space has been measured stalling for twelve minutes
** (nsf370 ADR-0044) and freeing that slot is a live corruption.
*/
void
oftp_svc_sweep(OFTP_MON *m)
{
    unsigned char savekey;
    unsigned      i, n, now;
    unsigned      stuck = 0, aged = 0;

    if (m == NULL || m->anchor == NULL)
        return;
    n = m->anchor->nslot;
    if (n > OFTP_SVC_NSLOT)
        n = OFTP_SVC_NSLOT;
    now = (unsigned)time(NULL);
    if (__super(PSWKEY0, &savekey) != 0)
        return;
    for (i = 0; i < n; i++) {
        OFTP_SLOT *s  = &m->anchor->slot[i];
        unsigned   st = s->state;

        if (st == OFTP_SL_CLAIMED) {
            m->sw_copyout[i] = 0;
            m->sw_claimed[i]++;
            if (m->sw_claimed[i] >= OFTP_SW_CLAIMED
                && state_cas(s, OFTP_SL_CLAIMED, OFTP_SL_FREE)) {
                dec_inflight(m->anchor);
                m->sw_claimed[i] = 0;
                stuck |= 1u << i;
            }
        } else if (st == OFTP_SL_COPYOUT) {
            m->sw_claimed[i] = 0;
            m->sw_copyout[i]++;
            if (m->sw_copyout[i] >= OFTP_SW_COPYOUT
                && state_cas(s, OFTP_SL_COPYOUT, OFTP_SL_FREE)) {
                dec_inflight(m->anchor);
                m->sw_copyout[i] = 0;
                stuck |= 1u << i;
            }
        } else if (st == OFTP_SL_DONE) {
            m->sw_claimed[i] = 0;
            m->sw_copyout[i] = 0;
            if (s->stamp != 0 && now > s->stamp
                && now - s->stamp > OFTP_SW_DONE_SEC
                && state_cas(s, OFTP_SL_DONE, OFTP_SL_FREE)) {
                dec_inflight(m->anchor);
                aged |= 1u << i;
            }
        } else {
            m->sw_claimed[i] = 0;
            m->sw_copyout[i] = 0;
        }
    }
    __prob(savekey, NULL);

    if (stuck != 0)
        oftp_mon_msg("OFTP060W CROSS MEMORY SWEPT SLOTS X'%02X', A CALLER"
                     " FAULTED INSIDE THE SVC ROUTINE", stuck);
    if (aged != 0)
        oftp_mon_msg("OFTP061W CROSS MEMORY FREED SLOTS X'%02X' UNCOLLECTED"
                     " FOR %u SECONDS", aged, (unsigned)OFTP_SW_DONE_SEC);
}

/* ---- the SSCT, discovery only ------------------------------------ */
/* A client finds the anchor by name instead of by address. No SSVT:
** nothing is dispatched through this, no IEFSSREQ is ever issued
** against it, and the chain entry exists so that ssct_find() has
** something to return (nsf370 ADR-0048 5.1 took the same decision for
** the same reason).
*/
static int
ssct_put(OFTP_MON *m)
{
    unsigned char savekey;
    SSCT         *old, *ssct;
    int           rc;

    if (__super(PSWKEY0, &savekey) != 0)
        return -1;
    /* A previous OFTPD that abended left its SSCT chained. Unchain it
    ** -- by name, which never dereferences SSCTSUSE, so a stale entry
    ** pointing at an anchor we know nothing about is still removable.
    ** It is NOT freed: another address space may be walking the chain
    ** right now, the walk is unserialised on MVS, and 24 bytes of CSA
    ** per abend is the cheaper side of that trade. The anchor it names
    ** stays too (xmem.md section 6, "After an abend").
    */
    if ((old = ssct_find(OFTP_SVC_SSNAME)) != NULL) {
        ssct_remove(old);
        __prob(savekey, NULL);
        oftp_mon_msg("OFTP062W SUBSYSTEM %s WAS STILL INSTALLED FROM AN"
                     " EARLIER RUN, UNCHAINED; ITS CSA IS RETAINED",
                     OFTP_SVC_SSNAME);
        if (__super(PSWKEY0, &savekey) != 0)
            return -1;
    }
    if ((ssct = ssct_new(OFTP_SVC_SSNAME, NULL, m->anchor)) == NULL) {
        __prob(savekey, NULL);
        return -1;
    }
    rc = ssct_install(ssct, NULL);
    if (rc != 0) {
        ssct_free(ssct);
        __prob(savekey, NULL);
        return -1;
    }
    m->ssct = ssct;
    __prob(savekey, NULL);
    return 0;
}

/* Unchain ours. Not freed, for the reason above; the block is 24
** bytes and it is reused by the next start only in the sense that the
** next start allocates a new one.
*/
static void
ssct_drop(OFTP_MON *m)
{
    unsigned char savekey;

    if (m->ssct == NULL)
        return;
    if (__super(PSWKEY0, &savekey) != 0)
        return;
    ssct_remove(m->ssct);
    __prob(savekey, NULL);
    m->ssct = NULL;
}

/* Stop: clear ACTIVE, drain, give the slot back, read it back, and only
** then free. The read-back is the point of the exercise: a restore that
** did not take is indistinguishable from one that did until somebody
** looks, and what it costs is the SVC number until the next IPL.
**
** Nothing is freed unless the drain and the read-back both agree. A
** caller may still be inside the routine, and freeing the module under
** it would turn a leaked 12 KB into a program check in somebody else's
** address space.
*/
void
oftp_svc_stop(OFTP_MON *m)
{
    unsigned char savekey;
    unsigned      n     = (unsigned)m->cfg.svc;
    unsigned      left  = 0;
    unsigned      back0 = 0, back1 = 0;
    int           i, was_ours, ok;

    if (m->anchor == NULL && !m->svc_taken)
        return;
    was_ours = m->svc_taken;

    if (m->anchor != NULL) {
        /* The anchor is key-0 CSA and OFTPD runs key 8, so touching it
        ** needs the bracket. This line is where that was learnt: it
        ** stood here without one and took an S0C4 on the first stop on
        ** m38x (2026-10-18), while the same store inside
        ** oftp_svc_restore() -- which has the bracket -- ran fine from
        ** the ESTAE and gave the SVC entry back.
        */
        if (__super(PSWKEY0, &savekey) == 0) {
            m->anchor->flags &= ~OFTP_ANC_ACTIVE;
            __prob(savekey, NULL);
        }
        /* The SSCT comes out next, and it has to come out before the
        ** anchor is freed at the bottom: a client that finds it a
        ** moment later would read freed CSA. ACTIVE is already off, so
        ** between these two lines a client finds the anchor and is
        ** told the transport is down, which is the right answer.
        */
        ssct_drop(m);
        /* Bounded drain, 10 s in steps of 0.1 (STIMER BINTVL units).
        **
        ** The STOP post goes inside the loop, not once before it, and
        ** that is not belt and braces. Clearing ACTIVE does not stop a
        ** caller that is already past the flag test: it goes on to
        ** claim a slot, publish it and WAIT. Posting once would miss
        ** every one of those, and each costs the drain its ten
        ** seconds and the caller its session. Posting each pass
        ** catches them as they appear, and a slot already STOP is
        ** skipped by the CS.
        **
        ** The read takes the bracket too. Subpool 241 is meant to be
        ** non-fetch-protected, which would make a key-8 read legal --
        ** but that is an assumption nothing here has measured, and the
        ** cost of it being wrong is another abend and another slot.
        ** Key 0 is never held across the wait.
        */
        for (i = 0; i < 100; i++) {
            ECB e = 0;

            if (__super(PSWKEY0, &savekey) != 0)
                break;
            stop_parked(m->anchor);
            left = m->anchor->inflight;
            __prob(savekey, NULL);
            if (left == 0)
                break;
            ecb_timed_wait(&e, 10, 0);
        }
    }

    if (oftp_svc_restore(m) < 0) {
        oftp_mon_msg("OFTP036W SVC %u NOT RESTORED (NO KEY 0), CSA RETAINED",
                     n);
        return;
    }

    /* Read the entry back and compare it against the bytes we saved.
    ** Only meaningful if it was ours in the first place.
    */
    if (was_ours) {
        if (__super(PSWKEY0, &savekey) != 0) {
            oftp_mon_msg("OFTP036W SVC %u NOT RESTORED (NO KEY 0),"
                         " CSA RETAINED", n);
            return;
        }
        {
            SVCENTRY *slot = &svc_table()->svcentry[n];

            memcpy(&back0, slot, 4);
            memcpy(&back1, (char *)slot + 4, 4);
            ok = memcmp(slot, m->svc_saved, 8) == 0;
        }
        __prob(savekey, NULL);

        if (!ok) {
            oftp_mon_msg("OFTP036W SVC %u NOT RESTORED, ENTRY READS %08X"
                         " %08X, CSA RETAINED", n, back0, back1);
            return;
        }
        oftp_mon_msg("OFTP032I SVC %u RESTORED, ENTRY READS %08X %08X",
                     n, back0, back1);
    }

    if (left != 0) {
        oftp_mon_msg("OFTP037W CROSS MEMORY DRAIN TIMED OUT, %u IN FLIGHT,"
                     " CSA RETAINED", left);
        return;
    }

    if (__super(PSWKEY0, &savekey) == 0) {
        if (m->svc_lpa != NULL)
            freemain(m->svc_lpa);
        if (m->anchor != NULL)
            freemain(m->anchor);
        __prob(savekey, NULL);
        m->svc_lpa  = NULL;
        m->svc_epa  = NULL;
        m->svc_size = 0;
        m->anchor   = NULL;
    }
}
