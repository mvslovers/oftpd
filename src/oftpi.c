/* OFTPI - the ISPF dialog of oftpd
**
** Started under ISPF as a command: SELECT CMD(OFTPI) NEWAPPL(OFTP). The
** panels OFTP@PRI, OFTPSND, OFTPLST, OFTPLSW, OFTPSHW, OFTPPTL, OFTPCFG
** and OFTPMON (ispplib/) must be in the ISPPLIB concatenation, this
** module in the ISPLLIB one: ISPLINK is loaded from there
** (src/mvs/oftp#isp.c).
**
** Everything the dialog does is a request to OFTPD, as the OFTP utility's
** is: through the cross-memory SVC, or through the TCP control port when
** a host is set (option 5; development only, no security). Host, port
** and the partner member's name are kept in the user's ISPF profile.
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpcli.h"
#include "oftpisp.h"

/* The dialog variables, each a blank-padded field ISPF reads and writes */
typedef struct vars {
    char oftopt[1], oftmsg[128], ofthost[60], oftport[5];
    char oftptn[8], oftdsn[54], oftvfn[26], oftfmt[1], oftxlt[1];
    char ofthld[1], oftusr[8];
    char oftfptn[8], oftfst[10], oftfmin[1];
    char os[1], ok[8], od[1], ot[9], op[8], ov[26], on[44];
    char pn[8], pi[25], pe[8], pb[4], pl[1], ph[72];
    char l[6][128];
    char oftcdsn[54];
    char oftscrw[4];
    char oftvia[60];
} VARS;

typedef struct dlg {
    OFTP_ISP    isp;
    VARS        v;
    char        me[9];
    char        host[61];
    int         port;
} DLG;

#define PUT(d, f, s)    oftp_isp_put((d)->v.f, (int)sizeof((d)->v.f), (s))
#define GET(d, f, s)    oftp_isp_get((d)->v.f, (int)sizeof((d)->v.f), (s))

static int
define(DLG *d)
{
    static const struct { const char *name; size_t off, len; } tab[] = {
#define V(n, f) { n, offsetof(VARS, f), sizeof(((VARS *)0)->f) }
        V("OFTOPT", oftopt), V("OFTMSG", oftmsg), V("OFTHOST", ofthost),
        V("OFTPORT", oftport), V("OFTPTN", oftptn), V("OFTDSN", oftdsn),
        V("OFTVFN", oftvfn), V("OFTFMT", oftfmt), V("OFTXLT", oftxlt),
        V("OFTHLD", ofthld), V("OFTUSR", oftusr), V("OFTFPTN", oftfptn),
        V("OFTFST", oftfst), V("OFTFMIN", oftfmin), V("OS", os),
        V("OK", ok), V("OD", od), V("OT", ot), V("OP", op), V("OV", ov),
        V("ON", on), V("PN", pn), V("PI", pi), V("PE", pe), V("PB", pb),
        V("PL", pl), V("PH", ph), V("OFTL1", l[0]), V("OFTL2", l[1]),
        V("OFTL3", l[2]), V("OFTL4", l[3]), V("OFTL5", l[4]),
        V("OFTL6", l[5]), V("OFTCDSN", oftcdsn), V("OFTSCRW", oftscrw),
        V("OFTVIA", oftvia),
#undef V
    };
    unsigned i;

    memset(&d->v, ' ', sizeof(d->v));
    for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
        if (oftp_isp_vdef(&d->isp, tab[i].name, (char *)&d->v + tab[i].off,
                          (int)tab[i].len) != 0)
            return -1;
    return 0;
}

/* How OFTPD is reached, for the panels: no host is the SVC */
static void
via(DLG *d)
{
    char t[80];

    if (d->host[0])
        snprintf(t, sizeof(t), "control port %s port %d", d->host, d->port);
    else
        snprintf(t, sizeof(t), "cross memory (SVC)");
    PUT(d, oftvia, t);
}

/* A request to OFTPD; the final answer goes into OFTMSG */
static int
ask(DLG *d, const char *req, OFTP_CLILINE line, void *ctx)
{
    char final[200];
    int rc = oftp_cli_request(d->host, d->port, req, line, ctx, final,
                              sizeof(final));

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
            n += (size_t)snprintf(req + n, sizeof(req) - n, " USER=%s", usr);
        if (d->me[0])
            snprintf(req + n, sizeof(req) - n, " OWNER=%s", d->me);
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
        /* MINE=Y for the SVC, OWNER= for the control port */
        if (fm[0] == 'Y' && d->me[0])
            snprintf(req + n, sizeof(req) - n, " MINE=Y OWNER=%s", d->me);

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

/* selecting: from Send's '?', S puts the partner into OFTPTN */
static void
partners(DLG *d, int selecting)
{
    char sel[2], name[9], req[40], keep[129];
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
        rc = oftp_isp_exec("TBDISPL OFTPPTN PANEL(OFTPPTL)");
        GET(d, os, sel);
        GET(d, pn, name);
        oftp_isp_exec("TBEND OFTPPTN");
        if (rc >= 8) {
            if (selecting)
                PUT(d, oftptn, "");
            PUT(d, oftmsg, "");
            return;
        }
        PUT(d, oftmsg, "");
        if (sel[0] == '\0' || name[0] == '\0')
            continue;
        if (sel[0] == 'S' && selecting) {
            PUT(d, oftptn, name);
            PUT(d, oftmsg, "");
            return;
        }
        if (sel[0] == 'C') {
            snprintf(req, sizeof(req), "CALL PARTNER=%s", name);
            ask(d, req, NULL, NULL);
        } else
            PUT(d, oftmsg, selecting ? "S selects a partner, C calls it"
                                     : "C calls a partner now");
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
    char host[61], port[6];

    PUT(d, oftmsg, "");
    for (;;) {
        memset(d->v.l, ' ', sizeof(d->v.l));
        if (ask(d, "STATUS", show_line, d) == 0)
            PUT(d, oftmsg, "");
        if (oftp_isp_exec("DISPLAY PANEL(OFTPMON)") != 0)
            return;
        GET(d, ofthost, host);
        GET(d, oftport, port);
        snprintf(d->host, sizeof(d->host), "%s", host);
        d->port = port[0] ? atoi(port) : 3306;
        via(d);
        oftp_isp_exec("VPUT (OFTHOST OFTPORT) PROFILE");
    }
}

int
main(void)
{
    DLG *d;
    char opt[2], host[61], port[6], dsn[55];

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
    oftp_cli_whoami(d->me);
    oftp_isp_exec("VGET (OFTHOST OFTPORT OFTCDSN) PROFILE");
    GET(d, ofthost, host);
    GET(d, oftport, port);
    GET(d, oftcdsn, dsn);
    /* no host is the SVC; a profile from before it keeps its host, and
    ** option 5 clears it
    */
    if (port[0] == '\0')
        PUT(d, oftport, strcpy(port, "3306"));
    if (dsn[0] == '\0')
        PUT(d, oftcdsn, "SYS2.PARMLIB(OFTPPTN0)");
    snprintf(d->host, sizeof(d->host), "%s", host);
    d->port = atoi(port);
    via(d);

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
