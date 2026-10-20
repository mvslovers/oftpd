/* TSTRCVR - REPLACE=Y: what a stop between its two renames leaves (MVS, #44)
**
** The monitor renames the old data set to TMPPREFIX.B<key> and the new one
** to the target name. A stop between the two leaves the old data set under
** the backup name only. oftp_mon_recover() at the next start has to put it
** back; when both names exist it leaves both. Also checks
** oftp_dsf_kind(), which decides what REPLACE=Y may rename.
**
** Needs DD OFTPSPL (a never-loaded KSDS), a PDS and a VSAM cluster
** (test/mvs/tstrcvr.jcl) and PARM='RUN'.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>
#include <mvs/rfile.h>

#include "oftpdsf.h"
#include "oftpmon.h"
#include "oftpspl.h"

#define P       "IBMUSER.OFTPD.TRCVR"

static OFTP_MON mon;
static OFTP_DSOPT opt = { 255, 4096, "", "", 0 };

static void
make_ps(const char *dsn, const char *text)
{
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    unsigned char rec[20];

    remove(dsn);
    memset(&sf, 0, sizeof(sf));
    sf.fmt = 'F';
    sf.lrecl = 20;
    memset(rec, ' ', sizeof(rec));
    memcpy(rec, text, strlen(text));
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_NONE, &opt, &f), 0, "allocate a test data set");
    CHECK_EQ(oftp_dsf_write(f, rec, 20, 1), 0, "write its record");
    CHECK_EQ(oftp_dsf_close(f, 1), 0, "close it");
}

static int
has(const char *dsn, const char *text)
{
    char name[64];
    RFILE *rf;
    char *rec;
    size_t got = 0;
    int ok = 0;

    snprintf(name, sizeof(name), "'%s'", dsn);
    if (ropen(name, 0, &rf) != 0)
        return 0;
    if ((rec = malloc((size_t)rf->lrecl + 4)) != NULL) {
        ok = rread(rf, rec, &got) == 0 && got >= strlen(text)
             && memcmp(rec, text, strlen(text)) == 0;
        free(rec);
    }
    rclose(rf);
    return ok;
}

static OFTP_SPREC
active_recv(OFTP_SPOOL *sp, const char *local)
{
    OFTP_SPREC r;

    memset(&r, 0, sizeof(r));
    r.dir = SPL_RECV;
    r.state = SPL_ACTIVE;
    strcpy(r.partner, "TSTA");
    strcpy(r.dsn, "TSTRCVR.FILE");
    strcpy(r.local, local);
    CHECK_EQ(oftp_spl_add(sp, &r), 0, "an ACTIVE receive in the spool");
    return r;
}

static void
bak(char *out, size_t cap, const OFTP_SPREC *r)
{
    snprintf(out, cap, "%s.B%.7s", mon.cfg.tmpprefix, r->key + 1);
}

int
main(int argc, char **argv)
{
    OFTP_SPOOL *sp;
    OFTP_SPREC r1, r2, r3, g;
    char why[80], b1[45], b2[45], tmp1[45];

    printf("=== TSTRCVR REPLACE recovery ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs DD OFTPSPL and PARM='RUN' (test/mvs/tstrcvr.jcl)\n");
        return 0;
    }

    make_ps(P ".PS", "PS");
    CHECK_EQ(oftp_dsf_kind(P ".PS"), OFTP_DSK_PS, "kind: a sequential data set");
    CHECK_EQ(oftp_dsf_kind(P ".NOSUCH"), OFTP_DSK_NONE, "kind: a name not in the catalog");
    CHECK_EQ(oftp_dsf_kind(P ".PDS"), OFTP_DSK_OTHER, "kind: a PDS");
    CHECK_EQ(oftp_dsf_kind(P ".KSDS"), OFTP_DSK_OTHER, "kind: a VSAM cluster");
    CHECK_EQ(oftp_dsf_kind(P ".PDS(MEM)"), OFTP_DSK_OTHER, "kind: a member");
    remove(P ".PS");

    sp = oftp_spl_open("OFTPSPL", why, sizeof(why));
    CHECK(sp != NULL, "open the spool");
    if (sp == NULL) {
        printf("    (%s)\n", why);
        return mbt_test_summary("TSTRCVR");
    }
    memset(&mon, 0, sizeof(mon));
    mon.spool = sp;
    strcpy(mon.cfg.tmpprefix, P ".TMP");

    /* 1: stopped after the old one went aside: only the backup is left */
    make_ps(P ".T1", "OLD ONE");
    r1 = active_recv(sp, P ".T1");
    bak(b1, sizeof(b1), &r1);
    snprintf(tmp1, sizeof(tmp1), "%s.%.8s", mon.cfg.tmpprefix, r1.key);
    make_ps(tmp1, "HALF RECEIVED");
    remove(b1);
    CHECK_EQ(rename(P ".T1", b1), 0, "1: the old data set goes aside");
    CHECK_EQ(oftp_dsf_kind(P ".T1"), OFTP_DSK_NONE, "1: nothing under the target name");

    /* 2: stopped after both renames: both names exist */
    make_ps(P ".T2", "NEW TWO");
    r2 = active_recv(sp, P ".T2");
    bak(b2, sizeof(b2), &r2);
    make_ps(b2, "OLD TWO");

    /* 3: a receive with no backup: the target is left alone */
    make_ps(P ".T3", "THREE");
    r3 = active_recv(sp, P ".T3");

    oftp_mon_recover(&mon);

    CHECK(has(P ".T1", "OLD ONE"), "1: the old data set is back under its name");
    CHECK_EQ(oftp_dsf_kind(b1), OFTP_DSK_NONE, "1: and the backup name is free");
    CHECK_EQ(oftp_dsf_kind(tmp1), OFTP_DSK_NONE, "1: the half received file is gone");
    CHECK(has(P ".T2", "NEW TWO"), "2: the target is untouched");
    CHECK(has(b2, "OLD TWO"), "2: and so is the backup, for the operator");
    CHECK(has(P ".T3", "THREE"), "3: a receive without backup leaves the target");
    CHECK_EQ(oftp_spl_get(sp, r1.key, &g), OFTP_SPL_NOTFOUND, "1: the record is discarded");
    CHECK_EQ(oftp_spl_get(sp, r2.key, &g), OFTP_SPL_NOTFOUND, "2: the record is discarded");
    CHECK_EQ(oftp_spl_get(sp, r3.key, &g), OFTP_SPL_NOTFOUND, "3: the record is discarded");

    oftp_spl_close(sp);
    remove(P ".T1");
    remove(P ".T2");
    remove(b2);
    remove(P ".T3");
    return mbt_test_summary("TSTRCVR");
}
