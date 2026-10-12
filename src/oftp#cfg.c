/* oftp#cfg.c - OFTPPRM and OFTPPTN: keywords to configuration */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpcfg.h"

static void
say(OFTP_CFGMSG msg, void *ctx, const char *fmt, int lineno, const char *what)
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
    c->ctlport = 3306;
    strcpy(c->inprefix, "OFTP.IN");
    strcpy(c->tmpprefix, "OFTP.TMP");
    c->maxsess = 4;
    c->sdeb = 4096;
    c->cred = 99;
    c->timeout = 60;
    c->tlrecl = 255;
    c->ublksize = 6144;
    c->retry = 300;
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

static int
monitor_kw(OFTP_CONFIG *c, const char *k, const char *v)
{
    long n;

    if (strcmp(k, "ODETTEID") == 0)
        return put(c->odetteid, sizeof(c->odetteid), v);
    if (strcmp(k, "PORT") == 0)
        return (n = num(v, 1, 65535)) < 0 ? -1 : (c->port = (int)n, 0);
    if (strcmp(k, "CONTROLPORT") == 0)
        return (n = num(v, 0, 65535)) < 0 ? -1 : (c->ctlport = (int)n, 0);
    if (strcmp(k, "INPREFIX") == 0)
        return put(c->inprefix, sizeof(c->inprefix), v);
    if (strcmp(k, "TMPPREFIX") == 0)
        return put(c->tmpprefix, sizeof(c->tmpprefix), v);
    if (strcmp(k, "MAXSESS") == 0)
        return (n = num(v, 1, 16)) < 0 ? -1 : (c->maxsess = (int)n, 0);
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
        return (b = yesno(v)) < 0 ? -1 : (p->call = b, 0);
    if (strcmp(k, "ADDRESS") == 0)
        return put(p->address, sizeof(p->address), v);
    if (strcmp(k, "XLATE") == 0)
        return (b = yesno(v)) < 0 ? -1 : (p->xlate = b, 0);
    if (strcmp(k, "VIA") == 0)
        return put(p->via, sizeof(p->via), v);
    return -2;
}

int
oftp_cfg_line(OFTP_CONFIG *c, int partner, const char *text, int lineno,
              OFTP_CFGMSG msg, void *ctx)
{
    const char *p = text;
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
            say(msg, ctx, "line %d: %s is not KEYWORD=VALUE", lineno, tok);
            errors++;
            continue;
        }
        *eq = '\0';
        v = eq + 1;
        for (eq = tok; *eq; eq++)
            *eq = (char)toupper((unsigned char)*eq);
        if (strcmp(tok, "HOST") != 0)
            for (eq = v; *eq; eq++)
                *eq = (char)toupper((unsigned char)*eq);

        if (!partner)
            rc = monitor_kw(c, tok, v);
        else if (strcmp(tok, "PARTNER") == 0) {
            OFTP_PARTNER *np;

            if (c->nptn >= OFTP_MAXPTN) {
                say(msg, ctx, "line %d: more than %s partners", lineno, "32");
                errors++;
                continue;
            }
            np = &c->ptn[c->nptn++];
            memset(np, 0, sizeof(*np));
            np->enabled = 1;
            np->call = 1;
            np->line = lineno;
            rc = put(np->peer.name, sizeof(np->peer.name), v);
            if (rc == 0 && v[0] == '\0')
                rc = -1;
        } else if (c->nptn == 0) {
            say(msg, ctx, "line %d: %s before the first PARTNER=", lineno, tok);
            errors++;
            continue;
        } else
            rc = partner_kw(&c->ptn[c->nptn - 1], tok, v);

        if (rc == -2) {
            say(msg, ctx, "line %d: unknown keyword %s", lineno, tok);
            errors++;
        } else if (rc != 0) {
            say(msg, ctx, "line %d: invalid value for %s", lineno, tok);
            errors++;
        }
        if (rc != 0 && partner && c->nptn > 0)
            c->ptn[c->nptn - 1].enabled = 0;
    }
    c->errors += errors;
    return errors;
}

int
oftp_cfg_finish(OFTP_CONFIG *c, OFTP_CFGMSG msg, void *ctx)
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
        if (p->host[0] == '\0' || p->via[0])
            p->call = 0;
        if (p->enabled)
            on++;
    }
    /* VIA must name an enabled direct partner */
    for (i = 0; i < c->nptn; i++) {
        OFTP_PARTNER *p = &c->ptn[i], *v = NULL;

        if (!p->via[0] || !p->enabled)
            continue;
        for (j = 0; j < c->nptn; j++)
            if (strcmp(c->ptn[j].peer.name, p->via) == 0)
                v = &c->ptn[j];
        if (v == NULL || !v->enabled || v->via[0] || v == p) {
            say(msg, ctx, "line %d: partner %s: VIA names no direct partner, disabled",
                p->line, p->peer.name);
            p->enabled = 0;
            on--;
        }
    }
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
oftp_cfg_byid(OFTP_CONFIG *c, const char *odetteid)
{
    int i;

    for (i = 0; i < c->nptn; i++)
        if (c->ptn[i].enabled && !c->ptn[i].via[0]
            && strcmp(c->ptn[i].peer.code, odetteid) == 0)
            return &c->ptn[i];
    return NULL;
}
