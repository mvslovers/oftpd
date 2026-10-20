/* oftp#job.c - the job after receipt: a member to the internal reader
**
** Two passes over the member. The first one checks every record against
** the symbols and the card width with no reader open, the second one
** sends it: a job that is closed half written on the reader is run by
** JES all the same.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mvs/jes2.h>
#include <mvs/rfile.h>

#include "oftpjob.h"

static void
reason(char *why, const char *text, unsigned long n)
{
    if (n > 0)
        snprintf(why, OFTP_JOB_WHY, "%s (RECORD %lu)", text, n);
    else
        snprintf(why, OFTP_JOB_WHY, "%s", text);
}

/* The next record as text in line (blanks off the end, NUL): its length,
** or -1 at the end of the member, -2 for a record past 80 columns.
*/
static int
next_line(RFILE *rf, unsigned char *rec, char *line)
{
    size_t got = 0, off = 0, n;

    if (rread(rf, rec, &got) != 0)
        return -1;
    if (rf->recfm == RFILE_RECFM_V)
        off = got < 4 ? got : 4;            /* the RDW */
    n = got - off;
    while (n > 0 && (rec[off + n - 1] == ' ' || rec[off + n - 1] == '\0'))
        n--;
    if (n > OFTP_SUB_CARD)
        return -2;
    memcpy(line, rec + off, n);
    line[n] = '\0';
    return (int)n;
}

int
oftp_job_submit(const char *job, const OFTP_SUBCTX *x, char *jobid, char *why)
{
    char        name[64];
    RFILE      *rf = NULL;
    unsigned char *rec = NULL;
    VSFILE     *rdr = NULL;
    char        line[OFTP_SUB_CARD + 1];
    char        card[OFTP_SUB_CARD + 1];
    unsigned char id[8];
    unsigned long recs = 0;
    size_t      size;
    int         pass, rc, n;

    jobid[0] = '\0';
    snprintf(name, sizeof(name), "'%s'", job);
    for (pass = 1; pass <= 2; pass++) {
        recs = 0;
        if (ropen(name, 0, &rf) != 0) {
            if (pass == 1) {
                reason(why, "MEMBER NOT FOUND OR NOT READABLE", 0);
                return 1;
            }
            reason(why, "MEMBER NOT READABLE", 0);
            goto fail;
        }
        /* the member can be another one in the second pass: a buffer
        ** for the record length of every open */
        free(rec);
        rec = NULL;
        size = oftp_sub_bufsize(rf->recfm, rf->lrecl);
        if (size == 0) {
            rclose(rf);
            reason(why, "RECORD FORMAT NOT SUPPORTED", 0);
            return 1;
        }
        if ((rec = malloc(size)) == NULL) {
            rclose(rf);
            reason(why, "NO STORAGE", 0);
            return 1;
        }
        if (pass == 2 && (rc = jesiropn(&rdr)) < 0) {
            rclose(rf);
            snprintf(why, OFTP_JOB_WHY, "INTERNAL READER OPEN FAILED (%d)", rc);
            free(rec);
            return 1;
        }
        while ((n = next_line(rf, rec, line)) != -1) {
            recs++;
            if (n == -2) {
                reason(why, "RECORD PAST COLUMN 80", recs);
                goto fail;
            }
            rc = oftp_sub_card(line, x, card);
            if (rc == OFTP_SUB_LONG) {
                reason(why, "LINE TOO LONG AFTER SUBSTITUTION", recs);
                goto fail;
            }
            if (rc == OFTP_SUB_UNSAFE) {
                reason(why, "?VFILE NOT ALLOWED IN JCL", recs);
                goto fail;
            }
            if (pass == 2) {
                if (jesirput(rdr, card) < 0) {
                    reason(why, "INTERNAL READER WRITE FAILED", recs);
                    goto fail;
                }
            }
        }
        rclose(rf);
        rf = NULL;
        if (recs == 0) {
            reason(why, "MEMBER IS EMPTY", 0);
            if (pass == 2)
                goto fail;
            free(rec);
            return 1;
        }
    }
    free(rec);
    rec = NULL;
    memset(id, ' ', sizeof(id));
    rc = jesircl2(rdr, id);
    rdr = NULL;
    if (rc < 0) {
        snprintf(why, OFTP_JOB_WHY, "INTERNAL READER CLOSE FAILED (%d)", rc);
        return 1;
    }
    memcpy(jobid, id, 8);
    jobid[8] = '\0';
    n = 7;
    while (n >= 0 && (jobid[n] == ' ' || jobid[n] == '\0'))
        jobid[n--] = '\0';
    why[0] = '\0';
    return 0;

fail:
    if (rf != NULL)
        rclose(rf);
    if (rdr != NULL)
        jesircls(rdr);
    free(rec);
    return 1;
}
