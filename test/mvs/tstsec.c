/* TSTSEC - a send from a caller without an ACEE, and on a system
** without a security product (MVS, oftpd#54)
**
** Over the SVC, SEND asks whether the caller may read the data set
** (src/mvs/oftp#sec.c). Two cases were read off the code and never run:
**
**   - the caller's address space has no ACEE, so the routine leaves the
**     slot's userid blank;
**   - the system has no security product (CVTRAC is zero).
**
** One program, both systems: what it expects follows from CVTRAC. With a
** security product a caller without a userid is refused (509); without
** one nothing is checked and the send is queued.
**
** **The "no ACEE" call is the real thing.** ASXBSENV of this address
** space is cleared for the length of one SVC and put back, in key 0, so
** OFTPSVC walks the same control blocks it walks for any caller and finds
** none. That needs authority, which is why this job -- unlike TSTSVC --
** must run authorized (SVC 244 as OFTPD does, or APF).
**
** **NEVER RUN THIS ON A STAND.** With NOSEC the call clears CVTRAC of the
** whole system for the length of one SVC, not just of this address
** space. An ESTAE (TERM=YES, so a CANCEL comes here too) puts CVTRAC and
** ASXBSENV back, in key 0, if the call abends or the job is cancelled
** while the SVC waits. The call has no timeout of its own: a hang that
** nobody cancels keeps CVTRAC cleared. The PARM can switch the security
** of a system off: keep the program out of LNKLST and out of SYS2.LINKLIB
** (test/mvs/tstsec.jcl). It is for a throwaway container only.
**
** It needs a running OFTPD with SVC= and a partner, and a sequential
** data set the caller can read (test/mvs/tstsec.jcl). It queues held
** sends and deletes them again; ADMIN= in OFTPPRM must name the userid
** the job runs under, or the clean-up is refused.
**
** PARM='RUN,partner,dsn[,NOSEC|HANG]'
**   NOSEC  the calls without an ACEE also run without CVTRAC
**   HANG   one call with CVTRAC cleared that OFTPD holds for a minute
**          (an OFTPD built with -DOFTP_DEBUG_SLOW): cancel the job while
**          it waits, then run PARM='CHECK'
** PARM='CHECK'
**   fails if CVTRAC is zero, the state a cancelled HANG must not leave
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ibm/mvs/cvt.h>
#include <ibm/mvs/ihaacee.h>
#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/racf.h>
#include <mvs/recovery.h>
#include <mvs/subsys.h>

#include "oftpsvc.h"

#define PSAAOLD  0x224          /* PSA -> ASCB                          */
#define ASCBASXB 0x6C           /* ASCB -> ASXB                         */
#define ASXBSENV 0xC8           /* ASXB -> ACEE                         */

/* The address of this address space's ASXBSENV, or NULL */
static void **
senv_addr(void)
{
    unsigned char *ascb = *(unsigned char **)(unsigned long)PSAAOLD;
    unsigned char *asxb;

    if (ascb == NULL)
        return NULL;
    asxb = *(unsigned char **)(ascb + ASCBASXB);
    return asxb == NULL ? NULL : (void **)(asxb + ASXBSENV);
}

/* What the ESTAE puts back. Static, not reached through the SDWA: the
** exit must work when the system gave it none.
*/
static struct {
    void **senv;
    void  *senv_val;
    CVT   *cvt;
    void  *rac_val;
    int    armed;
} rec;

/* Both values back, in key 0; harmless to run twice */
static int
put_back(void)
{
    unsigned char sk;

    if (!rec.armed)
        return 0;
    if (__super(PSWKEY0, &sk) != 0)
        return -1;
    *rec.senv = rec.senv_val;
    rec.cvt->cvtrac = rec.rac_val;
    __prob(sk, NULL);
    return 0;
}

static void
recover(SDWA *sdwa)
{
    if (put_back() != 0)
        fprintf(stderr, "TSTSEC: CVTRAC / ASXBSENV NOT PUT BACK\n");
    if (sdwa != NULL)
        sdwa->SDWARCDE = SDWACWT;
}

/* One request as a caller without an ACEE, and with `nosec` as on a
** system without a security product: ASXBSENV cleared, and CVTRAC too,
** each put back as soon as the SVC returns, or by the ESTAE if the call
** is left by an abend or a CANCEL. Nothing between the two stores prints
** or allocates. `req` NULL: abend (S0C4) inside the window instead of
** calling.
*/
static int
call_as(int nosec, const char *req, char *buf, unsigned cap, unsigned *len)
{
    unsigned char sk;
    unsigned      next = 0;
    int           rc;

    if ((rec.senv = senv_addr()) == NULL)
        return -1;
    rec.cvt = CVTPTR;
    if (estae(ESTAE_CREATE, recover, NULL) != 0)
        return -3;
    if (__super(PSWKEY0, &sk) != 0) {
        estae(ESTAE_DELETE, NULL, NULL);
        return -2;
    }
    rec.senv_val = *rec.senv;
    rec.rac_val  = rec.cvt->cvtrac;
    rec.armed    = 1;
    *rec.senv = NULL;
    if (nosec)
        rec.cvt->cvtrac = NULL;
    __prob(sk, NULL);

    if (req != NULL)
        rc = oftp_svc_call(req, buf, cap, 0, len, &next);
    else {
        rc = 0;
        *(volatile char *)(unsigned long)0x10 = 1;      /* key 0 PSA */
    }

    rc = put_back() == 0 ? rc : -4;
    rec.armed = 0;
    estae(ESTAE_DELETE, NULL, NULL);
    return rc;
}

static void
trim(char *buf, unsigned len)
{
    unsigned i;

    buf[len] = '\0';
    for (i = 0; i < len; i++)
        if (buf[i] == '\n') {
            buf[i] = '\0';
            return;
        }
}

static int
queued(const char *answer)
{
    return strncmp(answer, "200 ", 4) == 0 || strncmp(answer, "210 ", 4) == 0;
}

/* Does the whole answer to LIST name key? */
static int
lists(int anon, int nosec, const char *key)
{
    char     buf[OFTP_SVC_REPMAX + 1];
    unsigned len = 0, next = 0;
    int      rc;

    rc = anon ? call_as(nosec, "LIST", buf, OFTP_SVC_REPMAX, &len)
              : oftp_svc_call("LIST", buf, OFTP_SVC_REPMAX, 0, &len, &next);
    if (rc != OFTP_SVC_OK)
        return 0;
    buf[len] = '\0';
    return strstr(buf, key) != NULL;
}

/* One request, answered; the first line of the reply in `out` */
static int
ask(int anon, int nosec, const char *req, char *out, size_t cap)
{
    char        buf[OFTP_SVC_REPMAX + 1];
    unsigned    len = 0, next = 0;
    int         rc;

    rc = anon ? call_as(nosec, req, buf, OFTP_SVC_REPMAX, &len)
              : oftp_svc_call(req, buf, OFTP_SVC_REPMAX, 0, &len, &next);
    if (rc != OFTP_SVC_OK) {
        snprintf(out, cap, "(SVC rc %d)", rc);
        return rc;
    }
    trim(buf, len);
    snprintf(out, cap, "%s", buf);
    return rc;
}

static int
abend_call(int *nosec)
{
    return call_as(*nosec, NULL, NULL, 0, NULL);
}

int
main(int argc, char **argv)
{
    char       ans[200], req[OFTP_SVC_REQMAX + 1];
    char       key[9], key2[9];
    char       parm[120];
    char      *partner, *dsn, *mode = "", *p;
    int        nosec, secure, hang, trc;
    void      *senv0, *rac0;
    ACEE      *own;
    CVT       *cvt = CVTPTR;

    printf("=== TSTSEC a send without an ACEE, without a security product ===\n");
    /* a PARM is one string, its items separated by commas */
    snprintf(parm, sizeof(parm), "%s", argc > 1 ? argv[1] : "");
    partner = "PARTNER1";
    dsn     = "";
    if ((p = strchr(parm, ',')) != NULL) {
        *p++ = '\0';
        partner = p;
        if ((p = strchr(p, ',')) != NULL) {
            *p++ = '\0';
            dsn = p;
            if ((p = strchr(p, ',')) != NULL) {
                *p++ = '\0';
                mode = p;
            }
        }
    }
    if (strcmp(parm, "CHECK") == 0) {
        printf("  CVTRAC %p\n", (void *)cvt->cvtrac);
        CHECK(cvt->cvtrac != NULL, "CVTRAC is not zero");
        return mbt_test_summary("TSTSEC");
    }
    if (strcmp(parm, "RUN") != 0) {
        printf("  SKIP: needs a running OFTPD with SVC=, an APF-authorized"
               " STEPLIB and PARM='RUN,partner,dsn[,NOSEC]' (test/mvs/tstsec.jcl)\n");
        return 0;
    }
    hang  = strcmp(mode, "HANG") == 0;
    nosec = strcmp(mode, "NOSEC") == 0 || hang;
    CHECK(dsn[0] != '\0', "a data set to send is given");
    /* Like OFTPD: authorized through the library, or else asked for
    ** through SVC 244 where RAKF permits it (TK5).
    */
    if (__isauth()) {
        printf("  authorized by the library\n");
    } else {
        int arc = clib_apf_setup(argv[0]);

        printf("  clib_apf_setup() %d, authorized %d\n", arc, __isauth());
    }
    CHECK_EQ(__isauth(), 1, "the job IS APF-authorized (it clears ASXBSENV)");
    CHECK(ssct_find(OFTP_SVC_SSNAME) != NULL, "subsystem " OFTP_SVC_SSNAME
          " is installed (OFTPD is running with SVC=)");
    if (dsn[0] == '\0' || !__isauth() || ssct_find(OFTP_SVC_SSNAME) == NULL)
        return mbt_test_summary("TSTSEC");

    /* What OFTPD sees: the system's own CVTRAC, or none when the calls
    ** below clear it (NOSEC) -- a container has RAKF and no image has
    ** been found that boots without it.
    */
    secure = cvt->cvtrac != NULL && !nosec;
    own    = racf_get_acee();
    printf("  CVTRAC %p: %s%s\n", cvt->cvtrac,
           cvt->cvtrac != NULL ? "a security product" : "NO security product",
           nosec ? "; NOSEC: cleared (with ASXBSENV) for each anonymous call"
                 : "");
    printf("  own ACEE %p, userid %.*s\n", (void *)own,
           own != NULL ? own->aceeusrl : 1,
           own != NULL ? own->aceeusri : "-");

    /* HANG: parked in OFTPD with CVTRAC cleared; the operator cancels the
    ** job and PARM='CHECK' then shows what the ESTAE did.
    */
    if (hang) {
        char     hbuf[OFTP_SVC_REPMAX + 1];
        unsigned len = 0;
        int      hrc;

        printf("  HANG: LIST SLOW=60 with CVTRAC cleared; CANCEL this job now\n");
        fflush(stdout);
        hrc = call_as(1, "LIST SLOW=60", hbuf, OFTP_SVC_REPMAX, &len);
        printf("  HANG: not cancelled, the call returned %d\n", hrc);
        CHECK(cvt->cvtrac != NULL, "CVTRAC is back after the call");
        return mbt_test_summary("TSTSEC");
    }

    /* 1. The control: this job's own ACEE, which reads the data set. */
    snprintf(req, sizeof(req), "SEND PARTNER=%s DSN=%s HOLD=Y", partner, dsn);
    ask(0, 0, req, ans, sizeof(ans));
    printf("    own ACEE           : %s\n", ans);
    CHECK(queued(ans), "control: queued for the caller's own userid");
    snprintf(key, sizeof(key), "%.8s", ans + 4);

    /* 2. No ACEE (and, with NOSEC, no security product). */
    key2[0] = '\0';
    ask(1, nosec, req, ans, sizeof(ans));
    printf("    no ACEE            : %s\n", ans);
    if (secure) {
        CHECK(strcmp(ans, "509 not authorized: the caller has no userid")
              == 0, "no ACEE, security product: refused (509)");
    } else {
        CHECK(queued(ans), "no ACEE, no security product: queued");
        if (queued(ans))
            snprintf(key2, sizeof(key2), "%.8s", ans + 4);
    }

    /* 3. DISP=DELETE asks for ALTER, the same rule. */
    snprintf(req, sizeof(req), "SEND PARTNER=%s DSN=%s HOLD=Y DISP=DELETE",
             partner, dsn);
    ask(1, nosec, req, ans, sizeof(ans));
    printf("    no ACEE, DISP=DELETE: %s\n", ans);
    if (secure) {
        CHECK(strncmp(ans, "509 ", 4) == 0,
              "no ACEE, security product: DISP=DELETE refused too");
    } else {
        CHECK(queued(ans), "no ACEE, no security product: DISP=DELETE queued");
        if (queued(ans))
            snprintf(req, sizeof(req), "DELETE KEY=%.8s", ans + 4),
            ask(0, 0, req, ans, sizeof(ans));
    }

    /* 4. What a caller without a userid can then do with transfers.
    ** The control transfer (key, HERC01's, held) is there in both
    ** modes. With a product a blank userid owns nothing: it sees no
    ** transfer and may touch none. Without one there is nobody to tell
    ** apart, so everyone sees and manages everything (oftpd#54).
    */
    ask(1, nosec, "LIST", ans, sizeof(ans));
    printf("    no ACEE, LIST      : %s\n", ans);
    snprintf(req, sizeof(req), "SHOW KEY=%s", key);
    if (secure) {
        CHECK(strcmp(ans, "200 0 transfers") == 0,
              "no ACEE, security product: LIST shows nothing");
        ask(1, nosec, req, ans, sizeof(ans));
        printf("    no ACEE, SHOW ctl  : %s\n", ans);
        CHECK(strcmp(ans, "506 not your transfer") == 0,
              "no ACEE, security product: SHOW of another's transfer refused");
        snprintf(req, sizeof(req), "DELETE KEY=%s", key);
        ask(1, nosec, req, ans, sizeof(ans));
        printf("    no ACEE, DELETE ctl: %s\n", ans);
        CHECK(strcmp(ans, "506 not your transfer") == 0,
              "no ACEE, security product: DELETE of another's transfer refused");
    } else if (key2[0] != '\0') {
        CHECK(lists(1, nosec, key2),
              "no ACEE, no security product: LIST shows its transfer");
        snprintf(req, sizeof(req), "SHOW KEY=%s", key2);
        ask(1, nosec, req, ans, sizeof(ans));
        printf("    no ACEE, SHOW      : %s\n", ans);
        CHECK(strncmp(ans, "100 ", 4) == 0 && strstr(ans, key2) != NULL,
              "no ACEE, no security product: SHOW answers");
        /* held already: 505 says the request passed the owner test */
        snprintf(req, sizeof(req), "HOLD KEY=%s", key2);
        ask(1, nosec, req, ans, sizeof(ans));
        printf("    no ACEE, HOLD      : %s\n", ans);
        CHECK(strncmp(ans, "505 ", 4) == 0,
              "no ACEE, no security product: HOLD is not refused as foreign");
        snprintf(req, sizeof(req), "DELETE KEY=%s", key2);
        ask(1, nosec, req, ans, sizeof(ans));
        printf("    no ACEE, DELETE    : %s\n", ans);
        CHECK(strncmp(ans, "200 ", 4) == 0 && strstr(ans, "deleted") != NULL,
              "no ACEE, no security product: DELETE of its transfer works");
        key2[0] = '\0';
    }

    /* 5. An abend inside the window: the ESTAE puts CVTRAC and ASXBSENV
    ** back. try() takes the abend the ESTAE percolates to it.
    */
    senv0 = *senv_addr();
    rac0  = cvt->cvtrac;
    trc   = try(abend_call, &nosec);
    printf("    abend in the window: try() %08X, CVTRAC %p, ASXBSENV %p\n",
           (unsigned)trc, (void *)cvt->cvtrac, *senv_addr());
    CHECK(trc > 0, "the abend inside the window is seen (S0C4)");
    CHECK(cvt->cvtrac == rac0, "CVTRAC is back after an abend inside the window");
    CHECK(*senv_addr() == senv0, "ASXBSENV is back after an abend inside the window");
    CHECK(racf_get_acee() == own, "the job's own ACEE is found again");

    /* Clean up with the job's own ACEE (ADMIN=) */
    snprintf(req, sizeof(req), "DELETE KEY=%s", key);
    ask(0, 0, req, ans, sizeof(ans));
    printf("    own ACEE, DELETE   : %s\n", ans);
    if (key2[0] != '\0') {
        snprintf(req, sizeof(req), "DELETE KEY=%s", key2);
        ask(0, 0, req, ans, sizeof(ans));
        printf("    own ACEE, DELETE   : %s\n", ans);
    }
    return mbt_test_summary("TSTSEC");
}
