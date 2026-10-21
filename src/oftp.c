/* OFTP - TSO command and batch utility, a client of the monitor
**
**   OFTP SEND PARTNER(name) DSN(data.set[(member)]) [VFN(name)]
**             [FORMAT(T)] [XLATE] [HOLD] [USER(text)] [DISP(DELETE)]
**             [STRTDATE(*|*+n|ddd)] [STRTTIME(hh:mm)] [SENDWIN(hh:mm-hh:mm)]
**   OFTP LIST [PARTNER(name)] [STATUS(state)] [MINE]
**   OFTP SHOW key           OFTP DELETE key
**   OFTP HOLD key           OFTP RELEASE key        OFTP RETRY key
**   OFTP HOLD|RELEASE PARTNER(name|*) [VFN(name|prefix*)]
**                           every send to that partner (* all) that you
**                           may change: QUEUED/WAITING held, HELD queued
**   OFTP CALL partner       OFTP PARTNERS
**   OFTP PARTNER LIST|SHOW name|ENABLE name|DISABLE name|DELETE name
**   OFTP PARTNER ADD|CHANGE name [ODETTEID(id)] [PASSWORD(pw)]
**             [OURPASSWORD(pw)] [OURID(id)] [HOST(name)] [PORT(n)]
**             [LEVEL(n)] [MODE(S|R|B)] [COMPRESS(Y|N)] [CALL(Y|N)]
**             [SESSIONS(n)] [ADDRESS(ip)] [VIA(partner)] [XLATE(Y|N)]
**             [SDEB(n)] [CREDIT(n)] [ENABLED(Y|N)]
**                           the partner table, kept by OFTPD and surviving
**                           its restart; administrators only, except LIST
**                           and SHOW. DELETE is refused while a transfer
**                           in the spool names the partner.
**   OFTP PURGE [DAYS(n)]    finished transfers older than n days (default
**                           KEEPDAYS in OFTPPRM); administrators only
**
** OFTPD is reached through its cross-memory SVC (SVC= in OFTPPRM).
** KEYWORD=VALUE works as well as KEYWORD(VALUE).
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
** and changes only their own. OFTPD reads the userid itself.
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
    { "PURGE", "DAYS" }, { "PARTNER", "PARTNER" },
};

/* The end of the word that starts at from, cut where words() cuts it: at
** a blank outside '...'. q says whether from is inside quotes.
*/
static const char *
word_end(const char *from, int q)
{
    const char *p;

    for (p = from; *p && (q || *p != ' '); p++)
        if (*p == '\'')
            q = !q;
    return p;
}

/* A password does not belong in a listing: the value of PASSWORD(x),
** PASSWORD=x, PASSWORD x, PASSWORD = x, OURPASSWORD=x or PASSWORD('x y')
** in an echoed line becomes eight stars, whatever its own length. The
** value is the whole word, as operand() takes it, less the closing
** parenthesis of a PASSWORD(...). cap is the size of line.
*/
static void
mask(char *line, size_t cap)
{
    char *p;
    char *s;
    const char *e;

    for (p = line; strlen(p) > 8; p++) {
        char k[9];
        char open;
        size_t head;
        size_t stars;
        size_t tail;
        int q = 0;

        memcpy(k, p, 8);
        k[8] = '\0';
        upcase(k);
        if (strcmp(k, "PASSWORD") != 0)
            continue;
        s = p + 8;
        while (*s == ' ')
            s++;
        open = *s == '(' || *s == '=' ? *s : '\0';
        if (open != '\0') {
            s++;
            while (*s == ' ')
                s++;
        } else if (s == p + 8) {
            continue;
        }
        for (e = line; e < s; e++)
            if (*e == '\'')
                q = !q;
        e = word_end(s, q);
        if (open == '(' && e > s && e[-1] == ')')
            e--;
        if (e == s)
            continue;
        head = (size_t)(s - line);
        stars = 8;
        if (head + stars >= cap)
            stars = cap - 1 - head;
        tail = strlen(e);
        if (head + stars + tail >= cap)
            tail = cap - 1 - head - stars;
        memmove(s + stars, e, tail);
        memset(s, '*', stars);
        s[stars + tail] = '\0';
        p = s + stars - 1;
    }
}

/* The echo of a command in lines of at most 72 columns, a long one
** continued with " -" as SYSIN continues it: the SYSPRINT of a job cuts
** what is longer than its record. It breaks at a blank; a word of more
** than 65 characters is broken inside.
*/
static void
put_echo(FILE *out, const char *text)
{
    const char *p = text;
    const char *lead = "OFTP ";

    for (;;) {
        size_t room = 72 - 5;
        size_t cut;

        if (strlen(p) <= room) {
            fprintf(out, "%s%s\n", lead, p);
            return;
        }
        for (cut = room - 2; cut > 0 && p[cut] != ' '; cut--)
            ;
        if (cut == 0)
            cut = room - 2;
        fprintf(out, "%s%.*s -\n", lead, (int)cut, p);
        p += cut;
        while (*p == ' ')
            p++;
        lead = "     ";
    }
}

/* What OFTP003E says of an operand that does not fit: its keyword and
** nothing of its value, which can be a password
*/
static void
too_long(FILE *out, const char *arg)
{
    const char *p = strpbrk(arg, "(=");

    if (p != NULL && p - arg < 16)
        fprintf(out, "OFTP003E OPERAND TOO LONG: %.*s\n", (int)(p - arg), arg);
    else
        fprintf(out, "OFTP003E OPERAND TOO LONG\n");
}

/* One command: argv[0] the verb. Returns its RC. */
static int
command(int argc, char **argv, FILE *out)
{
    char req[256];
    char final[256];
    char verb[12];
    char psub[10];
    const char *pos = NULL, *setting = NULL;
    int i;
    int rc;
    int mine = 0;
    int named = 0;
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

    /* PARTNER sub name ...: ENABLE and DISABLE are a CHANGE, LIST is
    ** PARTNERS */
    if (strcmp(verb, "PARTNER") == 0) {
        if (argc < 2) {
            fprintf(out, "OFTP002E PARTNER LIST, SHOW, ADD, CHANGE, ENABLE, "
                    "DISABLE OR DELETE\n");
            return 8;
        }
        snprintf(psub, sizeof(psub), "%s", argv[1]);
        upcase(psub);
        if (strcmp(psub, "LIST") == 0) {
            strcpy(verb, "PARTNERS");
            pos = "";
            argc = 1;
        } else if (strcmp(psub, "ENABLE") == 0
                   || strcmp(psub, "DISABLE") == 0) {
            setting = psub[0] == 'E' ? " ENABLED=Y" : " ENABLED=N";
            strcpy(psub, "CHANGE");
        } else if (strcmp(psub, "ADD") != 0 && strcmp(psub, "CHANGE") != 0
                   && strcmp(psub, "DELETE") != 0
                   && strcmp(psub, "SHOW") != 0) {
            fprintf(out, "OFTP002E UNKNOWN PARTNER COMMAND %s\n", argv[1]);
            return 8;
        }
        if (argc > 1 && argc < 3) {
            fprintf(out, "OFTP004E PARTNER %s NEEDS A PARTNER NAME\n", psub);
            return 8;
        }
    }

    n = (size_t)snprintf(req, sizeof(req), "%s", verb);
    if (argc > 1 && strcmp(verb, "PARTNER") == 0) {
        n += (size_t)snprintf(req + n, sizeof(req) - n, " %s", psub);
        argv++;
        argc--;
    }
    for (i = 1; i < argc; i++) {
        char key[16];
        char val[80];
        const char *a = argv[i];

        if (operand(a, key, sizeof(key), val, sizeof(val)) != 0) {
            too_long(out, a);
            return 8;
        }
        upcase(key);
        if (strcmp(key, "MINE") == 0 && val[0] == '\0') {
            mine = 1;
            continue;
        }
        /* a lone word after SHOW, CALL, ...: the key or the partner, once;
        ** a second one is a keyword that lost its value */
        if (val[0] == '\0' && pos[0] && strchr(argv[i], '(') == NULL
            && strchr(argv[i], '=') == NULL) {
            if (!named) {
                snprintf(val, sizeof(val), "%s", key);
                snprintf(key, sizeof(key), "%s", pos);
            } else if (strcmp(key, "XLATE") != 0 && strcmp(key, "HOLD") != 0) {
                /* the word itself may be a password: its place only */
                fprintf(out, "OFTP007E OPERAND %d HAS NO VALUE: WRITE "
                        "KEYWORD(VALUE)\n", i);
                return 8;
            }
        }
        if (pos[0] && strcmp(key, pos) == 0)
            named = 1;
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
    if (setting != NULL)
        n += (size_t)snprintf(req + n, sizeof(req) - n, "%s", setting);
    /* OFTPD knows who asks; MINE=Y says to show only that one's own */
    if (mine && n < sizeof(req))
        n += (size_t)snprintf(req + n, sizeof(req) - n, " MINE=Y");
    if (n >= sizeof(req)) {
        fprintf(out, "OFTP003E COMMAND TOO LONG\n");
        return 8;
    }
    rc = oftp_cli_request(req, show, out, final, sizeof(final));
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
            mask(echo, sizeof(echo));
            put_echo(out, echo);
            r = command(n, w, out);
            if (r > rc)
                rc = r;
            cmds++;
            line[0] = '\0';
        }
        if (cmds == 0) {
            fprintf(out, "OFTP001E NO COMMAND IN PARM OR SYSIN. "
                    "OFTP SEND|LIST|SHOW|DELETE|HOLD|RELEASE|RETRY|CALL|PARTNERS|PARTNER|PURGE\n");
            rc = 8;
        }
    }
    if (out != stdout)
        fclose(out);
    return rc;
}
