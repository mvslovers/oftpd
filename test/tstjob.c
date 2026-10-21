/* TSTJOB - the job after receipt: src/mvs/oftp#job.c against a member and
** an internal reader that are test doubles (test/hostdbl/mvs/).
**
** The doubles can fail a read: rread() of the real library gives 1 for the
** end of the member and for an I/O error alike, so the code cannot tell
** them apart and has to find a short or a different member by counting.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>

#include <mvs/jes2.h>
#include <mvs/rfile.h>

#include "oftpjob.h"

#define MAXREC  8
#define CHECK_STR_EQ(got, want, msg) CHECK(strcmp((got), (want)) == 0, (msg))
#define LRECL   80

/* the member, as every open of it finds it */
static char         mem[2][MAXREC][LRECL + 1];  /* [0] first open, [1] later */
static int          nrec[2];
static int          recfm = RFILE_RECFM_F;
static int          lrecl = LRECL;
static int          open_ok = 1;                /* ropen() succeeds */
static int          err_at = -1;                /* open #2: the read that fails */

static int          opens, reads_open, rdr_opens, rdr_closed_ok, rdr_closed_bad;
static int          cards;
static char         card_put[MAXREC * 2][LRECL + 1];
static int          rdr_token;
static const char  *x_vfile = "VFN";

static void
reset(void)
{
    memset(mem, 0, sizeof(mem));
    nrec[0] = nrec[1] = 0;
    recfm = RFILE_RECFM_F;
    lrecl = LRECL;
    open_ok = 1;
    err_at = -1;
    opens = reads_open = rdr_opens = rdr_closed_ok = rdr_closed_bad = 0;
    cards = 0;
    x_vfile = "VFN";
}

static void
add(int open, const char *text)
{
    snprintf(mem[open][nrec[open]], LRECL + 1, "%-*s", LRECL, text);
    nrec[open]++;
}

static void
add_both(const char *text)
{
    add(0, text);
    add(1, text);
}

int
ropen(const char *fnm, int write, RFILE **fp)
{
    RFILE *rf;

    (void)fnm;
    (void)write;
    if (!open_ok)
        return -1;
    rf = calloc(1, sizeof(*rf));
    rf->recfm = recfm;
    rf->lrecl = lrecl;
    rf->hfile = (void *)(long)(opens < 1 ? 0 : 1);
    opens++;
    reads_open = 0;
    *fp = rf;
    return 0;
}

int
rclose(RFILE *fp)
{
    free(fp);
    return 0;
}

int
rread(RFILE *fp, void *ptr, size_t *read)
{
    int which = (int)(long)fp->hfile;
    int n = reads_open++;

    if (which == 1 && err_at >= 0 && n >= err_at)
        return 1;                       /* an error, looks like the end */
    if (n >= nrec[which])
        return 1;                       /* the end */
    memcpy(ptr, mem[which][n], LRECL);
    *read = LRECL;
    return 0;
}

int
jesiropn(VSFILE **vsfile)
{
    rdr_opens++;
    *vsfile = (VSFILE *)&rdr_token;
    return 0;
}

int
jesirput(VSFILE *vsfile, const char c[80])
{
    (void)vsfile;
    if (cards < MAXREC * 2) {
        memcpy(card_put[cards], c, LRECL);
        card_put[cards][LRECL] = '\0';
    }
    cards++;
    return 0;
}

int
jesircls(VSFILE *vsfile)
{
    (void)vsfile;
    rdr_closed_bad++;
    return 0;
}

int
jesircl2(VSFILE *vsfile, unsigned char jobid[8])
{
    (void)vsfile;
    memcpy(jobid, "JOB00042", 8);
    rdr_closed_ok++;
    return 0;
}


static int
submit(char *id, char *why)
{
    OFTP_SUBCTX x;

    x.pfile = "PROD.IN.DATA";
    x.vfile = x_vfile;
    x.partner = "PARTNER";
    x.key = "T0000001";
    return oftp_job_submit("PROD.JCL(RUN)", &x, id, why);
}

static void
test_ok(void)
{
    char id[OFTP_JOB_ID], why[OFTP_JOB_WHY];

    reset();
    add_both("//RUN JOB");
    add_both("//* ?ID ?KEY");
    add_both("//S EXEC PGM=IEFBR14");
    CHECK_EQ(submit(id, why), 0, "ok: submitted");
    CHECK_STR_EQ(id, "JOB00042", "ok: the job id");
    CHECK_EQ(cards, 3, "ok: every record went to the reader");
    CHECK(strncmp(card_put[1], "//* PARTNER T0000001 ", 21) == 0,
          "ok: the symbols are in");
    CHECK_EQ(rdr_closed_ok, 1, "ok: closed for submit");
    CHECK_EQ(rdr_closed_bad, 0, "ok: not discarded");
}

static void
test_short_second_pass(void)
{
    char id[OFTP_JOB_ID], why[OFTP_JOB_WHY];

    /* a read error in the second pass looks like the end of the member */
    reset();
    add_both("//RUN JOB");
    add_both("//S1 EXEC PGM=IEFBR14");
    add_both("//S2 EXEC PGM=IEFBR14");
    add_both("//S3 EXEC PGM=IEFBR14");
    err_at = 2;
    CHECK_EQ(submit(id, why), 1, "read error in pass 2: not submitted");
    CHECK_STR_EQ(why, "MEMBER CHANGED DURING READ", "read error: the reason");
    CHECK_EQ(rdr_closed_ok, 0, "read error: the job was not closed for run");
    CHECK_EQ(rdr_closed_bad, 1, "read error: the reader was discarded");
    CHECK_STR_EQ(id, "", "read error: no job id");

    /* the error at the very first record: an empty pass 2 */
    reset();
    add_both("//RUN JOB");
    add_both("//S1 EXEC PGM=IEFBR14");
    err_at = 0;
    CHECK_EQ(submit(id, why), 1, "error at record 1: not submitted");
    CHECK_STR_EQ(why, "MEMBER CHANGED DURING READ", "error at record 1: reason");
    CHECK_EQ(rdr_closed_bad, 1, "error at record 1: discarded");
}

static void
test_longer_second_pass(void)
{
    char id[OFTP_JOB_ID], why[OFTP_JOB_WHY];

    reset();
    add(0, "//RUN JOB");
    add(0, "//S1 EXEC PGM=IEFBR14");
    add(1, "//RUN JOB");
    add(1, "//S1 EXEC PGM=IEFBR14");
    add(1, "//S2 EXEC PGM=IEFBR14");
    add(1, "//S3 EXEC PGM=IEFBR14");
    CHECK_EQ(submit(id, why), 1, "grown: not submitted");
    CHECK_STR_EQ(why, "MEMBER CHANGED DURING READ", "grown: the reason");
    CHECK(cards <= 2, "grown: nothing past the first pass went out");
    CHECK_EQ(rdr_closed_bad, 1, "grown: discarded");
}

static void
test_refusals(void)
{
    char id[OFTP_JOB_ID], why[OFTP_JOB_WHY];

    reset();
    open_ok = 0;
    CHECK_EQ(submit(id, why), 1, "no member: not submitted");
    CHECK_STR_EQ(why, "MEMBER NOT FOUND OR NOT READABLE", "no member: reason");
    CHECK_EQ(rdr_opens, 0, "no member: no reader");

    reset();
    CHECK_EQ(submit(id, why), 1, "empty: not submitted");
    CHECK_STR_EQ(why, "MEMBER IS EMPTY", "empty: reason");
    CHECK_EQ(rdr_opens, 0, "empty: no reader");

    reset();
    recfm = RFILE_RECFM_U;
    add_both("//RUN JOB");
    CHECK_EQ(submit(id, why), 1, "RECFM=U: not submitted");
    CHECK_STR_EQ(why, "RECORD FORMAT NOT SUPPORTED", "RECFM=U: reason");
    CHECK_EQ(rdr_opens, 0, "RECFM=U: no reader");

    reset();
    add_both("//RUN JOB");
    add_both("//* ?PFILE ?PFILE ?PFILE ?PFILE ?PFILE ?PFILE");
    CHECK_EQ(submit(id, why), 1, "too long: not submitted");
    CHECK_STR_EQ(why, "LINE TOO LONG AFTER SUBSTITUTION (RECORD 2)",
                 "too long: reason and record");
    CHECK_EQ(rdr_opens, 0, "too long: found before the reader was opened");

    reset();
    add_both("//RUN JOB");
    add_both("//* ?VFILE");
    x_vfile = "A&B";
    CHECK_EQ(submit(id, why), 1, "unsafe ?VFILE: not submitted");
    CHECK_STR_EQ(why, "?VFILE NOT ALLOWED IN JCL (RECORD 2)",
                 "unsafe ?VFILE: reason and record");
    CHECK_EQ(rdr_opens, 0, "unsafe ?VFILE: no reader");
}

int
main(void)
{
    test_ok();
    test_short_second_pass();
    test_longer_second_pass();
    test_refusals();
    return mbt_test_summary("TSTJOB");
}
