/* oftp#rcv.c - receive rules: OFTPRCV0 to rules, matching, data set names */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpdsn.h"
#include "oftprcv.h"

static void
say(OFTP_CFGMSG msg, void *ctx, const char *line)
{
    if (msg != NULL)
        msg(ctx, line);
}

static char
up(char c)
{
    return (char)toupper((unsigned char)c);
}

void
oftp_rcv_clear(OFTP_RULES *s)
{
    memset(s, 0, sizeof(*s));
}

int
oftp_rcv_wild(const char *p, const char *t)
{
    const char *star = NULL, *mark = NULL;

    while (*t) {
        if (*p == '*') {
            star = p++;
            mark = t;
        } else if (*p == '?' || (*p != '\0' && up(*p) == up(*t))) {
            p++;
            t++;
        } else if (star != NULL) {
            p = star + 1;
            t = ++mark;
        } else
            return 0;
    }
    while (*p == '*')
        p++;
    return *p == '\0';
}

const OFTP_RULE *
oftp_rcv_match(const OFTP_RULES *s, const char *partner, const char *vfn)
{
    int i;

    for (i = 0; i < s->n; i++) {
        const OFTP_RULE *r = &s->r[i];

        if (r->from[0] != '*' && strcmp(r->from, partner) != 0)
            continue;
        if (oftp_rcv_wild(r->vfn, vfn))
            return r;
    }
    return NULL;
}

/* ---- symbols ----------------------------------------------------- */

#define S_NONE  0
#define S_VFN   1
#define S_QUAL  2                       /* with the number in *arg       */
#define S_ID    3
#define S_ARR   4                       /* arrival: *arg the offset      */
#define S_CTR   5
#define S_KEY   6

/* The symbol at p (which points at the '%'): its kind, the length of its
** name including the '%' in *len, a number in *arg.
*/
static int
symbol(const char *p, int *len, int *arg)
{
    p++;
    *arg = 0;
    if (strncmp(p, "VFN", 3) == 0) {
        *len = 4;
        return S_VFN;
    }
    if (p[0] == 'Q' && p[1] >= '1' && p[1] <= '9') {
        *len = 3;
        *arg = p[1] - '0';
        return S_QUAL;
    }
    if (strncmp(p, "ID", 2) == 0) {
        *len = 3;
        return S_ID;
    }
    if (strncmp(p, "YYYY", 4) == 0) {
        *len = 5;
        *arg = 0;
        return S_ARR;
    }
    if (strncmp(p, "MM", 2) == 0) {
        *len = 3;
        *arg = 4;
        return S_ARR;
    }
    if (strncmp(p, "DD", 2) == 0) {
        *len = 3;
        *arg = 6;
        return S_ARR;
    }
    if (strncmp(p, "HH", 2) == 0) {
        *len = 3;
        *arg = 8;
        return S_ARR;
    }
    if (strncmp(p, "MI", 2) == 0) {
        *len = 3;
        *arg = 10;
        return S_ARR;
    }
    if (strncmp(p, "SS", 2) == 0) {
        *len = 3;
        *arg = 12;
        return S_ARR;
    }
    if (strncmp(p, "CTR", 3) == 0) {
        *len = 4;
        return S_CTR;
    }
    if (strncmp(p, "KEY", 3) == 0) {
        *len = 4;
        return S_KEY;
    }
    return S_NONE;
}

/* A literal character of a data set name pattern */
static int
literal_ok(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.'
           || c == '-' || c == '@' || c == '#' || c == '$';
}

/* Appends n characters of s, upper-cased; -1 when out[cap] is full */
static int
append(char *out, size_t cap, size_t *at, const char *s, size_t n)
{
    size_t i;

    if (*at + n >= cap)
        return -1;
    for (i = 0; i < n; i++)
        out[(*at)++] = up(s[i]);
    out[*at] = '\0';
    return 0;
}

/* The pattern with its symbols replaced. -1: a symbol or a character that
** means nothing, -2: out is too small. With x NULL only the syntax is
** checked.
*/
static int
expand(const char *pat, const OFTP_RCVCTX *x, char *out, size_t cap)
{
    size_t at = 0;

    out[0] = '\0';
    while (*pat) {
        const char *v = "";
        size_t n = 1;
        int kind, len, arg;

        if (*pat != '%') {
            if (!literal_ok(*pat))
                return -1;
            if (append(out, cap, &at, pat, 1) != 0)
                return -2;
            pat++;
            continue;
        }
        kind = symbol(pat, &len, &arg);
        if (kind == S_NONE)
            return -1;
        pat += len;
        if (x == NULL)
            continue;
        switch (kind) {
        case S_VFN:
            v = x->vfn;
            n = strlen(v);
            break;
        case S_QUAL: {
            const char *q = x->vfn, *e;
            int i;

            for (i = 1; i < arg; i++) {
                q = strchr(q, '.');
                if (q == NULL)
                    break;
                q++;
            }
            if (q == NULL) {
                n = 0;
                break;
            }
            e = strchr(q, '.');
            v = q;
            n = e == NULL ? strlen(q) : (size_t)(e - q);
            break;
        }
        case S_ID:
            v = x->partner;
            n = strlen(v);
            break;
        case S_ARR:
            v = x->arrival + arg;
            n = arg == 0 ? 4 : 2;
            break;
        case S_CTR:
            v = x->sfidtime + 6;
            n = 4;
            break;
        case S_KEY:
            v = x->key;
            n = strlen(v);
            break;
        }
        if (append(out, cap, &at, v, n) != 0)
            return -2;
    }
    return 0;
}

int
oftp_rcv_name(const OFTP_RULE *r, const OFTP_RCVCTX *x, char *out)
{
    char t[128];

    if (r->dsn[0] == '\0' || expand(r->dsn, x, t, sizeof(t)) != 0
        || !oftp_dsn_valid(t))
        return -1;
    strcpy(out, t);
    return 0;
}

/* ---- the member -------------------------------------------------- */

static int
yesno(const char *v)
{
    if (strcmp(v, "Y") == 0 || strcmp(v, "YES") == 0)
        return 1;
    if (strcmp(v, "N") == 0 || strcmp(v, "NO") == 0)
        return 0;
    return -1;
}

/* Copies v into dst[cap] when it is 1..cap-1 characters */
static int
put(char *dst, size_t cap, const char *v)
{
    if (v[0] == '\0' || strlen(v) >= cap)
        return -1;
    strcpy(dst, v);
    return 0;
}

static int
rule_kw(OFTP_RULE *r, const char *k, const char *v)
{
    int b;

    if (strcmp(k, "FROM") == 0)
        return put(r->from, sizeof(r->from), v);
    if (strcmp(k, "VFN") == 0)
        return put(r->vfn, sizeof(r->vfn), v);
    if (strcmp(k, "DSN") == 0) {
        char t[OFTP_RCV_DSN + 1];

        if (put(r->dsn, sizeof(r->dsn), v) != 0)
            return -1;
        return expand(r->dsn, NULL, t, sizeof(t)) == 0 ? 0 : -1;
    }
    if (strcmp(k, "REPLACE") == 0)
        return (b = yesno(v)) < 0 ? -1 : (r->replace = b, 0);
    if (strcmp(k, "UNIT") == 0)
        return put(r->unit, sizeof(r->unit), v);
    if (strcmp(k, "VOLSER") == 0)
        return put(r->volser, sizeof(r->volser), v);
    if (strcmp(k, "SPACE") == 0) {
        char *end;
        long n = strtol(v, &end, 10);

        if (v[0] == '\0' || *end != '\0' || n < 1 || n > 9999)
            return -1;
        r->space = (unsigned)n;
        return 0;
    }
    if (strcmp(k, "XLATE") == 0)
        return (b = yesno(v)) < 0 ? -1 : (r->xlate = b, 0);
    return -2;
}

int
oftp_rcv_line(OFTP_RULES *s, const char *text, int lineno, OFTP_CFGMSG msg,
              void *ctx)
{
    const char *p = text;
    char m[160];
    int errors = 0;

    if (*p == '*')
        return 0;
    for (;;) {
        char tok[100], *eq, *v;
        size_t n = 0;
        int rc;

        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '\r')
            break;
        while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n'
               && *p != '\r') {
            if (n < sizeof(tok) - 1)
                tok[n++] = *p;
            p++;
        }
        tok[n] = '\0';
        if ((eq = strchr(tok, '=')) == NULL) {
            snprintf(m, sizeof(m), "line %d: %s is not KEYWORD=VALUE", lineno, tok);
            say(msg, ctx, m);
            errors++;
            continue;
        }
        *eq = '\0';
        v = eq + 1;
        for (eq = tok; *eq; eq++)
            *eq = up(*eq);
        for (eq = v; *eq; eq++)
            *eq = up(*eq);

        if (strcmp(tok, "RULE") == 0) {
            OFTP_RULE *r;

            if (s->n >= OFTP_MAXRULE) {
                snprintf(m, sizeof(m), "line %d: more than %d rules", lineno,
                         OFTP_MAXRULE);
                say(msg, ctx, m);
                errors++;
                continue;
            }
            r = &s->r[s->n++];
            memset(r, 0, sizeof(*r));
            strcpy(r->from, "*");
            strcpy(r->vfn, "*");
            r->xlate = -1;
            r->line = lineno;
            rc = put(r->name, sizeof(r->name), v);
        } else if (s->n == 0) {
            snprintf(m, sizeof(m), "line %d: %s before the first RULE=", lineno, tok);
            say(msg, ctx, m);
            errors++;
            continue;
        } else
            rc = rule_kw(&s->r[s->n - 1], tok, v);

        if (rc == -2) {
            snprintf(m, sizeof(m), "line %d: unknown keyword %s", lineno, tok);
            say(msg, ctx, m);
            errors++;
        } else if (rc != 0) {
            snprintf(m, sizeof(m), "line %d: invalid value for %s", lineno, tok);
            say(msg, ctx, m);
            errors++;
        }
        if (rc != 0 && s->n > 0)
            s->r[s->n - 1].bad = 1;
    }
    s->errors += errors;
    return errors;
}

int
oftp_rcv_finish(OFTP_RULES *s, const OFTP_CONFIG *c, OFTP_CFGMSG msg, void *ctx)
{
    int i, j;
    char m[160];

    for (i = 0; i < s->n; i++) {
        OFTP_RULE *r = &s->r[i];

        if (r->bad)
            continue;
        if (r->from[0] != '*' || r->from[1] != '\0') {
            for (j = 0; j < c->nptn; j++)
                if (strcmp(c->ptn[j].peer.name, r->from) == 0)
                    break;
            if (j == c->nptn) {
                snprintf(m, sizeof(m), "line %d: rule %s: FROM=%s is no partner",
                         r->line, r->name, r->from);
                say(msg, ctx, m);
                s->errors++;
                r->bad = 1;
                continue;
            }
        }
        for (j = 0; j < i; j++)
            if (strcmp(s->r[j].name, r->name) == 0) {
                snprintf(m, sizeof(m), "line %d: rule %s is defined twice",
                         r->line, r->name);
                say(msg, ctx, m);
                s->errors++;
                r->bad = 1;
                break;
            }
    }
    return s->errors;
}
