/* oftp#cfg.c - OFTPPRM and OFTPPTN: keywords to configuration */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpcfg.h"

static void
say(OFTP_CFGMSG msg, const void *ctx, const char *fmt, int lineno, const char *what)
{
    char line[120];

    if (msg == NULL)
        return;
    snprintf(line, sizeof(line), fmt, lineno, what);
    msg(ctx, line);
}

void
oftp_cfg_defaults(OFTP_CONFIG *c)
{
    memset(c, 0, sizeof(*c));
    c->port = 3305;
    strcpy(c->inprefix, "OFTP.IN");
    strcpy(c->tmpprefix, "OFTP.TMP");
    c->maxsess = 4;
    c->sdeb = 4096;
    c->cred = 99;
    c->timeout = 60;
    c->tlrecl = 255;
    c->ublksize = 6144;
    c->retry = 300;
    c->maxretry = 10;
    c->keepdays = 7;
}

/* A decimal number in lo..hi, or -1 */
static long
num(const char *v, long lo, long hi)
{
    char *end;
    long n;

    if (*v == '\0')
        return -1;
    n = strtol(v, &end, 10);
    if (*end != '\0' || n < lo || n > hi)
        return -1;
    return n;
}

static int
yesno(const char *v)
{
    if (strcmp(v, "Y") == 0 || strcmp(v, "YES") == 0)
        return 1;
    if (strcmp(v, "N") == 0 || strcmp(v, "NO") == 0)
        return 0;
    return -1;
}

/* Copies v into dst[cap]; 0, or -1 when it does not fit */
static int
put(char *dst, size_t cap, const char *v)
{
    if (strlen(v) >= cap)
        return -1;
    strcpy(dst, v);
    return 0;
}

/* ADMIN=id adds one userid to the list, so ADMIN= is repeated for each:
** a comma separates keywords in these members, not values. An id is 1-8
** characters; one named twice is kept once.
*/
static int
admin_kw(OFTP_CONFIG *c, const char *v)
{
    if (v[0] == '\0' || strlen(v) > 8)
        return -1;
    if (oftp_cfg_admin(c, v))
        return 0;
    if (c->nadmin >= OFTP_MAXADM)
        return -1;
    strcpy(c->admin[c->nadmin++], v);
    return 0;
}

static int
monitor_kw(OFTP_CONFIG *c, const char *k, const char *v)
{
    long n;

    if (strcmp(k, "ODETTEID") == 0)
        return put(c->odetteid, sizeof(c->odetteid), v);
    if (strcmp(k, "PORT") == 0)
        return (n = num(v, 1, 65535)) < 0 ? -1 : (c->port = (int)n, 0);
    /* 200-255 is the installation SVC range. A number outside it is an
    ** error rather than a silent 0: a typo would otherwise leave the
    ** transport off with nothing said about it.
    */
    if (strcmp(k, "SVC") == 0)
        return (n = num(v, 200, 255)) < 0 ? -1 : (c->svc = (int)n, 0);
    if (strcmp(k, "INPREFIX") == 0)
        return put(c->inprefix, sizeof(c->inprefix), v);
    if (strcmp(k, "TMPPREFIX") == 0)
        return put(c->tmpprefix, sizeof(c->tmpprefix), v);
    if (strcmp(k, "MAXSESS") == 0)
        return (n = num(v, 1, OFTP_MAXSESS)) < 0 ? -1 : (c->maxsess = (int)n, 0);
    if (strcmp(k, "SDEB") == 0)
        return (n = num(v, 128, OFTP_SDEB_MAX)) < 0 ? -1 : (c->sdeb = (unsigned)n, 0);
    if (strcmp(k, "CREDIT") == 0)
        return (n = num(v, 1, 999)) < 0 ? -1 : (c->cred = (unsigned)n, 0);
    if (strcmp(k, "TIMEOUT") == 0)
        return (n = num(v, 5, 3600)) < 0 ? -1 : (c->timeout = (int)n, 0);
    if (strcmp(k, "TLRECL") == 0)
        return (n = num(v, 20, 32756)) < 0 ? -1 : (c->tlrecl = (unsigned)n, 0);
    if (strcmp(k, "UBLKSIZE") == 0)
        return (n = num(v, 512, 32760)) < 0 ? -1 : (c->ublksize = (unsigned)n, 0);
    if (strcmp(k, "UNIT") == 0)
        return put(c->unit, sizeof(c->unit), v);
    if (strcmp(k, "VOLSER") == 0)
        return put(c->volser, sizeof(c->volser), v);
    if (strcmp(k, "RETRY") == 0)
        return (n = num(v, 10, 86400)) < 0 ? -1 : (c->retry = (int)n, 0);
    if (strcmp(k, "MAXRETRY") == 0)
        return (n = num(v, 0, 255)) < 0 ? -1 : (c->maxretry = (int)n, 0);
    if (strcmp(k, "KEEPDAYS") == 0)
        return (n = num(v, 0, 9999)) < 0 ? -1 : (c->keepdays = (int)n, 0);
    if (strcmp(k, "RELAY") == 0)
        return (n = yesno(v)) < 0 ? -1 : (c->relay = (int)n, 0);
    if (strcmp(k, "ADMIN") == 0)
        return admin_kw(c, v);
    return -2;
}

static int
partner_kw(OFTP_PARTNER *p, const char *k, const char *v)
{
    long n;
    int b;

    if (strcmp(k, "ODETTEID") == 0)
        return put(p->peer.code, sizeof(p->peer.code), v);
    if (strcmp(k, "PASSWORD") == 0)
        return put(p->peer.pswd_in, sizeof(p->peer.pswd_in), v);
    if (strcmp(k, "OURPASSWORD") == 0)
        return put(p->peer.pswd_out, sizeof(p->peer.pswd_out), v);
    if (strcmp(k, "OURID") == 0)
        return put(p->peer.ourcode, sizeof(p->peer.ourcode), v);
    if (strcmp(k, "HOST") == 0)
        return put(p->host, sizeof(p->host), v);
    if (strcmp(k, "PORT") == 0)
        return (n = num(v, 1, 65535)) < 0 ? -1 : (p->port = (int)n, 0);
    if (strcmp(k, "LEVEL") == 0) {
        n = num(v, 1, 5);
        if (n != 1 && n != 2 && n != 4 && n != 5)
            return -1;
        p->peer.level = (int)n;
        return 0;
    }
    if (strcmp(k, "SDEB") == 0)
        return (n = num(v, 128, OFTP_SDEB_MAX)) < 0 ? -1 : (p->peer.sdeb = (unsigned)n, 0);
    if (strcmp(k, "CREDIT") == 0)
        return (n = num(v, 1, 999)) < 0 ? -1 : (p->peer.cred = (unsigned)n, 0);
    if (strcmp(k, "MODE") == 0) {
        if (strcmp(v, "S") != 0 && strcmp(v, "R") != 0 && strcmp(v, "B") != 0)
            return -1;
        p->peer.mode = v[0];
        return 0;
    }
    if (strcmp(k, "COMPRESS") == 0)
        return (b = yesno(v)) < 0 ? -1 : (p->peer.cmpr = b, 0);
    if (strcmp(k, "ENABLED") == 0)
        return (b = yesno(v)) < 0 ? -1 : (p->enabled = b, 0);
    if (strcmp(k, "CALL") == 0)
        return (b = yesno(v)) < 0 ? -1 : (p->callwant = b, 0);
    if (strcmp(k, "ADDRESS") == 0)
        return put(p->address, sizeof(p->address), v);
    if (strcmp(k, "XLATE") == 0)
        return (b = yesno(v)) < 0 ? -1 : (p->xlate = b, 0);
    if (strcmp(k, "VIA") == 0)
        return put(p->via, sizeof(p->via), v);
    if (strcmp(k, "SESSIONS") == 0)
        return (n = num(v, 1, OFTP_MAXSESS)) < 0 ? -1 : (p->sessions = (int)n, 0);
    return -2;
}

/* The next KEYWORD=VALUE of *pp into tok[100]: tok the keyword, upper-cased,
** *v the value, upper-cased except HOST. 1 for a pair, 0 at the end, -1
** for a word without '=' (tok holds it, but it is not to be shown: it can
** be the rest of a password), -2 for a word of more than 99 characters
** (tok is empty).
*/
static int
next_kv(const char **pp, char *tok, char **v)
{
    const char *p = *pp;
    char *eq;
    size_t n = 0;

    while (*p == ' ' || *p == '\t' || *p == ',')
        p++;
    if (*p == '\0' || *p == '\n' || *p == '\r') {
        *pp = p;
        return 0;
    }
    while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n'
           && *p != '\r') {
        if (n < 99)
            tok[n++] = *p;
        else
            n = 100;
        p++;
    }
    *pp = p;
    if (n > 99) {
        tok[0] = '\0';
        return -2;
    }
    tok[n] = '\0';
    if ((eq = strchr(tok, '=')) == NULL)
        return -1;
    *eq = '\0';
    *v = eq + 1;
    for (eq = tok; *eq; eq++)
        *eq = (char)toupper((unsigned char)*eq);
    if (strcmp(tok, "HOST") != 0)
        for (eq = *v; *eq; eq++)
            *eq = (char)toupper((unsigned char)*eq);
    return 1;
}

static OFTP_PARTNER *
last_partner(OFTP_CONFIG *c)
{
    return c->nptn > 0 ? &c->ptn[c->nptn - 1] : NULL;
}

/* Where a message about a line goes: its number and the message sink. */
typedef struct {
    int         lineno;
    OFTP_CFGMSG msg;
    const void *ctx;
} CFG_AT;

/* A word next_kv() refused, the word-th of its line: -2 too long, else no
** KEYWORD=VALUE. The word is named by its place, never shown.
*/
static void
cfg_badword(int partner, OFTP_PARTNER *cur, int got, int word,
            const CFG_AT *at)
{
    if (got != -2) {
        char what[40];

        snprintf(what, sizeof(what), "word %d is not KEYWORD=VALUE", word);
        say(at->msg, at->ctx, "line %d: %s", at->lineno, what);
        return;
    }
    say(at->msg, at->ctx, "line %d: %s", at->lineno,
        "a word is longer than 99 characters");
    if (partner && cur != NULL)
        cur->enabled = 0;
}

/* One KEYWORD=VALUE word of a line; 1 when it is wrong, else 0. */
static int
cfg_word(OFTP_CONFIG *c, int partner, OFTP_PARTNER **cur, const char *tok,
         const char *v, const CFG_AT *at)
{
    OFTP_CFGMSG msg = at->msg;
    const void *ctx = at->ctx;
    int lineno = at->lineno;
    int rc;

    if (!partner)
        rc = monitor_kw(c, tok, v);
    else if (strcmp(tok, "PARTNER") == 0) {
        OFTP_PARTNER *np;

        if (c->nptn >= OFTP_MAXPTN) {
            say(msg, ctx, "line %d: more than %s partners", lineno, "32");
            return 1;
        }
        np = &c->ptn[c->nptn++];
        *cur = np;
        oftp_cfg_newptn(np, "");
        np->line = lineno;
        rc = put(np->peer.name, sizeof(np->peer.name), v);
        if (rc == 0 && v[0] == '\0')
            rc = -1;
    } else if (*cur == NULL) {
        say(msg, ctx, "line %d: %s before the first PARTNER=", lineno, tok);
        return 1;
    } else
        rc = partner_kw(*cur, tok, v);

    if (rc == -2)
        say(msg, ctx, "line %d: unknown keyword %s", lineno, tok);
    else if (rc != 0)
        say(msg, ctx, "line %d: invalid value for %s", lineno, tok);
    if (rc != 0 && partner && *cur != NULL)
        (*cur)->enabled = 0;
    return rc != 0;
}

int
oftp_cfg_line(OFTP_CONFIG *c, int partner, const char *text, int lineno,
              OFTP_CFGMSG msg, const void *ctx)
{
    const char *p = text;
    const CFG_AT at = { lineno, msg, ctx };
    OFTP_PARTNER *cur = last_partner(c);
    int errors = 0;
    int word = 0;

    if (*p == '*')
        return 0;
    for (;;) {
        char tok[100];
        char *v = NULL;
        int got = next_kv(&p, tok, &v);

        if (got == 0)
            break;
        word++;
        if (got < 0) {
            cfg_badword(partner, cur, got, word, &at);
            errors++;
            continue;
        }
        errors += cfg_word(c, partner, &cur, tok, v, &at);
    }
    c->errors += errors;
    return errors;
}

void
oftp_cfg_newptn(OFTP_PARTNER *p, const char *name)
{
    memset(p, 0, sizeof(*p));
    p->enabled = 1;
    p->call = 1;
    p->callwant = 1;
    snprintf(p->peer.name, sizeof(p->peer.name), "%s", name);
}

int
oftp_cfg_apply(OFTP_PARTNER *p, const char *text, char *why, int whylen)
{
    const char *t = text;
    int word = 0;

    for (;;) {
        char tok[100];
        char *v = NULL;
        int rc;
        int got;

        if ((got = next_kv(&t, tok, &v)) == 0)
            return 0;
        word++;
        if (got == -2) {
            snprintf(why, (size_t)whylen, "a word is longer than 99 characters");
            return -1;
        }
        if (got < 0) {
            snprintf(why, (size_t)whylen, "word %d is not KEYWORD=VALUE", word);
            return -1;
        }
        if (strcmp(tok, "PARTNER") == 0)
            rc = strcmp(v, p->peer.name) == 0 ? 0 : -3;
        else
            rc = partner_kw(p, tok, v);
        if (rc == -3)
            snprintf(why, (size_t)whylen, "a partner cannot be renamed");
        else if (rc == -2)
            snprintf(why, (size_t)whylen, "unknown keyword %s", tok);
        else if (rc != 0)
            snprintf(why, (size_t)whylen, "invalid value for %s", tok);
        if (rc != 0)
            return -1;
    }
}

void
oftp_cfg_complete(const OFTP_CONFIG *c, OFTP_PARTNER *p)
{
    if (p->peer.ourcode[0] == '\0')
        strcpy(p->peer.ourcode, c->odetteid);
    if (p->peer.level == 0)
        p->peer.level = OFTP_LEV20;
    if (p->peer.sdeb == 0)
        p->peer.sdeb = c->sdeb;
    if (p->peer.cred == 0)
        p->peer.cred = c->cred;
    if (p->peer.mode == 0)
        p->peer.mode = 'B';
    if (p->port == 0)
        p->port = 3305;
    if (p->sessions == 0)
        p->sessions = 1;
    p->call = p->callwant && p->host[0] != '\0' && p->via[0] == '\0';
}

int
oftp_cfg_vias(OFTP_CONFIG *c, OFTP_CFGMSG msg, const void *ctx)
{
    int i, j, off = 0;

    for (i = 0; i < c->nptn; i++) {
        OFTP_PARTNER *p = &c->ptn[i];
        const OFTP_PARTNER *v = NULL;

        if (!p->via[0] || !p->enabled)
            continue;
        for (j = 0; j < c->nptn; j++)
            if (c->ptn[j].peer.name[0]
                && strcmp(c->ptn[j].peer.name, p->via) == 0)
                v = &c->ptn[j];
        if (v == NULL || !v->enabled || v->via[0] || v == p) {
            if (p->line > 0)
                say(msg, ctx, "line %d: partner %s: VIA names no direct partner, disabled",
                    p->line, p->peer.name);
            else if (msg != NULL) {
                char line[120];

                snprintf(line, sizeof(line),
                         "partner %s: VIA names no direct partner, disabled",
                         p->peer.name);
                msg(ctx, line);
            }
            p->enabled = 0;
            off++;
        }
    }
    return off;
}

int
oftp_cfg_finish(OFTP_CONFIG *c, OFTP_CFGMSG msg, const void *ctx)
{
    int i, j, on = 0;

    for (i = 0; i < c->nptn; i++) {
        OFTP_PARTNER *p = &c->ptn[i];

        if (p->peer.code[0] == '\0') {
            say(msg, ctx, "line %d: partner %s has no ODETTEID, disabled",
                p->line, p->peer.name);
            p->enabled = 0;
        }
        for (j = 0; j < i; j++)
            if (strcmp(c->ptn[j].peer.name, p->peer.name) == 0
                || strcmp(c->ptn[j].peer.code, p->peer.code) == 0) {
                say(msg, ctx, "line %d: partner %s defined twice, disabled",
                    p->line, p->peer.name);
                p->enabled = 0;
            }
        oftp_cfg_complete(c, p);
        if (p->enabled)
            on++;
    }
    on -= oftp_cfg_vias(c, msg, ctx);
    return on;
}

OFTP_PARTNER *
oftp_cfg_find(OFTP_CONFIG *c, const char *name)
{
    int i;

    for (i = 0; i < c->nptn; i++)
        if (c->ptn[i].enabled && strcmp(c->ptn[i].peer.name, name) == 0)
            return &c->ptn[i];
    return NULL;
}

OFTP_PARTNER *
oftp_cfg_route(OFTP_CONFIG *c, const char *odetteid, OFTP_PARTNER **final)
{
    int i;

    for (i = 0; i < c->nptn; i++) {
        OFTP_PARTNER *p = &c->ptn[i];

        if (!p->enabled || strcmp(p->peer.code, odetteid) != 0)
            continue;
        *final = p;
        return p->via[0] ? oftp_cfg_find(c, p->via) : p;
    }
    *final = NULL;
    return NULL;
}

int
oftp_cfg_relay_live(const OFTP_PARTNER *next, const char *nextname,
                    const OFTP_PARTNER *final, const char *dest,
                    char *tname, size_t tcap)
{
    tname[0] = '\0';
    if (!next->enabled || strcmp(next->peer.name, nextname) != 0)
        return 0;
    if (final == next)
        return 1;
    if (!final->enabled || strcmp(final->peer.code, dest) != 0)
        return 0;
    snprintf(tname, tcap, "%s", final->peer.name);
    return 1;
}

OFTP_PARTNER *
oftp_cfg_byid(OFTP_CONFIG *c, const char *odetteid)
{
    int i;

    for (i = 0; i < c->nptn; i++)
        if (c->ptn[i].enabled && !c->ptn[i].via[0]
            && strcmp(c->ptn[i].peer.code, odetteid) == 0)
            return &c->ptn[i];
    return NULL;
}

int
oftp_cfg_retry(const OFTP_CONFIG *c, unsigned attempts, long now, long *tnext)
{
    long wait;

    if (c->maxretry > 0 && attempts >= (unsigned)c->maxretry)
        return 1;
    if (attempts == 0)
        attempts = 1;
    wait = (long)attempts > 86400L / c->retry ? 86400L : c->retry * (long)attempts;
    *tnext = now + wait;
    return 0;
}

int
oftp_cfg_admin(const OFTP_CONFIG *c, const char *user)
{
    int i;

    if (user == NULL || user[0] == '\0')
        return 0;
    for (i = 0; i < c->nadmin; i++)
        if (strcmp(c->admin[i], user) == 0)
            return 1;
    return 0;
}

int
oftp_cfg_slot(const OFTP_CONFIG *c, const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0')
        return -1;
    for (i = 0; i < c->nptn; i++)
        if (strcmp(c->ptn[i].peer.name, name) == 0)
            return i;
    return -1;
}

int
oftp_cfg_alloc(OFTP_CONFIG *c)
{
    int i;

    for (i = 0; i < c->nptn; i++)
        if (c->ptn[i].peer.name[0] == '\0') {
            memset(&c->ptn[i], 0, sizeof(c->ptn[i]));
            return i;
        }
    if (c->nptn >= OFTP_MAXPTN)
        return -1;
    memset(&c->ptn[c->nptn], 0, sizeof(c->ptn[0]));
    return c->nptn++;
}

void
oftp_cfg_free(OFTP_CONFIG *c, int i)
{
    memset(&c->ptn[i], 0, sizeof(c->ptn[i]));
}

int
oftp_cfg_count(const OFTP_CONFIG *c, int *enabled)
{
    int i, n = 0, on = 0;

    for (i = 0; i < c->nptn; i++) {
        if (c->ptn[i].peer.name[0] == '\0')
            continue;
        n++;
        if (c->ptn[i].enabled)
            on++;
    }
    if (enabled != NULL)
        *enabled = on;
    return n;
}

int
oftp_cfg_check(const OFTP_CONFIG *c, const OFTP_PARTNER *p, int self,
               char *why, int whylen)
{
    int j;

    if (p->peer.name[0] == '\0') {
        snprintf(why, (size_t)whylen, "no partner name");
        return -1;
    }
    if (p->peer.code[0] == '\0') {
        snprintf(why, (size_t)whylen, "ODETTEID is required");
        return -1;
    }
    for (j = 0; j < c->nptn; j++) {
        const OFTP_PARTNER *q = &c->ptn[j];

        if (j == self || q->peer.name[0] == '\0')
            continue;
        if (strcmp(q->peer.name, p->peer.name) == 0) {
            snprintf(why, (size_t)whylen, "partner %s exists", q->peer.name);
            return -1;
        }
        if (strcmp(q->peer.code, p->peer.code) == 0) {
            snprintf(why, (size_t)whylen,
                     "ODETTEID is partner %s's already", q->peer.name);
            return -1;
        }
        if (q->via[0] && strcmp(q->via, p->peer.name) == 0 && p->via[0]) {
            snprintf(why, (size_t)whylen,
                     "partner %s is reached through it: it cannot have a VIA",
                     q->peer.name);
            return -1;
        }
    }
    if (p->via[0]) {
        int v = oftp_cfg_slot(c, p->via);

        if (v < 0 || v == self || !c->ptn[v].enabled || c->ptn[v].via[0]) {
            snprintf(why, (size_t)whylen,
                     "VIA names no enabled direct partner");
            return -1;
        }
    }
    return 0;
}

int
oftp_cfg_text(const OFTP_CONFIG *c, const OFTP_PARTNER *p, char *out,
              size_t cap)
{
    size_t n;
    int w;

#define ADD(...) \
    do { \
        w = snprintf(out + n, cap - n, __VA_ARGS__); \
        if (w < 0 || (size_t)w >= cap - n) \
            return -1; \
        n += (size_t)w; \
    } while (0)

    if (cap == 0)
        return -1;
    n = 0;
    out[0] = '\0';
    ADD("PARTNER=%s ODETTEID=%s", p->peer.name, p->peer.code);
    if (p->peer.pswd_in[0])
        ADD(" PASSWORD=%s", p->peer.pswd_in);
    if (p->peer.pswd_out[0])
        ADD(" OURPASSWORD=%s", p->peer.pswd_out);
    if (p->peer.ourcode[0] && strcmp(p->peer.ourcode, c->odetteid) != 0)
        ADD(" OURID=%s", p->peer.ourcode);
    if (p->host[0])
        ADD(" HOST=%s", p->host);
    if (p->port && p->port != 3305)
        ADD(" PORT=%d", p->port);
    if (p->peer.level && p->peer.level != OFTP_LEV20)
        ADD(" LEVEL=%d", p->peer.level);
    if (p->peer.sdeb && p->peer.sdeb != c->sdeb)
        ADD(" SDEB=%u", p->peer.sdeb);
    if (p->peer.cred && p->peer.cred != c->cred)
        ADD(" CREDIT=%u", p->peer.cred);
    if (p->peer.mode && p->peer.mode != 'B')
        ADD(" MODE=%c", p->peer.mode);
    if (p->peer.cmpr)
        ADD(" COMPRESS=Y");
    if (!p->enabled)
        ADD(" ENABLED=N");
    if (!p->callwant)
        ADD(" CALL=N");
    if (p->address[0])
        ADD(" ADDRESS=%s", p->address);
    if (p->xlate)
        ADD(" XLATE=Y");
    if (p->via[0])
        ADD(" VIA=%s", p->via);
    if (p->sessions > 1)
        ADD(" SESSIONS=%d", p->sessions);
#undef ADD
    return (int)n;
}
