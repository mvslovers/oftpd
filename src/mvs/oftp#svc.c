/* oftp#svc.c - the cross-memory transport, OFTPD's side: the CSA anchor,
** the OFTPSVC module and the SVC table entry (internals/xmem.md
** sections 5 and 6).
**
** This is piece one of the transport: it installs and it restores, and
** it deliberately does not activate. OFTP_ANC_ACTIVE stays clear, so a
** caller of the SVC number counts itself in, finds the flag off and
** leaves with OFTP_SVC_DOWN without ever claiming a slot --
** asm/oftpsvc.asm tests ANCFLAG X'80' ahead of the claim, which is what
** makes this order safe. Nothing can park in a slot until piece two
** sets the flag, so there is no slot to leak and the drain below is
** trivially zero. The slot thread, the wake ECB, the SSCT and the sweep
** come with that piece.
**
** Every write here is a key-0 write: the anchor, the module's anchor
** word and the SVC table entry. The subpool-241 GETMAIN is issued
** inside the same bracket on purpose -- CSA storage comes out in the
** PSW key of the requester, so allocating in key 8 would hand a
** problem-state client a writable anchor.
*/
#include <stddef.h>
#include <string.h>

#include <ibm/mvs/cvt.h>
#include <ibm/mvs/ihascvt.h>
#include <mvs/apf.h>
#include <mvs/ecb.h>
#include <mvs/link.h>
#include <mvs/storage.h>
#include <mvs/xmem.h>

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
    if (m->anchor != NULL)
        m->anchor->flags &= ~OFTP_ANC_ACTIVE;
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
        m->anchor->flags &= ~OFTP_ANC_ACTIVE;
        /* Bounded drain, 10 s in steps of 0.1 (STIMER BINTVL units).
        ** In piece one the flag was never on, so the only caller that
        ** can be counted here is one inside the few instructions
        ** between the count and the ACTIVE test.
        */
        for (i = 0; i < 100; i++) {
            ECB e = 0;

            if ((left = m->anchor->inflight) == 0)
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
