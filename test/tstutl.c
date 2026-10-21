/* TSTUTL - the OFTP utility's own parts: operands, the echo of SYSIN, the
** messages it prints. src/oftp.c is included with main renamed and the
** transport (oftp_cli_request) stubbed; POSIX, so host only.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mbtcheck.h>

#define main oftp_util_main
#include "../src/oftp.c"
#undef main

static char lastreq[512];
static int  nreq;

int
oftp_cli_request(const char *req, OFTP_CLILINE line, void *ctx, char *final,
                 int cap, int *code)
{
    (void)line;
    (void)ctx;
    (void)code;
    snprintf(lastreq, sizeof(lastreq), "%s", req);
    nreq++;
    snprintf(final, (size_t)cap, "done");
    return 0;
}

/* One command given as the words of a blank-separated string; what it
** printed goes to buf. Returns its RC.
*/
static int
run(const char *text, char *buf, size_t cap)
{
    char copy[512];
    char *w[20];
    char *save = NULL;
    FILE *f = tmpfile();
    size_t n;
    int rc;
    int argc = 0;

    buf[0] = '\0';
    if (f == NULL)
        return -1;
    snprintf(copy, sizeof(copy), "%s", text);
    for (w[argc] = strtok_r(copy, " ", &save); w[argc] != NULL && argc < 19; )
        w[++argc] = strtok_r(NULL, " ", &save);
    rc = command(argc, w, f);
    rewind(f);
    n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    return rc;
}

/* main() with SYSIN from a file. Off MVS its output file is a plain file
** named *PUTLINE in the current directory, so it runs in a directory of
** its own.
*/
static int
batch(const char *input, char *buf, size_t cap)
{
    char dir[] = "tstutlXXXXXX";    /* under the build directory */
    char here[512];
    char path[600];
    FILE *f;
    size_t n = 0;
    int rc;

    buf[0] = '\0';
    if (getcwd(here, sizeof(here)) == NULL || mkdtemp(dir) == NULL
        || chdir(dir) != 0)
        return -1;
    f = fopen("in", "w");
    if (f == NULL || fputs(input, f) == EOF || fclose(f) != 0)
        return -1;
    if (freopen("in", "r", stdin) == NULL)
        return -1;
    rc = oftp_util_main(1, (char *[]){ "OFTP", NULL });
    f = fopen("*PUTLINE", "r");
    if (f != NULL) {
        n = fread(buf, 1, cap - 1, f);
        fclose(f);
    }
    buf[n] = '\0';
    remove("in");
    remove("*PUTLINE");
    if (chdir(here) != 0)
        return -1;
    snprintf(path, sizeof(path), "%s", dir);
    rmdir(path);
    return rc;
}

static int
longest_line(const char *s)
{
    int best = 0;
    int len = 0;

    for (; *s; s++) {
        if (*s == '\n')
            len = 0;
        else if (++len > best)
            best = len;
    }
    return best;
}

int
main(void)
{
    char out[4096];
    char big[200];
    char cmd[512];
    int rc;

    printf("=== TSTUTL the OFTP utility ===\n");

    /* a keyword without its value is not a second partner name (#113):
    ** OURPASS after the name was read as PARTNER=OURPASS, and the answer
    ** was "a partner cannot be renamed"
    */
    nreq = 0;
    rc = run("PARTNER CHANGE ACME OURPASS", out, sizeof(out));
    CHECK_EQ(rc, 8, "a keyword without value: an error in the command");
    CHECK_EQ(nreq, 0, "and nothing is sent");
    CHECK(strstr(out, "OFTP007E OPERAND 2 HAS NO VALUE") != NULL,
          "the message names the operand's place");
    CHECK(strstr(out, "KEYWORD(VALUE)") != NULL, "and says how to write it");
    CHECK(strstr(out, "OURPASS") == NULL,
          "but not the word: it may be a password");

    /* a flag on a PARTNER command is a flag, not a name */
    nreq = 0;
    rc = run("PARTNER ADD ACME ODETTEID(O0013000000ACME) XLATE", out,
             sizeof(out));
    CHECK_EQ(rc, 0, "a bare XLATE on ADD is taken");
    CHECK(strstr(lastreq, "PARTNER=ACME") != NULL
          && strstr(lastreq, "XLATE=Y") != NULL, "as XLATE=Y");
    CHECK(strstr(lastreq, "PARTNER=XLATE") == NULL, "and not as a name");

    /* the name once is enough, whichever way it is written */
    rc = run("PARTNER CHANGE PARTNER(ACME) PORT(4000)", out, sizeof(out));
    CHECK(rc == 0 && strstr(lastreq, "PARTNER=ACME PORT=4000") != NULL,
          "PARTNER(name) written out is still one name");

    /* OFTP003E shows no operand text: it can be a password (#113) */
    memset(big, 'x', sizeof(big));
    memcpy(big, "PASSWORD(SECRET", 15);
    big[sizeof(big) - 1] = '\0';
    snprintf(cmd, sizeof(cmd), "PARTNER ADD ACME %s", big);
    nreq = 0;
    rc = run(cmd, out, sizeof(out));
    CHECK_EQ(rc, 8, "an operand too long is an error");
    CHECK(strstr(out, "OFTP003E OPERAND TOO LONG") != NULL, "OFTP003E");
    CHECK(strstr(out, "SECRET") == NULL && strstr(out, "xxxx") == NULL,
          "without a word of the operand's value");
    CHECK(strstr(out, "PASSWORD") != NULL, "only the keyword");
    memset(big, 'y', sizeof(big));
    big[sizeof(big) - 1] = '\0';
    snprintf(cmd, sizeof(cmd), "PARTNER ADD ACME %s", big);
    rc = run(cmd, out, sizeof(out));
    CHECK(rc == 8 && strstr(out, "yyyy") == NULL,
          "a long bare word is not shown either");

    /* the echo of SYSIN: a password is stars of one fixed length, whatever
    ** its own, and a quoted one with blanks goes whole (#113)
    */
    rc = batch("PARTNER ADD ACME PASSWORD(ab) OURPASSWORD='s e c r e t' -\n"
               "  ODETTEID(O0013000000ACME)\n"
               "PARTNER CHANGE ACME password=averyverylongsecret\n",
               out, sizeof(out));
    CHECK_EQ(rc, 0, "batch: both commands ran");
    CHECK(strstr(out, "PASSWORD(********)") != NULL,
          "PASSWORD(ab): eight stars, not two");
    CHECK(strstr(out, "OURPASSWORD=********") != NULL
          && strstr(out, "e c r e t") == NULL && strstr(out, "secret") == NULL,
          "a quoted password with blanks: all of it masked");
    CHECK(strstr(out, "password=********\n") != NULL
          && strstr(out, "verylong") == NULL,
          "a long password: eight stars, not its length");
    CHECK(strstr(out, "ODETTEID(O0013000000ACME)") != NULL,
          "the rest of the line is as typed");
    CHECK(strstr(lastreq, "PASSWORD=AVERYVERYLONGSECRET") != NULL,
          "and the request itself is not masked");

    /* every way of writing the value is masked, and all of it: a blank
    ** before the value, a blank around '=', a ')' inside the value (#121)
    */
    rc = batch("PARTNER CHANGE ACME PASSWORD secret1\n"
               "PARTNER CHANGE ACME PASSWORD (secret2)\n"
               "PARTNER CHANGE ACME PASSWORD = secret3\n"
               "PARTNER CHANGE ACME PASSWORD(ab)secret4)\n"
               "PARTNER CHANGE ACME PASSWORD( secret5 ) PORT(4000)\n"
               "PARTNER CHANGE ACME OURPASSWORD ='se cret6' PORT(4000)\n"
               "PARTNER CHANGE ACME PASSWORD('se)cret7')\n",
               out, sizeof(out));
    CHECK(strstr(out, "secret") == NULL && strstr(out, "cret") == NULL
          && strstr(out, "ab)") == NULL,
          "no form of the password value reaches the echo");
    CHECK(strstr(out, "PASSWORD ********\n") != NULL
          && strstr(out, "PASSWORD (********)\n") != NULL
          && strstr(out, "PASSWORD = ********\n") != NULL
          && strstr(out, "PASSWORD(********)\n") != NULL,
          "the stars stand where the value stood");
    CHECK(strstr(out, "PASSWORD( ******** ) PORT(4000)") != NULL
          && strstr(out, "OURPASSWORD ='********' PORT(4000)") == NULL
          && strstr(out, "OURPASSWORD =******** PORT(4000)") != NULL,
          "and what follows the value is as typed");

    /* a long command is echoed in lines that fit */
    rc = batch("SEND PARTNER(ACME) DSN(AAAAAAAAAAAA.BBBBBBBBBBBB.CCCCCCCCCC) -\n"
               "  VFN(DDDDDDDDDDDDDDDDDDDDDDDDDDDDDD) -\n"
               "  USER(EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE) -\n"
               "  FORMAT(T) HOLD\n",
               out, sizeof(out));
    CHECK_EQ(rc, 0, "batch: a continued command ran");
    CHECK(rc == 0 && longest_line(out) <= 72,
          "no echo line is longer than 72 characters");
    CHECK(strstr(out, "FORMAT(T)") != NULL && strstr(out, "HOLD") != NULL,
          "and the end of the command is in it");
    CHECK(strstr(lastreq, "FMT=T") != NULL, "the request got all of it");

    return mbt_test_summary("TSTUTL");
}
