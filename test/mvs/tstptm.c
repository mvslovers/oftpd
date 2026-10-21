/* TSTPTM - maintaining the partner table at run time (MVS)
**
** An unauthorized batch program against RUNNING OFTPDs, through the SVC
** as every utility does. Needs two OFTPDs that know each other, which is
** why it is gated on PARM='RUN' (test/mvs/tstptm.jcl).
**
**   PARM='RUN A host port dsn'   against the sending OFTPD (ODETTEID
**                                O0000000000TSTA, this job's userid in
**                                ADMIN=): adds the partner TSTPTM that
**                                is the other OFTPD (O0000000000TSTB),
**                                changes it, sends the data set dsn to
**                                it, tries to delete it while the
**                                transfer is in the spool
**   PARM='RUN B'                 after OFTPD A was stopped and started
**                                again: the partner is still there, as
**                                changed; the transfer is deleted, then
**                                the partner is
**   PARM='RUN P1'                the two partners of the OFTPPTN member:
**                                one cannot be deleted, one is changed
**                                (it then overrides the member) and
**                                deleted again (the member's entry is
**                                back)
**   PARM='RUN P2'                after the member lost GONEPTN and
**                                REFRESH: GONEPTN is gone, PARMPTN stays
**   PARM='RUN C'                 against an OFTPD started with this job's
**                                userid NOT in ADMIN=: ADD, CHANGE,
**                                DELETE and REFRESH are refused (507),
**                                SHOW and PARTNERS are not, but SHOW
**                                keeps OURID and ADDRESS to itself
**
** The partner TSTPTM is in no parmlib member: it exists only because of
** the commands, so its being there after the restart is the proof that
** a change survives one. Once the gate is given, a missing OFTPD is a
** failure and not a skip.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mbtcheck.h>
#include <mvs/apf.h>

#include "oftpsvc.h"

#define NAME    "TSTPTM"

static char reply[OFTP_SVC_REPMAX];
static unsigned rlen;

/* One request; the reply is left in reply[] and rlen. Returns the
** transport's code.
*/
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

/* The first line of the reply that starts with prefix and holds text
** (NULL: any), as a pointer into reply[]; NULL when there is none
*/
static const char *
find(const char *prefix, const char *text)
{
    unsigned n = (unsigned)strlen(prefix), i = 0;

    while (i < rlen) {
        unsigned e = i;
        char     save;
        int      hit = 0;

        while (e < rlen && reply[e] != '\n')
            e++;
        if (e - i >= n && memcmp(reply + i, prefix, n) == 0) {
            save = reply[e];
            reply[e] = '\0';
            hit = text == NULL || strstr(reply + i, text) != NULL;
            reply[e] = save;
            if (hit)
                return reply + i;
        }
        i = e + 1;
    }
    return NULL;
}

/* The request answered, and its final line starting with code */
static int
answers(const char *req, const char *code)
{
    int ok = ask(req) == OFTP_SVC_OK && find(code, NULL) != NULL;

    if (!ok) {
        const char *last = find("2", NULL);

        if (last == NULL)
            last = find("4", NULL);
        if (last == NULL)
            last = find("5", NULL);
        printf("    %s: wanted %s, got: %.60s\n", req, code,
               last ? last : "(nothing)");
    }
    return ok;
}

/* The final line of the reply, without its code, fits the ISPF message
** line (cut at column 79) for an 8-character partner name (this one has
** 6) and a count of 5 digits (this one has 1)
*/
static int
fits_dialog(void)
{
    const char *l = find("5", NULL);

    if (l == NULL || strlen(l) < 4)
        return 0;
    printf("    %s\n", l);
    return strlen(l + 4) + (8 - (sizeof(NAME) - 1)) + 4 <= 78;
}

static int
show_of(const char *name, const char *text)
{
    char req[64];

    snprintf(req, sizeof req, "PARTNER SHOW PARTNER=%s", name);
    return ask(req) == OFTP_SVC_OK && find("100 ", text) != NULL;
}

static int
show_has(const char *text)
{
    return show_of(NAME, text);
}

/* The key of the newest transfer of the partner into key[9], and its
** state; 0 when there is none
*/
static int
transfer(char *key, char *state)
{
    const char *l;
    int         n;

    if (ask("LIST PARTNER=" NAME) != OFTP_SVC_OK)
        return 0;
    l = find("100 T", NULL);
    if (l == NULL)
        return 0;
    memcpy(key, l + 4, 8);                  /* key 0-7, state 14-22 */
    key[8] = '\0';
    memcpy(state, l + 4 + 14, 9);
    state[9] = '\0';
    for (n = 8; n > 0 && state[n] == ' '; n--)
        state[n] = '\0';
    return 1;
}

/* A KEY= that is not a transfer's: none of the transfer verbs may answer
** with, or change, a partner record (it holds the passwords)
*/
static void
no_partner_key(const char *key)
{
    static const char *const verbs[] = { "SHOW", "DELETE", "HOLD", "RELEASE",
                                         "RETRY" };
    char req[64];
    int  i;

    for (i = 0; i < 5; i++) {
        snprintf(req, sizeof(req), "%s KEY=%s", verbs[i], key);
        CHECK(answers(req, "504"), "a partner key is no transfer");
        CHECK(strstr(reply, "PASSWORD") == NULL && strstr(reply, "TSTB") == NULL
              && strstr(reply, "PARTNER=") == NULL,
              "and the answer has no partner text");
    }
}

static int
phase_a(const char *host, const char *port, const char *dsn)
{
    char req[256], key[16], state[16];
    int  i;

    CHECK(ask("PARTNER SHOW PARTNER=" NAME) == OFTP_SVC_OK
          && find("501", NULL) != NULL,
          "the partner is not there before the test (remove it first)");

    /* 1. add, disabled and with a port nothing listens on */
    snprintf(req, sizeof(req),
             "PARTNER ADD PARTNER=" NAME " ODETTEID=O0000000000TSTB "
             "PASSWORD=TSTB OURPASSWORD=TSTA HOST=%s PORT=1 ENABLED=N", host);
    CHECK(answers(req, "200"), "ADD takes the partner");
    CHECK(find("210", NULL) == NULL && strstr(reply, "WARNING") == NULL,
          "a partner with a PASSWORD gets no warning");
    CHECK(answers("PARTNERS", "200"), "PARTNERS answers");
    CHECK(find("100 " NAME, NULL) != NULL, "and lists it");
    CHECK(show_has("PORT=1 "), "SHOW reads the port back");
    CHECK(show_has("ENABLED=N"), "and that it is disabled");
    CHECK(show_has("PASSWORD=SET OURPASSWORD=SET"),
          "and that both passwords are set, without telling them");
    CHECK(show_has("PASSWORD=") && strstr(reply, "PASSWORD=TST") == NULL,
          "the passwords themselves are never in the answer");
    CHECK(show_has("kept by command"), "and that the partner is kept by command");

    /* 1a. the record just written is in the spool, and no transfer verb
    ** can read, change or delete it */
    no_partner_key("P0000001");
    no_partner_key("P0000002");
    no_partner_key("00000000");
    CHECK(show_has("PASSWORD=SET OURPASSWORD=SET"),
          "the partner is as it was after those");

    /* 2. refusals that change nothing */
    CHECK(answers(req, "509"), "ADD of the same name is refused: it exists");
    CHECK(answers("PARTNER ADD PARTNER=" NAME "2 ODETTEID=O0000000000TSTB "
                  "BOGUS=1", "508"), "an unknown keyword is refused");
    CHECK(answers("PARTNER CHANGE PARTNER=NOSUCHPT PORT=5", "501"),
          "CHANGE of a partner that is not there is refused");
    CHECK(answers("PARTNER DELETE PARTNER=NOSUCHPT", "501"),
          "so is DELETE");
    CHECK(answers("PARTNER CHANGE PARTNER=" NAME " LEVEL=3", "508"),
          "a level that is none is refused");
    CHECK(show_has("PORT=1 "), "and the partner is as it was");

    /* 2a. a partner without a PASSWORD is open to any peer that knows its
    ** Odette ID: it is added all the same, with a warning (#129)
    */
    snprintf(req, sizeof(req),
             "PARTNER ADD PARTNER=" NAME "W ODETTEID=O0000000000TSTW "
             "HOST=%s PORT=1 CALL=N", host);
    CHECK(answers(req, "210") && find("100 ", "without a PASSWORD") != NULL,
          "ADD of an enabled partner without PASSWORD says so");
    CHECK(show_of(NAME "W", "PASSWORD=NONE") && show_of(NAME "W", "ENABLED=Y"),
          "and it is added: the warning is no refusal");
    CHECK(answers("PARTNER CHANGE PARTNER=" NAME "W PORT=2", "210"),
          "a CHANGE that leaves it without PASSWORD says so again");
    CHECK(answers("PARTNER CHANGE PARTNER=" NAME "W PASSWORD=SECRETW", "200")
          && strstr(reply, "WARNING") == NULL && strstr(reply, "SECRETW") == NULL,
          "with a PASSWORD set there is no warning");
    snprintf(req, sizeof(req),
             "PARTNER ADD PARTNER=" NAME "X ODETTEID=O0000000000TSTX "
             "HOST=%s PORT=1 CALL=N ENABLED=N", host);
    CHECK(answers(req, "200") && strstr(reply, "WARNING") == NULL,
          "a disabled partner is open to nobody: no warning");
    CHECK(answers("PARTNER CHANGE PARTNER=" NAME "X ENABLED=Y", "210"),
          "until it is enabled");
    CHECK(answers("PARTNER DELETE PARTNER=" NAME "W", "200")
          && answers("PARTNER DELETE PARTNER=" NAME "X", "200"),
          "both are deleted again");

    /* 3. a disabled partner takes no transfer */
    snprintf(req, sizeof(req), "SEND PARTNER=" NAME " DSN=%s", dsn);
    CHECK(answers(req, "501"), "nothing is queued for a disabled partner");

    /* 4. REFRESH reads the parmlib layer only */
    CHECK(answers("REFRESH", "200"), "REFRESH is done");
    CHECK(show_has("ENABLED=N"), "and the partner kept by command is still there");

    /* 5. change: the right port, enabled */
    snprintf(req, sizeof(req),
             "PARTNER CHANGE PARTNER=" NAME " PORT=%s ENABLED=Y", port);
    CHECK(answers(req, "200"), "CHANGE takes the new port and enables it");
    snprintf(req, sizeof(req), "PORT=%s ", port);
    CHECK(show_has(req), "SHOW reads the new port");
    CHECK(show_has("ENABLED=Y"), "and that it is enabled");
    CHECK(show_has("PASSWORD=SET"), "the passwords were not touched");
    CHECK(answers("PARTNER CHANGE PARTNER=" NAME " ENABLED=Y", "200")
          && find("200", "unchanged") != NULL,
          "a change that changes nothing is told, not refused");

    /* 6. send a file to it, without a restart in between */
    snprintf(req, sizeof(req), "SEND PARTNER=" NAME " DSN=%s", dsn);
    CHECK(answers(req, "200"), "SEND to the partner is queued");
    key[0] = '\0';
    state[0] = '\0';
    for (i = 0; i < 60; i++) {
        if (transfer(key, state)
            && (strcmp(state, "DELIVERED") == 0 || strcmp(state, "FAILED") == 0
                || strcmp(state, "REJECTED") == 0))
            break;
        sleep(2);
    }
    printf("    transfer %s: %s after %d s\n", key, state, i * 2);
    CHECK(strcmp(state, "DELIVERED") == 0,
          "the file arrives and the EERP comes back");

    /* 7. the transfer is in the spool: the partner is not deleted */
    CHECK(answers("PARTNER DELETE PARTNER=" NAME, "512"),
          "DELETE is refused while the spool holds a transfer");
    CHECK(fits_dialog(), "and its text fits the ISPF message line");
    CHECK(show_has("PARTNER=" NAME), "the partner is still there");
    CHECK(show_has("ENABLED=N"), "disabled, so that nothing new comes");
    return 0;
}

/* The partners of OFTPPTN: PARMPTN and GONEPTN, both with port 1 */
static int
phase_p1(void)
{
    CHECK(show_of("PARMPTN", "PORT=1 ") && show_of("PARMPTN", "source: OFTPPTN"),
          "PARMPTN comes from the member");
    CHECK(show_of("PARMPTN", "OURID=O0000000000TSTA")
          && show_of("PARMPTN", "ADDRESS=127.0.0.1 "),
          "an administrator sees OURID and ADDRESS");
    CHECK(answers("PARTNER SHOW PARTNER=GONEPTN", "200"),
          "so does GONEPTN");
    CHECK(answers("PARTNER DELETE PARTNER=PARMPTN", "513"),
          "a partner that is only in the member cannot be deleted");
    CHECK(answers("PARTNER CHANGE PARTNER=PARMPTN PORT=7 ENABLED=N", "200"),
          "but it can be changed: that makes a record that overrides it");
    CHECK(show_of("PARMPTN", "PORT=7 ") && show_of("PARMPTN", "ENABLED=N")
          && show_of("PARMPTN", "overrides OFTPPTN"),
          "SHOW reads the change back and says it overrides the member");
    CHECK(answers("REFRESH", "200"), "REFRESH is done");
    CHECK(show_of("PARMPTN", "PORT=7 ") && show_of("PARMPTN", "ENABLED=N"),
          "and leaves the changed partner alone");
    CHECK(answers("PARTNER DELETE PARTNER=PARMPTN", "200")
          && find("200", "entry is back") != NULL,
          "DELETE takes the record away: the member's entry is back");
    CHECK(show_of("PARMPTN", "PORT=1 ") && show_of("PARMPTN", "ENABLED=Y")
          && show_of("PARMPTN", "source: OFTPPTN"),
          "as the member has it");
    return 0;
}

static int
phase_p2(void)
{
    CHECK(answers("REFRESH", "200"), "REFRESH is done");
    CHECK(answers("PARTNER SHOW PARTNER=GONEPTN", "501"),
          "a partner that left the member and has nothing in the spool is gone");
    CHECK(answers("PARTNER SHOW PARTNER=PARMPTN", "200"),
          "the one that is still in it is not");
    return 0;
}

static int
phase_c(void)
{
    CHECK(answers("PARTNER ADD PARTNER=" NAME " ODETTEID=O0000000000TSTB",
                  "507"), "ADD needs ADMIN=");
    CHECK(answers("PARTNER CHANGE PARTNER=PARMPTN PORT=9", "507"),
          "so does CHANGE");
    CHECK(answers("PARTNER DELETE PARTNER=PARMPTN", "507"), "and DELETE");
    CHECK(answers("REFRESH", "507"), "and REFRESH");
    CHECK(answers("PARTNER SHOW PARTNER=" NAME, "501"),
          "nothing was added");
    CHECK(answers("PARTNER SHOW PARTNER=PARMPTN", "200")
          && show_of("PARMPTN", "PORT=1 "),
          "nor changed: SHOW is open to everyone");
    CHECK(answers("PARTNERS", "200"), "and so is PARTNERS");
    /* ... but OURID and ADDRESS are for an administrator (#129) */
    CHECK(show_of("PARMPTN", "ODETTEID=O0000000000PARM")
          && show_of("PARMPTN", "OURID=-") && show_of("PARMPTN", "ADDRESS=- "),
          "SHOW gives a caller who is no administrator '-' for OURID/ADDRESS");
    CHECK(strstr(reply, "OURID=O") == NULL && strstr(reply, "127.0.0.1") == NULL
          && strstr(reply, "PORT=1 ") != NULL,
          "and the values are not anywhere in the answer");
    return 0;
}

static int
phase_b(void)
{
    char key[16], state[16], req[64];

    CHECK(answers("PARTNER SHOW PARTNER=" NAME, "200"),
          "after the restart the partner is still there");
    CHECK(show_has("ODETTEID=O0000000000TSTB"), "with its Odette ID");
    CHECK(show_has("ENABLED=N"), "as the refused DELETE and TSTPM2 left it");
    CHECK(show_has("SESSIONS=2"), "with the sessions TSTPM2 changed");
    CHECK(show_has("PASSWORD=SET OURPASSWORD=SET"), "with its passwords");
    CHECK(answers("PARTNER DELETE PARTNER=" NAME, "512"),
          "DELETE is still refused: the transfer survived as well");

    CHECK(transfer(key, state) && strcmp(state, "DELIVERED") == 0,
          "the delivered transfer is in the spool");
    snprintf(req, sizeof(req), "DELETE KEY=%s", key);
    CHECK(answers(req, "200"), "it is deleted");
    CHECK(answers("PARTNER DELETE PARTNER=" NAME, "200"),
          "and then the partner is deleted");
    CHECK(answers("PARTNER SHOW PARTNER=" NAME, "501"), "it is gone");
    CHECK(answers("PARTNERS", "200") && find("100 " NAME, NULL) == NULL,
          "and not listed");
    CHECK(answers("PARTNER DELETE PARTNER=" NAME, "501"), "deleted once only");
    return 0;
}

int
main(int argc, char **argv)
{
    printf("=== TSTPTM partner maintenance ===\n");
    if (argc < 3 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs two running OFTPDs and PARM='RUN A host port dsn'"
               " (test/mvs/tstptm.jcl)\n");
        return 0;
    }
    CHECK_EQ(__isauth(), 0, "the client job is NOT APF-authorized");
    if (argv[2][0] == 'A' && argc >= 6)
        phase_a(argv[3], argv[4], argv[5]);
    else if (argv[2][0] == 'B')
        phase_b();
    else if (strcmp(argv[2], "P1") == 0)
        phase_p1();
    else if (strcmp(argv[2], "P2") == 0)
        phase_p2();
    else if (argv[2][0] == 'C')
        phase_c();
    else
        CHECK(0, "PARM is RUN A host port dsn, RUN B, RUN P1, RUN P2 or RUN C");
    return mbt_test_summary("TSTPTM");
}
