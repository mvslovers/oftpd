/* OFTPD - the OFTP monitor (started task)
**
** DD OFTPPRM  the monitor's settings      DD OFTPPTN  the partners
** DD OFTPSPL  the spool, a KSDS KEYS(8 0) RECORDSIZE(512 512)
**
** Operator: F OFTPD,D P | D Q | D S | D M | CALL partner | REFRESH     P OFTPD
**
** The startup order follows FTPD: console set-up while main is the only
** task, then APF, then the banner. The monitor block is a main() local:
** OFTPD is AC(1) and reentrant and holds no writable static data.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mvs/apf.h>
#include <mvs/console.h>
#include <mvs/crt.h>
#include <mvs/ecb.h>
#include <mvs/recovery.h>

#include "oftp.h"
#include "oftpmon.h"

static void
cfgmsg(void *ctx, const char *line)
{
    oftp_mon_msg("OFTP010W %s: %s", (const char *)ctx, line);
}

/* The ESTAE (TERM=YES, so a CANCEL comes here too). A socket outlives
** the program unless it is closed, and a listener left open holds its
** port until Hercules is restarted; so close them all, then percolate
** for the dump. SDWAPARM is the {routine, udata} pair estae() keeps,
** not udata itself (ufsd#49). One message: in an S222 only the first and
** the last WTO are sure to reach the console. The message waits for no
** lock (the abend may have come inside a log write), so it goes to the
** console when the log is busy.
*/
static void
recover(SDWA *sdwa)
{
    void **param;
    OFTP_MON *m;

    if (sdwa == NULL)
        return;
    param = (void **)sdwa->SDWAPARM;
    m = param != NULL ? (OFTP_MON *)param[1] : NULL;
    if (m != NULL && memcmp(m->eye, "OFTPMON ", 8) == 0) {
        /* Read before the call: oftp_mon_abend() gives the SVC entry
        ** back as its first act, so afterwards svc_taken alone cannot
        ** tell "restored" from "never taken" -- and that is exactly the
        ** difference an operator needs, because an entry left reserved
        ** costs the SVC number until the next IPL.
        */
        int had = m->svc_taken;
        int n   = oftp_mon_abend(m);

        oftp_mon_msg_nowait("OFTP098E OFTPD ABEND, %d SOCKETS CLOSED, SVC %s",
                            n, !had ? "NOT TAKEN"
                               : m->svc_taken ? "NOT RESTORED" : "RESTORED");
    } else
        oftp_mon_msg_nowait("OFTP098E OFTPD ABEND, MONITOR BLOCK NOT FOUND");
    sdwa->SDWARCDE = SDWACWT;
}

static void
upcase(char *s)
{
    for (; *s; s++)
        if ((*s >= 'a' && *s <= 'i') || (*s >= 'j' && *s <= 'r')
            || (*s >= 's' && *s <= 'z'))
            *s = (char)(*s - 'a' + 'A');
}

int
main(int argc, char **argv)
{
    OFTP_MON *m;
    COM *com;
    CIB *cib;
    char why[80];
    int on, running = 1, apf_entry, apf_rc, rc = 8, stae;

    (void)argc;
    if ((com = __gtcom()) == NULL) {
        oftp_mon_msg("OFTP090E COM AREA NOT AVAILABLE");
        return 8;
    }
    __cibset(5);
    if ((m = calloc(1, sizeof(*m))) == NULL) {
        oftp_mon_msg("OFTP091E NO STORAGE");
        return 8;
    }
    memcpy(m->eye, "OFTPMON ", 8);
    /* the log before the first message that belongs in it */
    oftp_mon_logopen(m);
    /* Authorized at entry means an APF library; clib_apf_setup() erases
    ** the difference, so measure first. Nothing needs it yet (RACINIT and
    ** cross-memory come later): report the route and carry on. */
    apf_entry = __isauth();
    apf_rc = clib_apf_setup(argv[0]);
    oftp_mon_msg("OFTP000I %s STARTING", oftp_version());
    if (apf_entry)
        oftp_mon_msg("OFTP004P AUTHORIZED BY LIBRARY");
    else if (apf_rc == 0 && __isauth())
        oftp_mon_msg("OFTP004P AUTHORIZED BY SVC");
    else
        oftp_mon_msg("OFTP005W NOT AUTHORIZED, RC=%d, CONTINUING", apf_rc);

    if ((stae = estae(ESTAE_CREATE, recover, m) == 0) == 0)
        oftp_mon_msg("OFTP006W NO ABEND RECOVERY (ESTAE FAILED)");
    oftp_cfg_defaults(&m->cfg);
    if (oftp_mon_load(&m->cfg, "OFTPPRM", 0) != 0) {
        oftp_mon_msg("OFTP011E DD OFTPPRM CANNOT BE OPENED");
        goto out;
    }
    if (m->cfg.odetteid[0] == '\0') {
        oftp_mon_msg("OFTP012E OFTPPRM HAS NO ODETTEID");
        goto out;
    }
    if (oftp_mon_load(&m->cfg, "OFTPPTN", 1) != 0)
        oftp_mon_msg("OFTP013W DD OFTPPTN CANNOT BE OPENED, NO PARTNERS");
    on = oftp_cfg_finish(&m->cfg, cfgmsg, (void *)"OFTPPTN");
    oftp_mon_msg("OFTP014P ODETTE ID %s, %d OF %d PARTNERS ENABLED",
                 m->cfg.odetteid, on, m->cfg.nptn);

    if ((m->spool = oftp_spl_open("OFTPSPL", why, sizeof(why))) == NULL) {
        oftp_mon_msg("OFTP020E SPOOL NOT OPEN: %s", why);
        goto out;
    }
    if (oftp_spl_verified(m->spool))
        oftp_mon_msg("OFTP021W SPOOL WAS NOT CLOSED (OPEN RC %d), VERIFIED",
                     oftp_spl_verified(m->spool));
    oftp_mon_recover(m);
    if (oftp_mon_start(m) != 0) {
        oftp_mon_stop(m);
        oftp_spl_close(m->spool);
        goto out;
    }
    /* The transport last, after the monitor is up: it reports its own
    ** outcome, and OFTPD runs on for its partners and the operator when
    ** it does not install.
    **
    ** The install's return code IS tested now, because what follows
    ** turns the thing on -- the slot thread first, then the SSCT and
    ** ACTIVE (oftp_mon_svc_start). Without an install there is no
    ** anchor and the thread would wait on an ECB nobody can reach.
    */
    if (oftp_svc_start(m) == 0)
        oftp_mon_svc_start(m);
    oftp_mon_msg("OFTP001I OFTPD READY");

    while (running) {
        ECB e = 0;

        while ((cib = __cibget()) != NULL) {
            if (cib->cibverb == CIBSTOP)
                running = 0;
            else if (cib->cibverb == CIBMODFY) {
                char cmd[80];
                unsigned n = cib->cibdatln < sizeof(cmd) - 1
                             ? cib->cibdatln : sizeof(cmd) - 1;

                memcpy(cmd, cib->cibdata, n);
                cmd[n] = '\0';
                upcase(cmd);
                oftp_mon_command(m, cmd);
            }
            __cibdel(cib);
        }
        if (running)
            ecb_timed_wait(&e, 100, 0);
    }

    oftp_mon_msg("OFTP002I OFTPD STOPPING");
    /* The transport first on the way down, so a caller of the SVC gets
    ** "stopped" at once and the entry is back in the table before the
    ** sessions are waited for.
    **
    ** Two steps, and the order is load-bearing: oftp_mon_svc_stop()
    ** turns the transport off and joins the slot thread, and only
    ** then may oftp_svc_stop() drain and free the anchor -- a thread
    ** still looking at it would be reading freed CSA.
    */
    oftp_mon_svc_stop(m);
    oftp_svc_stop(m);
    oftp_mon_stop(m);
    oftp_spl_close(m->spool);
    oftp_mon_msg("OFTP003I OFTPD ENDED");
    rc = 0;
out:
    /* the ESTAE points at m: delete it before m goes */
    if (stae)
        estae(ESTAE_DELETE, NULL, NULL);
    oftp_mon_logclose(m);
    free(m);
    return rc;
}
