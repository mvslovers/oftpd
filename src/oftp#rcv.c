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

/* A literal character of a data set name pattern. The letters in three
** ranges: EBCDIC has gaps between I and J and between R and S.
*/
static int
literal_ok(char c)
{
    return (c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R')
           || (c >= 'S' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.'
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

static const char *
nz(const char *s)
{
    return s == NULL ? "" : s;
}

/* The n-th qualifier of vfn (empty when there is none) */
static void
qualifier(const char *vfn, int n, const char **v, size_t *len)
{
    const char *q = nz(vfn), *e;
    int i;

    for (i = 1; i < n && q != NULL; i++) {
        q = strchr(q, '.');
        if (q != NULL)
            q++;
    }
    if (q == NULL)
        q = "";
    e = strchr(q, '.');
    *v = q;
    *len = e == NULL ? strlen(q) : (size_t)(e - q);
}

/* The SFID time field is HHMMSScccc; the counter is its last four digits.
** A field that is shorter or not all digits has none: -1.
*/
static int
counter(const char *sfidtime, const char **v, size_t *n)
{
    int i;

    if (sfidtime == NULL || strlen(sfidtime) < 10)
        return -1;
    for (i = 6; i < 10; i++)
        if (sfidtime[i] < '0' || sfidtime[i] > '9')
            return -1;
    *v = sfidtime + 6;
    *n = 4;
    return 0;
}

/* What a symbol stands for: *v and *n; -1 when it stands for nothing */
static int
symvalue(int kind, int arg, const OFTP_RCVCTX *x, const char **v, size_t *n)
{
    switch (kind) {
    case S_QUAL:
        qualifier(x->vfn, arg, v, n);
        return 0;
    case S_ARR:
        *n = arg == 0 ? 4 : 2;
        if (strlen(nz(x->arrival)) < (size_t)arg + *n)
            return -1;
        *v = x->arrival + arg;
        return 0;
    case S_CTR:
        return counter(x->sfidtime, v, n);
    case S_ID:
        *v = nz(x->partner);
        break;
    case S_KEY:
        *v = nz(x->key);
        break;
    default:
        *v = nz(x->vfn);
        break;
    }
    *n = strlen(*v);
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
        const char *v = pat;
        size_t n = 1;
        int kind, len = 1, arg;

        if (*pat == '%') {
            kind = symbol(pat, &len, &arg);
            if (kind == S_NONE)
                return -1;
            if (x == NULL) {
                pat += len;
                continue;
            }
            if (symvalue(kind, arg, x, &v, &n) != 0)
                return -1;
        } else if (!literal_ok(*pat))
            return -1;
        if (append(out, cap, &at, v, n) != 0)
            return -2;
        pat += len;
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

/* The part of a pattern before its first literal '.' is the data set's
** high-level qualifier. What the partner sends (%VFN, %Q1..%Q9) or what
** comes with the file (%CTR) must not be in it: -3 when it is, else 0.
*/
static int
first_qualifier(const char *pat)
{
    int kind, len, arg;

    while (*pat != '\0' && *pat != '.') {
        len = 1;
        if (*pat == '%') {
            kind = symbol(pat, &len, &arg);
            if (kind == S_VFN || kind == S_QUAL || kind == S_CTR)
                return -3;
            if (kind == S_NONE)
                len = 1;
        }
        pat += len;
    }
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
        if (expand(r->dsn, NULL, t, sizeof(t)) != 0)
            return -1;
        return first_qualifier(r->dsn);
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

/* The next KEYWORD=VALUE token of text into tok, both upper-cased in
** place. 0 at the end of the line, 1 with *eq at the '=', -1 when the
** token has no '='.
*/
static int
next_token(const char **pp, char *tok, size_t cap, char **eq)
{
    const char *p = *pp;
    size_t n = 0;
    char *c;

    while (*p == ' ' || *p == '\t' || *p == ',')
        p++;
    if (*p == '\0' || *p == '\n' || *p == '\r') {
        *pp = p;
        return 0;
    }
    while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n'
           && *p != '\r') {
        if (n < cap - 1)
            tok[n++] = *p;
        p++;
    }
    tok[n] = '\0';
    *pp = p;
    if ((*eq = strchr(tok, '=')) == NULL)
        return -1;
    **eq = '\0';
    for (c = tok; *c; c++)
        *c = up(*c);
    for (c = *eq + 1; *c; c++)
        *c = up(*c);
    return 1;
}

/* RULE= or a keyword of the rule before: 0, or the error's text in m */
static const char *
apply(OFTP_RULES *s, const char *k, const char *v, int lineno, char *m,
      size_t mcap)
{
    OFTP_RULE *r;
    int rc;

    if (strcmp(k, "RULE") != 0) {
        if (s->n == 0) {
            snprintf(m, mcap, "line %d: %s before the first RULE=", lineno, k);
            return m;
        }
        rc = rule_kw(&s->r[s->n - 1], k, v);
    } else if (s->n >= OFTP_MAXRULE) {
        snprintf(m, mcap, "line %d: more than %d rules", lineno, OFTP_MAXRULE);
        return m;
    } else {
        r = &s->r[s->n++];
        memset(r, 0, sizeof(*r));
        strcpy(r->from, "*");
        strcpy(r->vfn, "*");
        r->xlate = -1;
        r->line = lineno;
        rc = put(r->name, sizeof(r->name), v);
    }
    if (rc == 0)
        return NULL;
    s->r[s->n - 1].bad = 1;
    if (rc == -2)
        snprintf(m, mcap, "line %d: unknown keyword %s", lineno, k);
    else if (rc == -3)
        snprintf(m, mcap, "line %d: DSN: the first qualifier must be literal"
                 " (no %%VFN, %%Q1-%%Q9, %%CTR)", lineno);
    else
        snprintf(m, mcap, "line %d: invalid value for %s", lineno, k);
    return m;
}

int
oftp_rcv_line(OFTP_RULES *s, const char *text, int lineno, OFTP_CFGMSG msg,
              void *ctx)
{
    const char *p = text;
    char m[160], tok[100], *eq;
    int errors = 0, t;

    if (*p == '*')
        return 0;
    while ((t = next_token(&p, tok, sizeof(tok), &eq)) != 0) {
        const char *err;

        if (t < 0) {
            snprintf(m, sizeof(m), "line %d: %s is not KEYWORD=VALUE", lineno, tok);
            err = m;
        } else
            err = apply(s, tok, eq + 1, lineno, m, sizeof(m));
        if (err != NULL) {
            say(msg, ctx, err);
            errors++;
        }
    }
    s->errors += errors;
    return errors;
}

/* 1 when a name the pattern can make may start with prefix + ".": the
** literal head of the pattern, up to its first symbol, and prefix agree on
** as many characters as the shorter of the two has. A pattern whose first
** qualifier is built from symbols (%ID, %KEY, the date) is left alone.
*/
static int
clashes(const char *pat, const char *prefix)
{
    char q[OFTP_RCV_DSN + 40];
    size_t n = 0, i;

    snprintf(q, sizeof(q), "%s.", prefix);
    for (i = 0; pat[i] != '\0' && pat[i] != '%'; i++)
        n++;
    if (pat[n] == '%' && memchr(pat, '.', n) == NULL)
        return 0;                       /* the first qualifier is built */
    if (n > strlen(q))
        n = strlen(q);
    return strncmp(pat, q, n) == 0;
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
        if (r->dsn[0] != '\0' && clashes(r->dsn, c->tmpprefix)) {
            snprintf(m, sizeof(m), "line %d: rule %s: DSN may name a data set"
                     " under TMPPREFIX=%s", r->line, r->name, c->tmpprefix);
            say(msg, ctx, m);
            s->errors++;
            r->bad = 1;
            continue;
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
