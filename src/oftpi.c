/* OFTPI - the ISPF dialog of oftpd
**
** Started under ISPF as a command: SELECT CMD(OFTPI) NEWAPPL(OFTP). The
** panels OFTP@PRI, OFTPSND, OFTPLST, OFTPLSW, OFTPSHW, OFTPPTL, OFTPPED,
** OFTPPDL, OFTPCFG and OFTPMON (ispplib/) must be in the ISPPLIB concatenation, this
** module in the ISPLLIB one: ISPLINK is loaded from there
** (src/mvs/oftp#isp.c).
**
** Everything the dialog does is a request to OFTPD through its
** cross-memory SVC, as the OFTP utility's is. The partner member's name
** is kept in the user's ISPF profile.
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpcli.h"
#include "oftpisp.h"

/* The dialog variables, each a blank-padded field ISPF reads and writes */
typedef struct vars {
    char oftopt[1], oftmsg[128];
    char oftptn[8], oftdsn[54], oftvfn[26], oftfmt[1], oftxlt[1];
    char ofthld[1], oftusr[8];
    char oftfptn[8], oftfst[10], oftfmin[1];
    char os[1], ok[8], od[1], ot[9], op[8], ov[26], on[44];
    char pn[8], pi[25], pe[8], pb[4], pl[1], ph[72];
    char l[6][128];
    char oftcdsn[54];
    char oftscrw[4];
    /* the partner entry panel (OFTPPED), one variable per attribute */
    char enam[8], eid[25], epw[8], eopw[8], eoid[25], ehst[56], eprt[5];
    char elev[1], emod[1], ecmp[1], ecal[1], eses[2], eadr[15], evia[8];
    char exlt[1], eenb[1], epws[24], eopws[24], ettl[8], enew[8];
} VARS;

typedef struct dlg {
    OFTP_ISP    isp;
    VARS        v;
} DLG;

#define PUT(d, f, s)    oftp_isp_put((d)->v.f, (int)sizeof((d)->v.f), (s))
#define GET(d, f, s)    oftp_isp_get((d)->v.f, (int)sizeof((d)->v.f), (s))

/* The attributes of a partner on the entry panel: the keyword of the
** request, the dialog variable, and whether blank clears a value that
** was set. The password fields are never filled in: SHOW does not tell.
*/
enum { F_ID, F_PW, F_OPW, F_OID, F_HST, F_PRT, F_LEV, F_MOD, F_CMP, F_CAL,
       F_SES, F_ADR, F_VIA, F_XLT, F_ENB, NF };

static const struct { const char *kw, *var; size_t off, len; int clear; }
pf[NF] = {
#define F(k, v, f, c) { k, v, offsetof(VARS, f), sizeof(((VARS *)0)->f), c }
    F("ODETTEID", "EID", eid, 0),    F("PASSWORD", "EPW", epw, 0),
    F("OURPASSWORD", "EOP", eopw, 0), F("OURID", "EOI", eoid, 0),
    F("HOST", "EHS", ehst, 1),      F("PORT", "EPT", eprt, 0),
    F("LEVEL", "ELV", elev, 0),     F("MODE", "EMD", emod, 0),
    F("COMPRESS", "ECM", ecmp, 0),  F("CALL", "ECL", ecal, 0),
    F("SESSIONS", "ESE", eses, 0),  F("ADDRESS", "EAD", eadr, 1),
    F("VIA", "EVI", evia, 1),       F("XLATE", "EXL", exlt, 0),
    F("ENABLED", "EEN", eenb, 0),
#undef F
};

typedef char PVAL[NF][68];              /* one string per attribute */

static int
define(DLG *d)
{
    static const struct { const char *name; size_t off, len; } tab[] = {
#define V(n, f) { n, offsetof(VARS, f), sizeof(((VARS *)0)->f) }
        V("OFTOPT", oftopt), V("OFTMSG", oftmsg),
        V("OFTPTN", oftptn), V("OFTDSN", oftdsn),
        V("OFTVFN", oftvfn), V("OFTFMT", oftfmt), V("OFTXLT", oftxlt),
        V("OFTHLD", ofthld), V("OFTUSR", oftusr), V("OFTFPTN", oftfptn),
        V("OFTFST", oftfst), V("OFTFMIN", oftfmin), V("OS", os),
        V("OK", ok), V("OD", od), V("OT", ot), V("OP", op), V("OV", ov),
        V("ON", on), V("PN", pn), V("PI", pi), V("PE", pe), V("PB", pb),
        V("PL", pl), V("PH", ph), V("OFTL1", l[0]), V("OFTL2", l[1]),
        V("OFTL3", l[2]), V("OFTL4", l[3]), V("OFTL5", l[4]),
        V("OFTL6", l[5]), V("OFTCDSN", oftcdsn), V("OFTSCRW", oftscrw),
        V("OFTENAM", enam), V("OFTEPWS", epws), V("OFTEOPS", eopws),
        V("OFTETTL", ettl), V("OFTNPTN", enew),
#undef V
    };
    unsigned i;

    memset(&d->v, ' ', sizeof(d->v));
    for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
        if (oftp_isp_vdef(&d->isp, tab[i].name, (char *)&d->v + tab[i].off,
                          (int)tab[i].len) != 0)
            return -1;
    for (i = 0; i < NF; i++)
        if (oftp_isp_vdef(&d->isp, pf[i].var, (char *)&d->v + pf[i].off,
                          (int)pf[i].len) != 0)
            return -1;
    return 0;
}

/* A request to OFTPD; the final answer goes into OFTMSG */
static int
ask(DLG *d, const char *req, OFTP_CLILINE line, void *ctx)
{
    char final[200];
    int rc = oftp_cli_request(req, line, ctx, final, sizeof(final));

    PUT(d, oftmsg, final);
    return rc;
}

/* Column cut from an answer line: from..to inclusive, blanks trimmed;
** out holds 80 characters
*/
static void
col(const char *line, int from, int to, char *out)
{
    int n = (int)strlen(line), len;

    out[0] = '\0';
    if (from >= n)
        return;
    if (to < 0 || to >= n)
        to = n - 1;
    len = to - from + 1;
    if (len > 79)
        len = 79;
    memcpy(out, line + from, (size_t)len);
    out[len] = '\0';
    while (len > 0 && out[len - 1] == ' ')
        out[--len] = '\0';
}

/* ---- 1 send ------------------------------------------------------ */

static void partners(DLG *d, int selecting);

static void
send(DLG *d)
{
    char p[9], dsn[55], vfn[27], usr[9], fmt[2], xl[2], hd[2], req[260];
    size_t n;

    PUT(d, oftmsg, "");
    PUT(d, oftfmt, "N");
    PUT(d, oftxlt, "N");
    PUT(d, ofthld, "N");
    for (;;) {
        if (oftp_isp_exec("DISPLAY PANEL(OFTPSND)") != 0)
            return;
        GET(d, oftptn, p);
        if (strcmp(p, "?") == 0) {
            partners(d, 1);
            continue;
        }
        GET(d, oftdsn, dsn);
        GET(d, oftvfn, vfn);
        GET(d, oftusr, usr);
        GET(d, oftfmt, fmt);
        GET(d, oftxlt, xl);
        GET(d, ofthld, hd);
        n = (size_t)snprintf(req, sizeof(req), "SEND PARTNER=%s DSN=%s", p, dsn);
        if (vfn[0])
            n += (size_t)snprintf(req + n, sizeof(req) - n, " VFN=%s", vfn);
        if (fmt[0] == 'Y')
            n += (size_t)snprintf(req + n, sizeof(req) - n, " FMT=T");
        if (xl[0] == 'Y')
            n += (size_t)snprintf(req + n, sizeof(req) - n, " XLATE=Y");
        if (hd[0] == 'Y')
            n += (size_t)snprintf(req + n, sizeof(req) - n, " HOLD=Y");
        if (usr[0])
            snprintf(req + n, sizeof(req) - n, " USER=%s", usr);
        ask(d, req, NULL, NULL);
    }
}

/* ---- 2 transfers ------------------------------------------------- */

/* One LIST line: key 0-7, direction 9-12, state 14-22, partner 24-31,
** virtual file 33-58, data set from 60 (src/mvs/oftp#mon.c ctl_list)
*/
static void
add_transfer(void *ctx, const char *line)
{
    DLG *d = ctx;
    char f[80];

    PUT(d, os, "");
    col(line, 0, 7, f);   PUT(d, ok, f);
    col(line, 9, 9, f);   PUT(d, od, f);     /* S(END) or R(ECV) */
    col(line, 14, 22, f); PUT(d, ot, f);
    col(line, 24, 31, f); PUT(d, op, f);
    col(line, 33, 58, f); PUT(d, ov, f);
    col(line, 60, -1, f); PUT(d, on, f);
    oftp_isp_exec("TBADD OFTPTRN");
}

static void
show_line(void *ctx, const char *line)
{
    DLG *d = ctx;
    int i;

    for (i = 0; i < 6; i++)
        if (d->v.l[i][0] == ' ' && d->v.l[i][1] == ' ') {
            oftp_isp_put(d->v.l[i], (int)sizeof(d->v.l[i]), line);
            return;
        }
}

static void
show(DLG *d, const char *key)
{
    char req[40];

    memset(d->v.l, ' ', sizeof(d->v.l));
    snprintf(req, sizeof(req), "SHOW KEY=%s", key);
    if (ask(d, req, show_line, d) == 0)
        PUT(d, oftmsg, "");          /* "end" says nothing */
    oftp_isp_exec("DISPLAY PANEL(OFTPSHW)");
}

static void
transfers(DLG *d)
{
    char fp[9], fs[11], fm[2], sel[2], key[9], req[120], keep[129];
    const char *panel = "OFTPLST";
    size_t n;
    int rc;

    PUT(d, oftmsg, "");
    PUT(d, oftfmin, "N");
    /* the data set fits on the line from 110 columns on, below that
    ** each transfer takes two lines; OFTP@PRI copied ZSCREENW
    */
    {
        char w[5];

        GET(d, oftscrw, w);
        if (atoi(w) >= 110)
            panel = "OFTPLSW";
    }
    for (;;) {
        GET(d, oftfptn, fp);
        GET(d, oftfst, fs);
        GET(d, oftfmin, fm);
        n = (size_t)snprintf(req, sizeof(req), "LIST");
        if (fp[0])
            n += (size_t)snprintf(req + n, sizeof(req) - n, " PARTNER=%s", fp);
        if (fs[0])
            n += (size_t)snprintf(req + n, sizeof(req) - n, " STATUS=%s", fs);
        if (fm[0] == 'Y')
            snprintf(req + n, sizeof(req) - n, " MINE=Y");

        oftp_isp_exec("TBCREATE OFTPTRN NAMES(OS OK OD OT OP OV ON) "
                      "NOWRITE REPLACE");
        GET(d, oftmsg, keep);
        ask(d, req, add_transfer, d);
        if (keep[0])                    /* an action's answer stays */
            PUT(d, oftmsg, keep);
        oftp_isp_exec("TBTOP OFTPTRN");
        PUT(d, os, "");
        rc = oftp_isp_exec("TBDISPL OFTPTRN PANEL(%s)", panel);
        GET(d, os, sel);
        GET(d, ok, key);
        oftp_isp_exec("TBEND OFTPTRN");
        if (rc >= 8)
            return;
        PUT(d, oftmsg, "");
        if (sel[0] == '\0' || key[0] == '\0')
            continue;                   /* new filters, or just Enter */
        switch (sel[0]) {
        case 'S':
            show(d, key);
            PUT(d, oftmsg, "");
            break;
        case 'H':
            snprintf(req, sizeof(req), "HOLD KEY=%s", key);
            ask(d, req, NULL, NULL);
            break;
        case 'R':
            snprintf(req, sizeof(req), "RELEASE KEY=%s", key);
            ask(d, req, NULL, NULL);
            break;
        case 'T':
            snprintf(req, sizeof(req), "RETRY KEY=%s", key);
            ask(d, req, NULL, NULL);
            break;
        case 'D':
            snprintf(req, sizeof(req), "DELETE KEY=%s", key);
            ask(d, req, NULL, NULL);
            break;
        default:
            PUT(d, oftmsg, "Line commands: S show, H hold, R release, "
                "T retry, D delete");
        }
    }
}

/* ---- 3 partners -------------------------------------------------- */

/* One PARTNERS line: name 0-7, Odette ID 9-33, state 35-42, busy 44-47,
** "LEVEL n" 49-55, host from 57 (ctl_partners)
*/
static void
add_partner(void *ctx, const char *line)
{
    DLG *d = ctx;
    char f[80];

    PUT(d, os, "");
    col(line, 0, 7, f);   PUT(d, pn, f);
    col(line, 9, 33, f);  PUT(d, pi, f);
    col(line, 35, 42, f); PUT(d, pe, f);
    col(line, 44, 47, f); PUT(d, pb, f);
    col(line, 55, 55, f); PUT(d, pl, f);
    col(line, 57, -1, f); PUT(d, ph, f);
    oftp_isp_exec("TBADD OFTPPTN");
}

/* One KEYWORD=VALUE of PARTNER SHOW into its dialog variable */
static void
shown_value(DLG *d, const char *kw, const char *v)
{
    unsigned i;

    if (strcmp(kw, "PASSWORD") == 0) {
        PUT(d, epws, strcmp(v, "SET") == 0 ? "(set)" : "(none)");
        return;
    }
    if (strcmp(kw, "OURPASSWORD") == 0) {
        PUT(d, eopws, strcmp(v, "SET") == 0 ? "(set)" : "(none)");
        return;
    }
    for (i = 0; i < NF; i++)
        if (strcmp(kw, pf[i].kw) == 0) {
            oftp_isp_put((char *)&d->v + pf[i].off, (int)pf[i].len, v);
            return;
        }
}

/* One row of PARTNER SHOW: KEYWORD=VALUE pairs; the other words, the
** "source" row's, are skipped. The passwords are only told as SET or NONE.
*/
static void
shown(void *ctx, const char *line)
{
    DLG *d = ctx;
    const char *p = line;

    while (*p) {
        char tok[100], *eq;
        size_t n = 0;

        while (*p == ' ')
            p++;
        while (*p && *p != ' ') {
            if (n < sizeof(tok) - 1)
                tok[n++] = *p;
            p++;
        }
        tok[n] = '\0';
        if ((eq = strchr(tok, '=')) == NULL)
            continue;
        *eq++ = '\0';
        shown_value(d, tok, eq);
    }
}

/* Appends " KEYWORD=VALUE" to req; -1 when it does not fit */
static int
addkw(char *req, size_t cap, size_t *n, const char *kw, const char *v)
{
    int k = snprintf(req + *n, cap - *n, " %s=%s", kw, v);

    if (k < 0 || (size_t)k >= cap - *n)
        return -1;
    *n += (size_t)k;
    return 0;
}

/* Does field i go into the request? ADD: what was filled in. CHANGE: what
** differs from the old value; a blank keeps the value, except where blank
** clears it, and a blank password is never sent.
*/
static int
sent(int i, int adding, const char *old, const char *now)
{
    if (adding || i == F_PW || i == F_OPW)
        return now[0] != '\0';
    if (strcmp(now, old) == 0)
        return 0;
    return now[0] != '\0' || pf[i].clear;
}

/* The panel variables for the entry, and the values they start with */
static int
pentry_init(DLG *d, const char *name, int adding, PVAL old)
{
    char req[60];
    int i;

    for (i = 0; i < NF; i++)
        memset((char *)&d->v + pf[i].off, ' ', pf[i].len);
    PUT(d, enam, name);
    PUT(d, ettl, adding ? "Add" : "Change");
    PUT(d, epws, "(none)");
    PUT(d, eopws, "(none)");
    PUT(d, oftmsg, "");
    memset(old, 0, sizeof(PVAL));
    if (adding) {
        PUT(d, elev, "5");
        PUT(d, emod, "B");
        PUT(d, ecmp, "N");
        PUT(d, ecal, "Y");
        PUT(d, eses, "1");
        PUT(d, exlt, "N");
        PUT(d, eenb, "Y");
    } else {
        snprintf(req, sizeof(req), "PARTNER SHOW PARTNER=%s", name);
        if (ask(d, req, shown, d) != 0)
            return -1;
        PUT(d, oftmsg, "");
    }
    for (i = 0; i < NF; i++)
        oftp_isp_get((char *)&d->v + pf[i].off, (int)pf[i].len, old[i]);
    return 0;
}

/* The entry panel for a new partner or for one that is there. Returns 1
** when OFTPD took the change, whose answer is then in OFTMSG; 0 when the
** user left or nothing was to do.
*/
static int
pentry(DLG *d, const char *name, int adding)
{
    PVAL old, now;
    char req[256];
    size_t n;
    int i, rc;

    if (pentry_init(d, name, adding, old) != 0)
        return 0;

    for (;;) {
        if (oftp_isp_exec("DISPLAY PANEL(OFTPPED)") != 0)
            return 0;
        for (i = 0; i < NF; i++)
            oftp_isp_get((char *)&d->v + pf[i].off, (int)pf[i].len, now[i]);
        n = (size_t)snprintf(req, sizeof(req), "PARTNER %s PARTNER=%s",
                             adding ? "ADD" : "CHANGE", name);
        rc = 0;
        for (i = 0; i < NF; i++) {
            if (!sent(i, adding, old[i], now[i]))
                continue;
            rc = addkw(req, sizeof(req), &n, pf[i].kw, now[i]);
            if (rc != 0)
                break;
        }
        if (rc != 0) {
            PUT(d, oftmsg, "The entries do not fit one request");
            continue;
        }
        if (!adding && strlen(req) == strlen("PARTNER CHANGE PARTNER=")
                                      + strlen(name)) {
            PUT(d, oftmsg, "Nothing was changed");
            continue;
        }
        if (ask(d, req, NULL, NULL) == 0)
            return 1;
    }
}

static void
pdelete(DLG *d, const char *name)
{
    char req[40];

    PUT(d, enam, name);
    PUT(d, oftmsg, "");
    if (oftp_isp_exec("DISPLAY PANEL(OFTPPDL)") != 0)
        return;
    snprintf(req, sizeof(req), "PARTNER DELETE PARTNER=%s", name);
    ask(d, req, NULL, NULL);
}

/* A line command of the partner list other than S of a selection */
static void
pline(DLG *d, char cmd, const char *name, int selecting)
{
    char req[60];

    if (cmd == 'C') {
        snprintf(req, sizeof(req), "CALL PARTNER=%s", name);
        ask(d, req, NULL, NULL);
    } else if (selecting)
        PUT(d, oftmsg, "S selects a partner, C calls it");
    else if (cmd == 'M') {
        if (pentry(d, name, 0) == 0)
            PUT(d, oftmsg, "");
    } else if (cmd == 'E' || cmd == 'X') {
        snprintf(req, sizeof(req), "PARTNER CHANGE PARTNER=%s ENABLED=%c",
                 name, cmd == 'E' ? 'Y' : 'N');
        ask(d, req, NULL, NULL);
    } else if (cmd == 'D')
        pdelete(d, name);
    else
        PUT(d, oftmsg, "Line commands: C call, M modify, E enable, "
            "X disable, D delete");
}

/* selecting: from Send's '?', S puts the partner into OFTPTN and the line
** commands that change the table are not offered
*/
static void
partners(DLG *d, int selecting)
{
    char sel[2], name[9], req[60], keep[129], add[9];
    int rc;

    if (!selecting)
        PUT(d, oftmsg, "");
    for (;;) {
        oftp_isp_exec("TBCREATE OFTPPTN NAMES(OS PN PI PE PB PL PH) "
                      "NOWRITE REPLACE");
        GET(d, oftmsg, keep);
        ask(d, "PARTNERS", add_partner, d);
        if (keep[0])
            PUT(d, oftmsg, keep);
        oftp_isp_exec("TBTOP OFTPPTN");
        PUT(d, os, "");
        PUT(d, enew, "");
        rc = oftp_isp_exec("TBDISPL OFTPPTN PANEL(OFTPPTL)");
        GET(d, os, sel);
        GET(d, pn, name);
        GET(d, enew, add);
        oftp_isp_exec("TBEND OFTPPTN");
        if (rc >= 8) {
            if (selecting)
                PUT(d, oftptn, "");
            PUT(d, oftmsg, "");
            return;
        }
        PUT(d, oftmsg, "");
        if (add[0] && selecting) {
            PUT(d, oftmsg, "Partners are added from option 3 of the menu");
            continue;
        }
        if (add[0]) {
            snprintf(req, sizeof(req), "PARTNER SHOW PARTNER=%s", add);
            if (ask(d, req, NULL, NULL) == 0)
                PUT(d, oftmsg, "The partner is there: M changes it");
            else if (pentry(d, add, 1) == 0)
                PUT(d, oftmsg, "");
            continue;
        }
        if (sel[0] == '\0' || name[0] == '\0')
            continue;
        if (sel[0] == 'S' && selecting) {
            PUT(d, oftptn, name);
            PUT(d, oftmsg, "");
            return;
        }
        pline(d, sel[0], name, selecting);
    }
}

/* ---- 4 configure, 5 monitor -------------------------------------- */

static void
configure(DLG *d)
{
    char dsn[55];

    PUT(d, oftmsg, "");
    for (;;) {
        if (oftp_isp_exec("DISPLAY PANEL(OFTPCFG)") != 0)
            return;
        GET(d, oftcdsn, dsn);
        oftp_isp_exec("VPUT (OFTCDSN) PROFILE");
        if (oftp_isp_exec("EDIT DATASET('%s')", dsn) > 4) {
            PUT(d, oftmsg, "The member cannot be edited");
            continue;
        }
        ask(d, "REFRESH", NULL, NULL);
    }
}

static void
monitor(DLG *d)
{
    PUT(d, oftmsg, "");
    for (;;) {
        memset(d->v.l, ' ', sizeof(d->v.l));
        if (ask(d, "STATUS", show_line, d) == 0)
            PUT(d, oftmsg, "");
        if (oftp_isp_exec("DISPLAY PANEL(OFTPMON)") != 0)
            return;
    }
}

int
main(void)
{
    DLG *d;
    char opt[2], dsn[55];

    if ((d = calloc(1, sizeof(*d))) == NULL)
        return 8;
    if (oftp_isp_init(&d->isp) != 0 || define(d) != 0) {
        FILE *out = fopen("*PUTLINE", "w");

        fprintf(out ? out : stdout,
                "OFTPI001E OFTPI RUNS UNDER ISPF: SELECT CMD(OFTPI)\n");
        if (out)
            fclose(out);
        free(d);
        return 8;
    }
    oftp_isp_exec("VGET (OFTCDSN) PROFILE");
    GET(d, oftcdsn, dsn);
    if (dsn[0] == '\0')
        PUT(d, oftcdsn, "SYS2.PARMLIB(OFTPPTN0)");

    for (;;) {
        PUT(d, oftopt, "");
        if (oftp_isp_exec("DISPLAY PANEL(OFTP@PRI)") != 0)
            break;
        GET(d, oftopt, opt);
        PUT(d, oftmsg, "");
        switch (opt[0]) {
        case '1': send(d);       break;
        case '2': transfers(d);  break;
        case '3': partners(d, 0); break;
        case '4': configure(d);  break;
        case '5': monitor(d);    break;
        case 'X': free(d);       return 0;
        default:  break;
        }
    }
    free(d);
    return 0;
}
