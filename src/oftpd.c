/* OFTPD - the OFTP monitor (started task)
**
** DD OFTPPRM  the monitor's settings      DD OFTPPTN  the partners
** DD OFTPSPL  the spool, a KSDS KEYS(8 0) RECORDSIZE(512 512)
**
** Operator: F OFTPD,D P | D Q | D S | CALL partner     P OFTPD
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

#include "oftp.h"
#include "oftpmon.h"

static void
cfgmsg(void *ctx, const char *line)
{
    oftp_mon_msg("OFTP010W %s: %s", (const char *)ctx, line);
}

/* Reads one member through its DD; -1 when the DD cannot be opened */
static int
read_member(OFTP_CONFIG *c, const char *dd, int partner)
{
    char name[16], line[256];
    FILE *fp;
    int n = 0;

    snprintf(name, sizeof(name), "DD:%s", dd);
    if ((fp = fopen(name, "r")) == NULL)
        return -1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);

        /* fixed records: columns 73-80 are sequence numbers */
        if (len > 72)
            line[72] = '\0';
        oftp_cfg_line(c, partner, line, ++n, cfgmsg, (void *)dd);
    }
    fclose(fp);
    return 0;
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
    int on, running = 1;

    (void)argc;
    if ((com = __gtcom()) == NULL) {
        oftp_mon_msg("OFTP090E COM AREA NOT AVAILABLE");
        return 8;
    }
    __cibset(5);
    clib_apf_setup(argv[0]);
    oftp_mon_msg("OFTP000I %s STARTING", oftp_version());

    if ((m = calloc(1, sizeof(*m))) == NULL) {
        oftp_mon_msg("OFTP091E NO STORAGE");
        return 8;
    }
    memcpy(m->eye, "OFTPMON ", 8);
    oftp_cfg_defaults(&m->cfg);
    if (read_member(&m->cfg, "OFTPPRM", 0) != 0) {
        oftp_mon_msg("OFTP011E DD OFTPPRM CANNOT BE OPENED");
        return 8;
    }
    if (m->cfg.odetteid[0] == '\0') {
        oftp_mon_msg("OFTP012E OFTPPRM HAS NO ODETTEID");
        return 8;
    }
    if (read_member(&m->cfg, "OFTPPTN", 1) != 0)
        oftp_mon_msg("OFTP013W DD OFTPPTN CANNOT BE OPENED, NO PARTNERS");
    on = oftp_cfg_finish(&m->cfg, cfgmsg, (void *)"OFTPPTN");
    oftp_mon_msg("OFTP014I ODETTE ID %s, %d OF %d PARTNERS ENABLED",
                 m->cfg.odetteid, on, m->cfg.nptn);

    if ((m->spool = oftp_spl_open("OFTPSPL", why, sizeof(why))) == NULL) {
        oftp_mon_msg("OFTP020E SPOOL NOT OPEN: %s", why);
        return 8;
    }
    oftp_mon_recover(m);
    if (oftp_mon_start(m) != 0) {
        oftp_mon_stop(m);
        oftp_spl_close(m->spool);
        return 8;
    }
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
    oftp_mon_stop(m);
    oftp_spl_close(m->spool);
    oftp_mon_msg("OFTP003I OFTPD ENDED");
    free(m);
    return 0;
}
