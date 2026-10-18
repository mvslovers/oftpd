/* OFTP - TSO command and batch utility, a client of the monitor
**
**   OFTP SEND PARTNER(name) DSN(data.set[(member)]) [VFN(name)]
**             [FORMAT(T)] [XLATE] [HOLD] [USER(text)]
**   OFTP LIST [PARTNER(name)] [STATUS(state)] [MINE]
**   OFTP SHOW key           OFTP DELETE key
**   OFTP HOLD key           OFTP RELEASE key        OFTP RETRY key
**   OFTP CALL partner       OFTP PARTNERS
**
** OFTPD is reached through its cross-memory SVC (SVC= in OFTPPRM).
** HOST(address) PORT(n) go to its TCP control port instead -- 127.0.0.1
** and 3306 for whichever is left out -- which is for development and has
** no security. KEYWORD=VALUE works as well as KEYWORD(VALUE).
** Without a command (EXEC PGM=OFTP and no PARM) the commands are read
** from SYSIN, one per line, '*' in column 1 a comment, a '-' at the end
** of a line continuing it on the next (columns 73-80 are ignored).
**
** Output goes through PUTLINE under the TSO TMP, to SYSPRINT otherwise.
** Return code: 0 done; 4 done with a warning (an empty data set) or
** refused for now (no such transfer, transfer active); 8 an error in the
** command or its data set; 12 severe: an unknown partner, OFTPD not
** reached or not answering. For SYSIN the highest of them.
**
** The owner of a transfer is the userid of the job or TSO session that
** queued it, and anyone but an administrator (ADMIN= in OFTPPRM) sees
** and changes only their own. Over the SVC, OFTPD reads the userid
** itself; the control port believes the OWNER= this program sends.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftp.h"
#include "oftpcli.h"

static void
upcase(char *s)
{
    for (; *s; s++)
        if ((*s >= 'a' && *s <= 'i') || (*s >= 'j' && *s <= 'r')
            || (*s >= 's' && *s <= 'z'))
            *s = (char)(*s - 'a' + 'A');
}

/* KEY(value) or KEY=value into key and value; quotes removed */
static int
operand(const char *arg, char *key, size_t kcap, char *val, size_t vcap)
{
    const char *p = strpbrk(arg, "(=");
    size_t kl, vl;

    if (p == NULL) {
        snprintf(key, kcap, "%s", arg);
        val[0] = '\0';
        return strlen(arg) < kcap ? 0 : -1;
    }
    kl = (size_t)(p - arg);
    if (kl >= kcap)
        return -1;
    memcpy(key, arg, kl);
    key[kl] = '\0';
    p++;
    if (*p == '\'')
        p++;
    vl = strlen(p);
    if (arg[kl] == '(' && vl > 0 && p[vl - 1] == ')')
        vl--;
    if (vl > 0 && p[vl - 1] == '\'')
        vl--;
    if (vl >= vcap)
        return -1;
    memcpy(val, p, vl);
    val[vl] = '\0';
    return 0;
}

/* Each answer line of OFTPD, as it comes */
static void
show(void *ctx, const char *text)
{
    fprintf((FILE *)ctx, "%s\n", text);
}

/* The commands, with the keyword a lone positional operand stands for */
static const char *const verbs[][2] = {
    { "SEND", "" },     { "LIST", "" },       { "SHOW", "KEY" },
    { "DELETE", "KEY" }, { "HOLD", "KEY" },   { "RELEASE", "KEY" },
    { "RETRY", "KEY" },  { "CALL", "PARTNER" }, { "PARTNERS", "" },
};

/* One command: argv[0] the verb. Returns its RC. */
static int
command(int argc, char **argv, FILE *out)
{
    char req[300], host[64] = "", final[256], verb[12], me[9];
    const char *pos = NULL;
    int port = 3306, tcp = 0, i, rc, mine = 0;
    size_t n;

    snprintf(verb, sizeof(verb), "%s", argv[0]);
    upcase(verb);
    for (i = 0; i < (int)(sizeof(verbs) / sizeof(verbs[0])); i++)
        if (strcmp(verb, verbs[i][0]) == 0)
            pos = verbs[i][1];
    if (pos == NULL) {
        fprintf(out, "OFTP002E UNKNOWN COMMAND %s\n", argv[0]);
        return 8;
    }
    oftp_cli_whoami(me);

    n = (size_t)snprintf(req, sizeof(req), "%s", verb);
    for (i = 1; i < argc; i++) {
        char key[16], val[80];
        const char *a = argv[i];

        if (operand(a, key, sizeof(key), val, sizeof(val)) != 0) {
            fprintf(out, "OFTP003E OPERAND TOO LONG: %s\n", a);
            return 8;
        }
        upcase(key);
        if (strcmp(key, "HOST") == 0) {
            snprintf(host, sizeof(host), "%s", val);
            tcp = 1;
            continue;
        }
        if (strcmp(key, "PORT") == 0) {
            port = atoi(val);
            tcp = 1;
            continue;
        }
        if (strcmp(key, "MINE") == 0 && val[0] == '\0') {
            mine = 1;
            continue;
        }
        /* a lone word after SHOW, CALL, ...: the key or the partner */
        if (val[0] == '\0' && pos[0] && strchr(argv[i], '(') == NULL
            && strchr(argv[i], '=') == NULL) {
            snprintf(val, sizeof(val), "%s", key);
            snprintf(key, sizeof(key), "%s", pos);
        }
        if (strcmp(key, "FORMAT") == 0)
            strcpy(key, "FMT");
        upcase(val);
        if ((strcmp(key, "XLATE") == 0 || strcmp(key, "HOLD") == 0)
            && val[0] == '\0')
            strcpy(val, "Y");
        n += (size_t)snprintf(req + n, sizeof(req) - n, " %s=%s", key, val);
        if (n >= sizeof(req)) {
            fprintf(out, "OFTP003E COMMAND TOO LONG\n");
            return 8;
        }
    }
    /* MINE=Y for the SVC, where OFTPD knows who asks; OWNER= for the
    ** control port, which does not (and the SVC drops it)
    */
    if (mine && n < sizeof(req))
        n += (size_t)snprintf(req + n, sizeof(req) - n, " MINE=Y");
    if ((strcmp(verb, "SEND") == 0 || mine) && me[0] && tcp && n < sizeof(req))
        n += (size_t)snprintf(req + n, sizeof(req) - n, " OWNER=%s", me);
    if (n >= sizeof(req)) {
        fprintf(out, "OFTP003E COMMAND TOO LONG\n");
        return 8;
    }
    if (tcp && host[0] == '\0')
        strcpy(host, "127.0.0.1");

    rc = oftp_cli_request(tcp ? host : NULL, port, req, show, out, final,
                          sizeof(final));
    if (rc == 12)
        fprintf(out, "OFTP010E %s\n", final);
    else
        fprintf(out, "%s\n", final);
    return rc;
}

/* Splits a SYSIN line into words; '...' keeps its blanks */
static int
words(char *line, char **w, int max)
{
    int n = 0, q = 0;
    char *p = line;

    while (*p && n < max) {
        while (*p == ' ')
            p++;
        if (*p == '\0' || *p == '\n')
            break;
        w[n++] = p;
        for (; *p && *p != '\n' && (q || *p != ' '); p++)
            if (*p == '\'')
                q = !q;
        if (*p)
            *p++ = '\0';
    }
    return n;
}

int
main(int argc, char **argv)
{
    FILE *out = fopen("*PUTLINE", "w");
    int rc = 0;

    if (out == NULL)
        out = stdout;
    if (argc >= 2)
        rc = command(argc - 1, argv + 1, out);
    else {
        /* stdin is DD SYSIN: libc370 opens it there at startup, and a
        ** second fopen("DD:SYSIN") fails (measured on MVS) */
        FILE *in = stdin;
        char line[256], card[100], *w[20];
        int cmds = 0;

        /* a card ending in '-' continues on the next one, as in TSO */
        line[0] = '\0';
        while (in != NULL && fgets(card, sizeof(card), in) != NULL) {
            char echo[256];
            size_t len;
            int n, r;

            if (strlen(card) > 72)
                card[72] = '\0';
            card[strcspn(card, "\n")] = '\0';
            if (card[0] == '*' && line[0] == '\0')
                continue;
            len = strlen(card);
            while (len > 0 && card[len - 1] == ' ')
                card[--len] = '\0';
            if (strlen(line) + len + 1 >= sizeof(line)) {
                fprintf(out, "OFTP003E COMMAND TOO LONG\n");
                line[0] = '\0';
                rc = 8;
                continue;
            }
            if (len > 0 && card[len - 1] == '-') {
                card[len - 1] = ' ';
                strcat(line, card);
                continue;
            }
            strcat(line, card);
            snprintf(echo, sizeof(echo), "%s", line);
            n = words(line, w, 20);
            if (n == 0) {
                line[0] = '\0';
                continue;
            }
            fprintf(out, "OFTP %s\n", echo);
            r = command(n, w, out);
            if (r > rc)
                rc = r;
            cmds++;
            line[0] = '\0';
        }
        if (cmds == 0) {
            fprintf(out, "OFTP001E NO COMMAND IN PARM OR SYSIN. "
                    "OFTP SEND|LIST|SHOW|DELETE|HOLD|RELEASE|RETRY|CALL|PARTNERS\n");
            rc = 8;
        }
    }
    if (out != stdout)
        fclose(out);
    return rc;
}
