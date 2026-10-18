/* TSTPROT - is a key-8 access to subpool 241 permitted? (MVS, oftpd#15)
**
** The transport brackets every access to the CSA anchor with
** __super(PSWKEY0), reads included. That is correct and it is what
** piece two ships, but it is correct by assumption on one point: the
** client side (src/mvs/oftp#svcc.c) cannot bracket anything, because
** an unauthorized program has no authority to bracket with, and it
** reads the SSCT chain and the anchor header in key 8. Whether that
** is legal has never been measured here.
**
** TPROT answers it without faulting -- it reports in the condition
** code instead of raising a program interruption, which is the whole
** reason #15 chose it over provoking an S0C4 and reading the dump.
**
** **This job must be APF-authorized, and that is not a convenience.**
** TPROT is a privileged instruction: Hercules checks it with
** PRIV_CHECK in its E501 handler (control.c, the `test_protection`
** instruction), so a problem-state caller gets S0C2 and measures
** nothing. The measurement is still the right one, because TPROT's
** SECOND operand supplies the key to test: a supervisor-state program
** asks "would a key-8 access be permitted here", without being in
** key 8 itself.
**
** Gated on PARM='RUN' (test/mvs/tstprot.jcl). **It does not run from
** the TESTLIB**: the library `mbt test --mvs` deploys to is not in
** IEAAPF00, so this job run from there fails its own first check
** every time -- a correct guard and no measurement. It needs a copy
** in an authorized library. TSTSVC is the exact opposite and must
** stay unauthorized; the two want opposite libraries, and each fails
** loudly in the other's rather than passing and proving nothing.
**
** It writes nothing -- no data sets, no system members, no console
** commands. It needs no
** OFTPD either; if one is running it probes that anchor as well, as a
** confirmation on the object the client will really read.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/link.h>
#include <mvs/storage.h>
#include <mvs/subsys.h>

#include "oftpsvc.h"

/* Window and step of the granularity scan, in bytes. 2048 is the step
** because that is the smaller of the two candidate key sizes: a scan
** in 4096 steps could not see a 2 K boundary at all.
*/
#define SCAN_STEP  2048
#define SCAN_HALF  (32 * 1024)

/* TPROT: would an access with `key` be permitted at `addr`?
**
**   0  store permitted (and therefore fetch)
**   1  fetch permitted, store not
**   2  no access: the location is fetch-protected against this key
**   3  the storage is not available (translation exception)
**
** The key goes in bits 24-27 of the second operand's ADDRESS, hence
** key << 4 -- the instruction takes an address there, not a value,
** and nothing is fetched from it.
**
** Supervisor state required; see the file header.
**
** The condition code is read with branches and not with IPM: IPM is
** B222 and as370 does not know it ("Undefined operation code"), which
** is the kind of thing that only shows up when you try. Neither LA
** nor BC alters the condition code, so the ladder below reads what
** TPROT left.
**
** noinline so the label is emitted once -- this is called from half a
** dozen places, and a duplicate label in one CSECT is an assembly
** error at best and the wrong branch target at worst.
*/
static int __attribute__((noinline))
tprot(const void *addr, unsigned key)
{
    int cc;

    __asm__ __volatile__(
        "         TPROT 0(%1),0(%2)\n"
        "         LA    %0,0\n"
        "         BC    8,TPRDONE\n"
        "         LA    %0,1\n"
        "         BC    4,TPRDONE\n"
        "         LA    %0,2\n"
        "         BC    2,TPRDONE\n"
        "         LA    %0,3\n"
        "TPRDONE  DS    0H\n"
        /* "=&r", not "=r": without the early-clobber gcc is free to
        ** put the result in a register it also used for an input,
        ** and it did -- the first build came out as
        ** `TPROT 0(3),0(2)` followed by `LA 2,0`, result and key
        ** sharing r2. It happens to be harmless because TPROT runs
        ** first, which is exactly what makes it the wrong thing to
        ** leave in: it is correct by instruction order, not by
        ** anything the compiler was told.
        */
        : "=&r"(cc)
        : "r"(addr), "r"(key << 4)
        : "cc", "memory");
    return cc;
}

static const char *
ccname(int cc)
{
    switch (cc) {
    case 0:  return "0 store+fetch";
    case 1:  return "1 fetch only";
    case 2:  return "2 no access";
    default: return "3 unavailable";
    }
}

/* Walk a window in SCAN_STEP steps and report where the answer for
** key 8 changes. The alignment of a transition is what settles the
** storage-key granularity: a boundary that is 2 K aligned but NOT 4 K
** aligned can only exist if a key covers 2 K.
**
** Returns the number of transitions seen; 0 means the window happened
** to be uniform and the run measured nothing about granularity.
*/
static int
scan(const char *base, int *odd2k)
{
    const char *p, *from;
    int         prev, cc, n = 0;

    /* **The grid must be aligned to SCAN_STEP, and the first version
    ** was not.** It started at `base - SCAN_HALF` with base an
    ** arbitrary getmain() address, so every probe sat at that address
    ** modulo 2048 -- 0x288 on the first run in the container -- and
    ** the 2K-only test below could not be true for any input
    ** whatsoever. It duly reported "no 2K-only boundary seen", which
    ** was a property of the grid and not of the machine, and the
    ** assertion above it passed while it did so.
    **
    ** Aligned, a transition between two neighbouring probes puts the
    ** key boundary exactly at the later one, because a key can only
    ** change on a multiple of 2048 whichever size holds.
    */
    *odd2k = 0;
    from = (const char *)((unsigned)base & ~(unsigned)(SCAN_STEP - 1));
    from -= SCAN_HALF;
    prev = tprot(from, 8);
    for (p = from + SCAN_STEP; p <= from + 2 * SCAN_HALF; p += SCAN_STEP) {
        cc = tprot(p, 8);
        if (cc != prev) {
            int odd = (((unsigned)p & 0xFFF) == 0x800);

            printf("    %08X  %s -> %s%s\n", (unsigned)p, ccname(prev),
                   ccname(cc), odd ? "   <- 2K boundary, not 4K" : "");
            if (odd)
                (*odd2k)++;
            n++;
            prev = cc;
        }
    }
    return n;
}

int
main(int argc, char **argv)
{
    void    *heap, *csa, *lpa = NULL, *epa = NULL;
    SSCT    *ssct;
    unsigned char savekey;
    unsigned size = 0;
    int      cc_heap, cc_csa, cc_mod = -1, cc_anc = -1;
    int      trans = 0, odd2k = 0, rc;

    printf("=== TSTPROT key-8 access to subpool 241 (oftpd#15) ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs an APF-authorized STEPLIB and PARM='RUN'"
               " (test/mvs/tstprot.jcl)\n");
        return 0;
    }

    /* Not a skip. TPROT is privileged, so an unauthorized run cannot
    ** measure anything, and a job that quietly passes because it was
    ** not allowed to look is worse than no job.
    */
    CHECK(__isauth() != 0, "the job is APF-authorized (TPROT is privileged)");
    if (!__isauth()) {
        printf("    (put the STEPLIB in an APF-authorized library;"
               " an unauthorized TPROT is S0C2, not a wrong answer)\n");
        return mbt_test_summary("TSTPROT");
    }

    heap = malloc(4096);
    CHECK(heap != NULL, "the key-8 control block is allocated");
    if (heap == NULL)
        return mbt_test_summary("TSTPROT");

    if (__super(PSWKEY0, &savekey) != 0) {
        CHECK(0, "supervisor state for TPROT");
        free(heap);
        return mbt_test_summary("TSTPROT");
    }
    csa = getmain(sizeof(OFTP_ANCHOR), 241);
    if (csa != NULL) {
        cc_csa = tprot(csa, 8);
        cc_mod = (__loadhi(OFTP_SVC_MODULE, &lpa, &epa, &size) == 0
                  && epa != NULL) ? tprot(epa, 8) : -1;
    } else {
        cc_csa = -1;
    }
    cc_heap = tprot(heap, 8);
    __prob(savekey, NULL);

    /* The control first: it decides whether anything else here counts.
    ** Ordinary key-8 heap must answer "store permitted" to a key-8
    ** access. Anything else means TPROT is not being read correctly
    ** and every other number in this run is worthless.
    */
    printf("  control, key-8 heap at %08X: CC %s\n",
           (unsigned)heap, ccname(cc_heap));
    CHECK_EQ(cc_heap, 0, "key-8 heap answers CC 0 to a key-8 access");
    if (cc_heap != 0) {
        printf("    (the control failed: this run measured nothing)\n");
        free(heap);
        return mbt_test_summary("TSTPROT");
    }

    CHECK(csa != NULL, "subpool 241 gave us a block");
    if (csa != NULL) {
        printf("  getmain(%u, 241) at %08X: CC %s\n",
               (unsigned)sizeof(OFTP_ANCHOR), (unsigned)csa, ccname(cc_csa));
        /* CC 1 is the answer the transport hopes for: a key-8 program
        ** may read it and may not write it. CC 0 would mean an
        ** unauthorized caller can WRITE the anchor, which is a finding
        ** of its own and a worse one. CC 2 means the client in
        ** oftp#svcc.c cannot read the anchor at all and the discovery
        ** path has to change.
        */
        CHECK(cc_csa == 0 || cc_csa == 1,
              "a key-8 FETCH of subpool 241 is permitted");
        CHECK(cc_csa != 0,
              "and a key-8 STORE into it is not");
    }

    if (cc_mod >= 0)
        printf("  __loadhi(%s) at %08X: CC %s\n",
               OFTP_SVC_MODULE, (unsigned)epa, ccname(cc_mod));
    else
        printf("  __loadhi(%s): not loaded, module probe skipped\n",
               OFTP_SVC_MODULE);

    /* The live anchor, if an OFTPD is up. This is the object the
    ** client really reads, so it confirms the answer above on the
    ** real thing rather than on a block we allocated ourselves.
    */
    if ((ssct = ssct_find(OFTP_SVC_SSNAME)) != NULL
        && ssct->ssctsuse != NULL) {
        if (__super(PSWKEY0, &savekey) == 0) {
            cc_anc = tprot(ssct->ssctsuse, 8);
            __prob(savekey, NULL);
        }
        printf("  OFTPD's live anchor at %08X: CC %s\n",
               (unsigned)ssct->ssctsuse, ccname(cc_anc));
        CHECK_EQ(cc_anc, cc_csa,
                 "the live anchor answers the same as our own block");
    } else {
        printf("  no %s subsystem: live-anchor probe skipped"
               " (start OFTPD with SVC= to include it)\n", OFTP_SVC_SSNAME);
    }

    /* Granularity. A transition that is 2 K aligned and not 4 K
    ** aligned can only exist if a storage key covers 2 K, which is
    ** what the two-ended TPROT check in the SVC routine depends on
    ** (it is sound to 2048 bytes under either value, and the routine
    ** stays inside that -- this says whether the tighter bound is the
    ** real one).
    */
    printf("  scanning %08X +/- %dK in %d-byte steps for key 8"
           " (grid aligned to %d):\n",
           (unsigned)(csa != NULL ? csa : heap), SCAN_HALF / 1024,
           SCAN_STEP, SCAN_STEP);
    if (__super(PSWKEY0, &savekey) == 0) {
        trans = scan((const char *)(csa != NULL ? csa : heap), &odd2k);
        __prob(savekey, NULL);
    }
    /* A window that happens to be uniform measures nothing about
    ** granularity, and that has to read as "did not measure" rather
    ** than as a pass.
    */
    CHECK(trans > 0, "the scan found at least one key transition");
    printf("  %d transition(s), %d of them on a 2K-but-not-4K boundary\n",
           trans, odd2k);
    /* A positive is decisive, an absence is not, and the asymmetry
    ** is the whole of what this scan can say. MVS hands out storage
    ** in pages, so it never creates a boundary at an odd 2 K even on
    ** hardware whose keys are 2 K -- an empty result is therefore
    ** consistent with BOTH candidates and settles neither. Saying
    ** "keys cover 4 K" here would be reading the absence as evidence.
    */
    if (trans > 0) {
        if (odd2k > 0)
            printf("    => storage keys cover 2 K: a boundary at an odd"
                   " 2 K exists, which 4 K keys cannot produce\n");
        else
            printf("    => NOT SETTLED. No boundary at an odd 2 K in this"
                   " window, which is what 4 K keys give AND what 2 K keys"
                   " give when MVS only ever allocates in pages. This scan"
                   " cannot separate them; SSK on one half of a page and"
                   " ISK on the other would.\n");
    }

    if (__super(PSWKEY0, &savekey) == 0) {
        if (lpa != NULL)
            freemain(lpa);
        if (csa != NULL)
            freemain(csa);
        __prob(savekey, NULL);
    }
    free(heap);
    rc = mbt_test_summary("TSTPROT");
    return rc;
}
