/* oftp#svcc.c - the cross-memory transport, the CLIENT's side: find
** OFTPD through the SSCT and issue the private SVC
** (internals/xmem.md section 4, steps 1 to 3).
**
** This runs in whatever state and key the calling program has, which
** for the TSO command and the ISPF dialog means problem state, key 8,
** unauthorized. That is the whole point of an SVC here: everything
** that needs authority happens on the other side of it.
**
** **Two key-8 fetches of key-0 CSA are unavoidable here, and they are
** the assumption oftpd#15 measures.** A client has no way to be
** authorized, so it cannot bracket anything: it reads the SSCT chain
** to find OFTPD, and the anchor header to learn the SVC number and
** whether the transport is on. Both live in subpool 241, key 0.
**
** What makes that defensible rather than hopeful is one fact that
** was read and not assumed: libc370's ssct_find() contains no
** MODESET. It takes CVTJESCT, follows JESSSCT and walks SSCTSCTA in
** whatever key its caller is in (src/mvs/subsys/@@ssfind.c), and its
** contract names no authorization requirement -- so libc370 expects
** a problem-state key-8 caller to read the SSCT chain, which is
** key-0 CSA. That says the fetch-protection bit is off for that
** storage, exactly as MVS-PROT-0001 requires for the fetch to be
** permitted.
**
** That is the whole of the evidence: one library's source. It does
** not establish the same for subpool 241 generally, and oftpd#15 is
** the measurement.
**
** The contrast with OFTPD's side is deliberate. There (oftp#svc.c)
** every access is bracketed although it need not be, because OFTPD
** *can* be authorized; here nothing can be, so the client is kept to
** the two smallest reads that let it reach the SVC at all.
*/
#include <stddef.h>
#include <string.h>

#include <mvs/subsys.h>

#include "oftpsvc.h"

/* Issue the private SVC with R1 = A(block).
**
** The number is not known at assembly time, so it rides in R6 and EX
** ORs its low byte into a stored "SVC 0": no storage is modified and
** the sequence stays RENT-safe. The pattern is nsf370's
** (src/nsfreqc.c), which is the one proven against a type-3 entry on
** MVS.
**
** The three outputs go to storage rather than to register operands.
** R15, R0 and R1 are the routine's answer and are therefore clobbered
** by it, so they cannot also be asm outputs -- gcc will not accept a
** register that is both. out[] is reached through a register the SVC
** FLIH restores (R2-R14), which is what makes this safe.
**
** noinline so the two labels are emitted exactly once: a duplicate
** external builds clean under this toolchain and says nothing.
*/
static void __attribute__((noinline))
svc_issue(OFTP_SVCREQ *req, unsigned svcn, unsigned *out)
{
    __asm__ __volatile__(
        "         LR    1,%1\n"            /* R1 = A(request block)   */
        "         LR    6,%2\n"            /* R6 = the SVC number     */
        "         EX    6,OFTPSCS0\n"      /* execute SVC <R6 low>    */
        "         B     OFTPSCSX\n"
        "OFTPSCS0 SVC   0\n"               /* EX target, not modified */
        "OFTPSCSX ST    15,0(,%0)\n"       /* R15: the return code    */
        "         ST    0,4(,%0)\n"        /* R0:  bytes in reply     */
        "         ST    1,8(,%0)\n"        /* R1:  resume index       */
        :
        : "r"(out), "r"(req), "r"(svcn)
        : "0", "1", "6", "15", "memory");
}

int
oftp_svc_call(const char *req, char *reply, unsigned cap, unsigned resume,
              unsigned *outlen, unsigned *outnext)
{
    OFTP_SVCREQ   blk;
    OFTP_ANCHOR  *a;
    SSCT         *ssct;
    unsigned      out[3];
    unsigned      n, svcn;

    if (outlen != NULL)
        *outlen = 0;
    if (outnext != NULL)
        *outnext = 0;
    if (req == NULL || reply == NULL || cap < OFTP_SVC_REPMIN)
        return OFTP_SVC_BADREQ;

    /* A request over REQMAX is refused, never cut: the bytes left would
    ** be another request, and OFTPD would answer that one.
    */
    n = (unsigned)strlen(req);
    if (n == 0 || n > OFTP_SVC_REQMAX)
        return OFTP_SVC_BADREQ;

    /* Discovery. Every one of these answers "OFTPD is not running":
    ** the caller is not meant to tell them apart (xmem.md section 7).
    */
    if ((ssct = ssct_find(OFTP_SVC_SSNAME)) == NULL)
        return OFTP_SVC_DOWN;
    if ((a = (OFTP_ANCHOR *)ssct->ssctsuse) == NULL)
        return OFTP_SVC_DOWN;
    /* The anchor's version, not the request block's: an anchor of
    ** another layout belongs to an OFTPD this client was not built for
    */
    if (memcmp(a->eye, "OFTPANCH", 8) != 0 || a->ver != OFTP_ANC_VER)
        return OFTP_SVC_DOWN;
    if ((a->flags & OFTP_ANC_ACTIVE) == 0)
        return OFTP_SVC_DOWN;

    /* The number is read from the anchor and checked, never trusted
    ** into an EX: a corrupted anchor must not make this issue SVC 13
    ** at a dispatcher.
    */
    svcn = a->svc;
    if (svcn < 200 || svcn > 255)
        return OFTP_SVC_DOWN;

    memset(&blk, 0, sizeof(blk));
    memcpy(blk.eye, "OFTQ", 4);
    blk.ver    = OFTP_SVC_VER;
    blk.reply  = reply;
    blk.cap    = cap;
    blk.resume = resume;
    blk.reqlen = n;
    memcpy(blk.req, req, n);

    out[0] = OFTP_SVC_DOWN;
    out[1] = 0;
    out[2] = 0;
    svc_issue(&blk, svcn, out);

    if (outlen != NULL)
        *outlen = (out[1] > cap) ? cap : out[1];
    if (outnext != NULL)
        *outnext = out[2];
    return (int)out[0];
}
