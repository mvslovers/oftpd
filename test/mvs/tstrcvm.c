/* TSTRCVM - receive rules against two RUNNING OFTPDs (MVS, #44)
**
** An unauthorized batch program that talks to OFTPD A (the sender, SVC=,
** this job's userid in ADMIN=) and looks at what OFTPD B (the receiver,
** with the rules of test/mvs/tstrcvmd.jcl) made of the files. Gated on
** PARM='RUN n' (test/mvs/tstrcvm.jcl), because it needs both of them.
**
**   RUN 1   the first rule set is loaded at B's start: a file through a
**           pattern rule, one through a date rule, a replace (the second
**           file takes the place of the first), a refused replace
**           (REPLACE=N: SFNA 13, the first stays), a replace of a
**           data set that is in use (SFNA 12, the old one stays), a file no rule
**           names (INPREFIX.<vfn>), and a rule that makes an invalid
**           name (SFNA 01, nothing allocated), and REPLACE=Y onto a PDS
**           and onto a VSAM cluster (SFNA 12, they stay)
**           and the job after receipt (#45): a JOB= file whose job runs
**           with the symbols put in (the file is delivered, the EERP does
**           not wait for the job), one whose line grows past column 71
**           (delivered, no job), one whose member is not there
**           (delivered, no job), one whose member is a load module
**           (RECFM=U: delivered, no job), the edge of the card (a
**           statement of 71 columns runs, one of 72 does not) and the
**           symbol names (?IDX stays, ?ID.X goes)
**   RUN 2   the member was replaced by a second set and B told REFRESH:
**           the new pattern decides, a rule that left the member no
**           longer does
**   RUN 3   the member now has a syntax error and B was told REFRESH
**           again: the second set stays in force
**   RUN 4   B is up again with the SVC (tstrcvmd.jcl, TSTRCVMC) and the
**           spool it kept: the records of the JOB= files say
**           "job submitted JOBnnnnn" and "job not submitted: reason"
**
** What B did is read off the data sets it made; what A saw, off the spool.
** The console messages of B are looked at by hand (tstrcvm.jcl).
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/rfile.h>

#include "oftpsvc.h"

#define D1  "IBMUSER.TSTRCVM.D1"
#define D2  "IBMUSER.TSTRCVM.D2"
#define PARTNER "TSTB"

static char reply[OFTP_SVC_REPMAX];
static unsigned rlen;

static int
ask(const char *req)
{
    int rc;

    memset(reply, 0, sizeof(reply));
    rlen = 0;
    rc = oftp_svc_call(req, reply, sizeof(reply), 0, &rlen, NULL);
    if (rc != OFTP_SVC_OK)
        printf("    %s: transport answered %d\n", req, rc);
    return rc;
}

/* The first line of the reply that starts with prefix (a copy, valid
** until the next call), NULL when there is none
*/
static const char *
find(const char *prefix)
{
    static char line[256];
    unsigned n = (unsigned)strlen(prefix);
    unsigned i = 0;

    while (i < rlen) {
        unsigned e = i;

        while (e < rlen && reply[e] != '\n')
            e++;
        if (e - i >= n && memcmp(reply + i, prefix, n) == 0) {
            snprintf(line, sizeof(line), "%.*s", (int)(e - i), reply + i);
            return line;
        }
        i = e + 1;
    }
    return NULL;
}

/* Queues dsn for B as vfn; the key in key[9], 0 when it was not queued */
static int
send_file(const char *dsn, const char *vfn, char *key)
{
    char req[200];
    const char *l = NULL;

    snprintf(req, sizeof(req), "SEND PARTNER=" PARTNER " DSN=%s VFN=%s", dsn,
             vfn);
    key[0] = '\0';
    if (ask(req) == OFTP_SVC_OK)
        l = find("200 ");
    if (l == NULL) {
        const char *e = find("5");

        printf("    %s: %s\n", req, e != NULL ? e : "no 200");
        return 0;
    }
    memcpy(key, l + 4, 8);
    key[8] = '\0';
    return 1;
}

/* The state of the transfer from the last SHOW reply into st[10]; 0 when
** the reply has none
*/
static int
state_of(char *st)
{
    const char *l = find("100 T");
    char *c;

    if (l == NULL)
        return 0;
    /* "100 T0000001 SEND DELIVERED, partner ..." */
    snprintf(st, 10, "%s", l + 4 + 8 + 1 + 5);
    c = strchr(st, ',');
    if (c != NULL)
        *c = '\0';
    return 1;
}

/* A state the transfer stays in: ended, or waiting for a retry with its
** reason in the reply
*/
static int
settled(const char *st)
{
    if (strcmp(st, "DELIVERED") == 0 || strcmp(st, "REJECTED") == 0
        || strcmp(st, "FAILED") == 0)
        return 1;
    return strcmp(st, "WAITING") == 0 && strstr(reply, "reason") != NULL;
}

/* Waits for the transfer to end or to wait for a retry; the state in
** st[10], the "reason nn" line of SHOW in why[100] ("" when none)
*/
static void
settle(const char *key, char *st, char *why)
{
    char req[40];
    int  i;

    snprintf(req, sizeof(req), "SHOW KEY=%s", key);
    st[0] = why[0] = '\0';
    for (i = 0; i < 90; i++) {
        if (ask(req) == OFTP_SVC_OK && state_of(st) && settled(st))
            break;
        sleep(2);
    }
    if (ask(req) == OFTP_SVC_OK) {
        const char *r = find("100 reason");

        if (r != NULL)
            snprintf(why, 100, "%s", r);
    }
    printf("    %s: %s after about %d s %s\n", key, st, i * 2, why);
}

/* The first record of a data set; 0 when it cannot be opened */
static int
first_line(const char *dsn, char *line, size_t cap)
{
    char    name[64];
    RFILE  *rf;
    size_t  got = 0;
    char   *rec;

    snprintf(name, sizeof(name), "'%s'", dsn);
    line[0] = '\0';
    if (ropen(name, 0, &rf) != 0)
        return 0;
    if ((rec = malloc((size_t)rf->lrecl + 4)) != NULL) {
        if (rread(rf, rec, &got) == 0) {
            if (got >= cap)
                got = cap - 1;
            memcpy(line, rec, got);
            line[got] = '\0';
        }
        free(rec);
    }
    rclose(rf);
    return 1;
}

static int
has(const char *dsn, const char *content)
{
    char line[200];

    if (!first_line(dsn, line, sizeof(line)))
        return 0;
    return strstr(line, content) != NULL;   /* a V record has its RDW */
}

static int
exists(const char *dsn)
{
    char line[200];

    return first_line(dsn, line, sizeof(line));
}

/* send_file and settle in one: 1 when it was delivered */
static int
transfer(const char *dsn, const char *vfn, char *key, char *st, char *why)
{
    if (!send_file(dsn, vfn, key)) {
        strcpy(st, "NOTQUEUED");
        why[0] = '\0';
        return 0;
    }
    settle(key, st, why);
    return strcmp(st, "DELIVERED") == 0;
}

static void
phase1(void)
{
    char key[16];
    char st[16];
    char why[100];
    char dsn[60];
    time_t now = time(NULL);
    struct tm tmv;
    struct tm *tm = localtime_r(&now, &tmv);
    int i;

    /* the pattern rule: DSN=IBMUSER.RCVTEST.%ID.%Q3 */
    CHECK(transfer(D1, "TSTRCV.PAT.ONE", key, st, why),
          "pattern: the file is delivered, the EERP is back");
    CHECK(has("IBMUSER.RCVTEST.TSTA.ONE", "D1 "),
          "pattern: B named it after the rule, with the content sent");
    CHECK(!exists("IBMUSER.OFTPBIN.TSTRCV.PAT.ONE"),
          "pattern: and not after INPREFIX");

    /* the date rule: DSN=IBMUSER.RCVTEST.D%MM%DD.%Q3 */
    snprintf(dsn, sizeof(dsn), "IBMUSER.RCVTEST.D%02d%02d.ONE", tm->tm_mon + 1,
             tm->tm_mday);
    CHECK(transfer(D1, "TSTRCV.DAT.ONE", key, st, why),
          "date: the file is delivered");
    CHECK(has(dsn, "D1 "), "date: the name carries the day of arrival");

    /* REPLACE=Y: DSN=IBMUSER.RCVTEST.REP.%Q3 */
    CHECK(transfer(D1, "TSTRCV.REP.ONE", key, st, why),
          "replace: the first file is delivered");
    CHECK(has("IBMUSER.RCVTEST.REP.ONE", "D1 "), "replace: it is there");
    CHECK(transfer(D2, "TSTRCV.REP.ONE", key, st, why),
          "replace: the second file under the same name is delivered");
    CHECK(has("IBMUSER.RCVTEST.REP.ONE", "D2 "),
          "replace: the data set now holds the second file");

    /* REPLACE=Y of a data set that is in use (the job holds it with
    ** DISP=OLD): it cannot be renamed, so the new file is refused (SFNA 12)
    ** and the old data set is as it was (compared in tstrcvm.jcl)
    */
    CHECK(!transfer(D2, "TSTRCV.REP.BUSY", key, st, why),
          "busy: the file is not delivered");
    CHECK(strstr(why, "reason 12") != NULL,
          "busy: refused with SFNA 12 (the old data set could not be renamed)");
    if (strcmp(st, "WAITING") == 0 && key[0]) {
        char req[40];

        snprintf(req, sizeof(req), "DELETE KEY=%s", key);
        ask(req);
        CHECK(find("200") != NULL, "busy: the waiting send is deleted");
    }

    /* REPLACE=Y onto a name that is a PDS, and one that is a VSAM cluster
    ** (made by tstrcvmd.jcl): a rename of either would damage it, so the
    ** file is refused (SFNA 12); the PDS is compared in tstrcvm.jcl
    */
    CHECK(!transfer(D1, "TSTRCV.PDS.ONE", key, st, why),
          "pds: the file is not delivered");
    CHECK(strstr(why, "reason 12") != NULL,
          "pds: refused with SFNA 12, the PDS is not replaced");
    if (key[0]) {
        char req[40];

        snprintf(req, sizeof(req), "DELETE KEY=%s", key);
        ask(req);
        CHECK(find("200") != NULL, "pds: the waiting send is deleted");
    }
    CHECK(!transfer(D1, "TSTRCV.PDS.TWO", key, st, why),
          "vsam: the file is not delivered");
    CHECK(strstr(why, "reason 12") != NULL,
          "vsam: refused with SFNA 12, the cluster is not replaced");
    if (key[0]) {
        char req[40];

        snprintf(req, sizeof(req), "DELETE KEY=%s", key);
        ask(req);
        CHECK(find("200") != NULL, "vsam: the waiting send is deleted");
    }

    /* REPLACE=N: DSN=IBMUSER.RCVTEST.NOR.%Q3 */
    CHECK(transfer(D1, "TSTRCV.NOR.ONE", key, st, why),
          "no replace: the first file is delivered");
    CHECK(!transfer(D2, "TSTRCV.NOR.ONE", key, st, why),
          "no replace: the second is not");
    CHECK(strstr(why, "reason 13") != NULL,
          "no replace: refused with SFNA 13, by the partner");
    CHECK(has("IBMUSER.RCVTEST.NOR.ONE", "D1 "),
          "no replace: the first file is untouched");
    if (key[0]) {
        char req[40];

        snprintf(req, sizeof(req), "DELETE KEY=%s", key);
        ask(req);
        CHECK(find("200") != NULL, "no replace: the waiting send is deleted");
    }

    /* no rule matches: INPREFIX.<vfn> */
    CHECK(transfer(D1, "TSTRCV.NONE.ONE", key, st, why),
          "no rule: the file is delivered");
    CHECK(has("IBMUSER.OFTPBIN.TSTRCV.NONE.ONE", "D1 "),
          "no rule: it is INPREFIX.<vfn>, as before rules existed");

    /* a rule that makes an invalid name: DSN=IBMUSER.RCVTEST.%Q3, "9X" */
    CHECK(!transfer(D1, "TSTRCV.BAD.9X", key, st, why),
          "invalid name: the file is not delivered");
    CHECK(strcmp(st, "REJECTED") == 0 && strstr(why, "reason 01") != NULL,
          "invalid name: refused with SFNA 01, no retry");
    CHECK(!exists("IBMUSER.RCVTEST.9X"), "invalid name: nothing was made");

    /* the job after receipt: a line that grows past column 71, a member
    ** that is not there, then the job that runs. The file is delivered in
    ** all three cases, the EERP does not wait for the job.
    */
    CHECK(transfer(D1, "TSTRCV.JOB.LONG", key, st, why),
          "job, line too long: the file is delivered all the same");
    CHECK(has("IBMUSER.RCVTEST.JOB.LONG", "D1 "),
          "job, line too long: the data set is there");
    CHECK(transfer(D1, "TSTRCV.JOB.NONE", key, st, why),
          "job, no member: the file is delivered all the same");
    CHECK(has("IBMUSER.RCVTEST.JOB.NONE", "D1 "),
          "job, no member: the data set is there");
    CHECK(transfer(D1, "TSTRCV.JOB.ONE", key, st, why),
          "job: the file is delivered, the EERP is back");
    CHECK(has("IBMUSER.RCVTEST.JOB.ONE", "D1 "), "job: the data set is there");
    for (i = 0; i < 45 && !exists("IBMUSER.RCVTEST.JOBV.TSTRCV.JOB.ONE");
         i++)
        sleep(2);
    printf("    the job ran after about %d s\n", i * 2);
    CHECK(has("IBMUSER.RCVTEST.JOBOUT.TSTA", "D1 "),
          "job: it ran, and ?PFILE and ?ID are in it");
    CHECK(has("IBMUSER.RCVTEST.JOBV.TSTRCV.JOB.ONE", "D1 "),
          "job: and ?VFILE");
    CHECK(!exists("IBMUSER.RCVTEST.JOBOUT.LONG"),
          "job, line too long: no job ran");

    /* a member of RECFM=U (a load module) is no JCL: the file is
    ** delivered, no job
    */
    CHECK(transfer(D1, "TSTRCV.JOB.RECU", key, st, why),
          "job, RECFM=U: the file is delivered all the same");
    CHECK(has("IBMUSER.RCVTEST.JOB.RECU", "D1 "),
          "job, RECFM=U: the data set is there");

    /* the edge of the card, tstrcvmd.jcl: a // statement of 72 columns
    ** (sent first: a job of it would run before the other) is not
    ** submitted, one of 71 with a sequence number behind it runs, and the
    ** symbol names: ?IDX ?PFILE1 ?KEYS ?VFILEX stay, ?ID.X and ?ID-?KEY go
    */
    CHECK(transfer(D1, "TSTRCV.JOB.Q72", key, st, why),
          "job, 72 columns: the file is delivered all the same");
    CHECK(transfer(D1, "TSTRCV.JOB.Q71", key, st, why),
          "job, 71 columns: the file is delivered");
    CHECK(transfer(D1, "TSTRCV.JOB.SYM", key, st, why),
          "job, symbol names: the file is delivered");
    for (i = 0; i < 45 && !exists("IBMUSER.RCVTEST.JOBSYM.DONE"); i++)
        sleep(2);
    printf("    the symbol job ran after about %d s\n", i * 2);
    CHECK(exists("IBMUSER.RCVTEST.JOBQ71.TSTA"),
          "job, 71 columns: it ran, the comma in column 71 is the text's");
    CHECK(!exists("IBMUSER.RCVTEST.JOBQ72.TSTA"),
          "job, 72 columns: no job ran");
    CHECK(has("IBMUSER.RCVTEST.JOBSYM",
              "?IDX ?PFILE1 ?KEYS ?VFILEX TSTA.X TSTA-T"),
          "job, symbol names: other names stay, ?ID.X and ?ID-?KEY go");
}

static void
phase2(void)
{
    char key[16];
    char st[16];
    char why[100];

    CHECK(transfer(D1, "TSTRCV.PAT.TWO", key, st, why),
          "new set: the file is delivered");
    CHECK(has("IBMUSER.RCVTEST.NEW.TWO", "D1 "),
          "new set: the changed pattern decides");
    CHECK(!exists("IBMUSER.RCVTEST.TSTA.TWO"),
          "new set: the old pattern no longer does");
    CHECK(transfer(D1, "TSTRCV.REP.TWO", key, st, why),
          "new set: a file of the rule that left the member is delivered");
    CHECK(has("IBMUSER.OFTPBIN.TSTRCV.REP.TWO", "D1 "),
          "new set: and is named INPREFIX.<vfn>, no rule is left for it");
}

static void
phase3(void)
{
    char key[16];
    char st[16];
    char why[100];

    CHECK(transfer(D1, "TSTRCV.PAT.THREE", key, st, why),
          "after a bad member: the file is delivered");
    CHECK(has("IBMUSER.RCVTEST.NEW.THREE", "D1 "),
          "after a bad member: the second set is still in force");
}

/* The latest transfer in the LIST reply whose line holds local (the spool
** keeps those of earlier runs), its key in key[9]; 0 when there is none
*/
static int
key_of(const char *local, char *key)
{
    unsigned i = 0;
    int found = 0;

    while (i < rlen) {
        unsigned e = i;

        while (e < rlen && reply[e] != '\n')
            e++;
        if (e - i > 12 && memcmp(reply + i, "100 ", 4) == 0) {
            char line[200];

            snprintf(line, sizeof(line), "%.*s", (int)(e - i), reply + i);
            if (strstr(line, local) != NULL) {
                memcpy(key, line + 4, 8);
                key[8] = '\0';
                found = 1;
            }
        }
        i = e + 1;
    }
    return found;
}

/* What B recorded for the file it made as local: the "100 job" line */
static const char *
job_of(const char *local)
{
    char key[9], req[40];
    unsigned next = 0, from = 0;
    int rc, found = 0;

    do {
        memset(reply, 0, sizeof(reply));
        rlen = 0;
        rc = oftp_svc_call("LIST PARTNER=TSTA", reply, sizeof(reply), from,
                           &rlen, &next);
        if (rc != OFTP_SVC_OK && rc != OFTP_SVC_MORE) {
            printf("    LIST PARTNER=TSTA: transport answered %d\n", rc);
            return NULL;
        }
        if (key_of(local, key))
            found = 1;
        from = next;
    } while (rc == OFTP_SVC_MORE);
    if (!found)
        return NULL;
    snprintf(req, sizeof(req), "SHOW KEY=%s", key);
    if (ask(req) != OFTP_SVC_OK)
        return NULL;
    return find("100 job");
}

static void
phase4(void)
{
    const char *j;

    j = job_of("IBMUSER.RCVTEST.JOB.ONE");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strncmp(j, "100 job submitted JOB", 21) == 0,
          "record of the job: submitted, with the JES job id");
    j = job_of("IBMUSER.RCVTEST.JOB.LONG");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strstr(j, "not submitted: LINE TOO LONG") != NULL,
          "record of the long line: not submitted, with the reason");
    j = job_of("IBMUSER.RCVTEST.JOB.NONE");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strstr(j, "not submitted: MEMBER NOT FOUND") != NULL,
          "record of the missing member: not submitted, with the reason");
    j = job_of("IBMUSER.RCVTEST.JOB.RECU");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strstr(j, "not submitted: RECORD FORMAT NOT SUPPORTED")
          != NULL, "record of the load module: not submitted, with the reason");
    j = job_of("IBMUSER.RCVTEST.JOB.Q72");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strstr(j, "not submitted: LINE TOO LONG") != NULL,
          "record of the 72 columns: not submitted, with the reason");
    j = job_of("IBMUSER.RCVTEST.JOB.Q71");
    printf("    %s\n", j != NULL ? j : "(no job line)");
    CHECK(j != NULL && strncmp(j, "100 job submitted JOB", 21) == 0,
          "record of the 71 columns: submitted");
    j = job_of("IBMUSER.RCVTEST.TSTA.ONE");
    CHECK(j == NULL, "a file whose rule has no JOB= has no job line");
}

int
main(int argc, char **argv)
{
    printf("=== TSTRCVM receive rules ===\n");
    if (argc < 3 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs two running OFTPDs and PARM='RUN n'"
               " (test/mvs/tstrcvm.jcl)\n");
        return 0;
    }
    CHECK_EQ(__isauth(), 0, "the client job is NOT APF-authorized");
    if (argv[2][0] == '1')
        phase1();
    else if (argv[2][0] == '2')
        phase2();
    else if (argv[2][0] == '3')
        phase3();
    else if (argv[2][0] == '4')
        phase4();
    else
        printf("  PARM='RUN 1', 'RUN 2', 'RUN 3' or 'RUN 4'\n");
    return mbt_test_summary("TSTRCVM");
}
