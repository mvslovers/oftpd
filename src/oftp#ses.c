/* oftp#ses.c - one OFTP session: negotiation, turns, files, EERP/NERP
**
** Sequential: each function sends what it must and reads the answer it
** expects. Anything else from the partner is a protocol violation and
** ends the session with ESID 02, except an ESID, which ends it with the
** partner's reason. Rules: internals/protocol.md section 1.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpdata.h"
#include "oftpsess.h"
#include "oftpwire.h"

#define OK      0
#define END     1               /* the session has ended; s->esid holds why */

/* speak() outcomes */
#define SPK_NONE    2           /* had nothing to send                       */
#define SPK_SENT    3           /* sent files or responses                   */
#define SPK_TURN    4           /* the partner asked for the turn            */

/* listen() outcome */
#define LSN_CD      5

/* A 2.0 SFID is 165 octets before its description, so a 2.0 session
** cannot run on the protocol's minimum buffer of 128.
*/
#define SDEB_MIN20  165

/* Piece size for reading U and T files */
#define CHUNK       4096

/* ESID reason codes used here */
#define ESID_NORMAL     0
#define ESID_UNKNOWN    1
#define ESID_PROTOCOL   2
#define ESID_USER       3
#define ESID_PASSWORD   4
#define ESID_DATA       6
#define ESID_BUFSIZE    7
#define ESID_RESOURCE   8
#define ESID_TIMEOUT    9
#define ESID_MODE       10
#define ESID_AUTH       12
#define ESID_OTHER      99

static void
slog(OFTP_SESS *s, const char *fmt, ...)
{
    char line[160];
    va_list ap;

    if (s->log == NULL)
        return;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s->log(s->logctx, line);
}

/* The level commands are coded at; before negotiation the 1.3 layouts,
** which every partner reads.
*/
static int
lev(OFTP_SESS *s)
{
    return s->level > 0 ? s->level : OFTP_LEV13;
}

/* ---- framing ----------------------------------------------------- */

/* Sends the command of n octets at s->ob + 4 */
static int
put_cmd(OFTP_SESS *s, unsigned n)
{
    unsigned len = n + 4;

    s->ob[0] = 0x10;                    /* version 1, no flags */
    s->ob[1] = (unsigned char)((len >> 16) & 0xFF);
    s->ob[2] = (unsigned char)((len >> 8) & 0xFF);
    s->ob[3] = (unsigned char)(len & 0xFF);
    if (s->x.send(s->x.ctx, s->ob, len) != 0) {
        slog(s, "send failed");
        s->esid = ESID_OTHER;
        s->esid_local = 1;
        return END;
    }
    return OK;
}

/* Ends the session from our side */
static int
end_local(OFTP_SESS *s, int reas)
{
    OFTP_REASCMD r;
    int n;

    memset(&r, 0, sizeof(r));
    r.reas = reas;
    s->esid = reas;
    s->esid_local = 1;
    n = oftp_enc_reas(lev(s), OFTP_ESID, &r, s->ob + 4, OFTP_SDEB_MAX);
    if (n > 0)
        put_cmd(s, (unsigned)n);
    s->esid = reas;
    s->esid_local = 1;
    slog(s, "session ended by us, ESID %02d", reas);
    return END;
}

/* Receives one command into s->ib + 4; *n its length */
static int
get_cmd(OFTP_SESS *s, unsigned *n)
{
    unsigned max = s->sdeb > 0 ? s->sdeb : OFTP_SDEB_MAX;
    unsigned len;
    int rc;

    rc = s->x.recv(s->x.ctx, s->ib, 4, s->timeout);
    if (rc == 0) {
        if ((s->ib[0] & 0xF0) != 0x10 || (s->ib[0] & 0x0F) != 0) {
            slog(s, "bad stream header %02X", s->ib[0]);
            return end_local(s, ESID_BUFSIZE);
        }
        len = ((unsigned)s->ib[1] << 16) | ((unsigned)s->ib[2] << 8)
              | s->ib[3];
        if (len < 5 || len - 4 > max) {
            slog(s, "stream length %u outside 5..%u", len, max + 4);
            return end_local(s, ESID_BUFSIZE);
        }
        rc = s->x.recv(s->x.ctx, s->ib + 4, len - 4, s->timeout);
        *n = len - 4;
    }
    if (rc == OFTP_XTIMEOUT) {
        slog(s, "no answer within %d s", s->timeout);
        return end_local(s, ESID_TIMEOUT);
    }
    if (rc != 0) {
        slog(s, "connection lost (%d)", rc);
        s->esid = ESID_OTHER;
        s->esid_local = 1;
        return END;
    }
    return OK;
}

/* The partner's ESID, or a violation if the command is anything else */
static int
unexpected(OFTP_SESS *s, unsigned n, const char *want)
{
    const unsigned char *c = s->ib + 4;
    char code = oftp_cmd_code(c, n);
    OFTP_REASCMD r;

    if (code == OFTP_ESID) {
        if (oftp_dec_reas(lev(s), OFTP_ESID, c, n, &r) != 0) {
            /* a lower-level ESID during negotiation is still an ESID */
            if (oftp_dec_reas(OFTP_LEV13, OFTP_ESID, c, n, &r) != 0)
                r.reas = ESID_OTHER;
        }
        s->esid = r.reas;
        s->esid_local = 0;
        slog(s, "session ended by partner, ESID %02d", r.reas);
        return END;
    }
    slog(s, "expected %s, received command '%c'", want, code);
    return end_local(s, code == '\0' ? ESID_UNKNOWN : ESID_PROTOCOL);
}

/* Sends a one-octet command, or CDT */
static int
put_simple(OFTP_SESS *s, char code)
{
    int n = code == OFTP_CDT ? oftp_enc_cdt(s->ob + 4, s->sdeb)
                             : oftp_enc_cmd1(code, s->ob + 4, s->sdeb);
    return put_cmd(s, (unsigned)n);
}

/* Waits for a one-octet command, or CDT */
static int
get_simple(OFTP_SESS *s, char code, const char *name)
{
    unsigned n;
    int rc;

    if (get_cmd(s, &n) != OK)
        return END;
    rc = code == OFTP_CDT ? oftp_dec_cdt(s->ib + 4, n)
                          : oftp_dec_cmd1(code, s->ib + 4, n);
    if (rc == OFTP_ECMD)
        return unexpected(s, n, name);
    if (rc != 0)
        return end_local(s, ESID_DATA);
    return OK;
}

/* ---- session start ----------------------------------------------- */

static unsigned
umin(unsigned a, unsigned b)
{
    return a < b ? a : b;
}

static void
our_ssid(OFTP_SESS *s, OFTP_SSIDCMD *c, int level, unsigned sdeb,
         unsigned cred, char mode, int cmpr)
{
    memset(c, 0, sizeof(*c));
    c->level = level;
    strcpy(c->code, s->peer.ourcode[0] ? s->peer.ourcode : s->localcode);
    strcpy(c->pswd, s->peer.pswd_out);
    c->sdeb = sdeb;
    c->sr = mode;
    c->cmpr = cmpr ? 'Y' : 'N';
    c->rest = 'N';
    c->spec = 'N';
    c->cred = cred;
    c->auth = 'N';
}

static int
known_level(int l)
{
    return l == OFTP_LEV12 || l == OFTP_LEV13 || l == OFTP_LEV14
           || l == OFTP_LEV20;
}

static int
start_initiator(OFTP_SESS *s)
{
    OFTP_SSIDCMD o, t;
    unsigned n;
    int rc;

    if (get_cmd(s, &n) != OK)
        return END;
    if ((rc = oftp_dec_ssrm(s->ib + 4, n)) == OFTP_ECMD)
        return unexpected(s, n, "SSRM");
    if (rc != 0)
        return end_local(s, ESID_DATA);

    our_ssid(s, &o, s->peer.level, umin(s->peer.sdeb, OFTP_SDEB_MAX),
             s->peer.cred, s->peer.mode, s->peer.cmpr);
    rc = oftp_enc_ssid(&o, s->ob + 4, OFTP_SDEB_MAX);
    if (rc < 0 || put_cmd(s, (unsigned)rc) != OK)
        return END;

    if (get_cmd(s, &n) != OK)
        return END;
    if ((rc = oftp_dec_ssid(s->ib + 4, n, &t)) == OFTP_ECMD)
        return unexpected(s, n, "SSID");
    if (rc != 0)
        return end_local(s, ESID_DATA);

    s->level = known_level(t.level) && t.level <= o.level ? t.level : 0;
    if (strcmp(t.code, s->peer.code) != 0) {
        slog(s, "answered by %s, not %s", t.code, s->peer.code);
        return end_local(s, ESID_USER);
    }
    if (strcmp(t.pswd, s->peer.pswd_in) != 0) {
        slog(s, "partner password does not match");
        return end_local(s, ESID_PASSWORD);
    }
    if (s->level == 0 || t.sdeb < 128 || t.sdeb > o.sdeb || t.cred < 1
        || t.cred > o.cred || t.rest == 'Y' || t.spec == 'Y'
        || (t.cmpr == 'Y' && o.cmpr != 'Y')) {
        slog(s, "answer not compatible: level %d buffer %u credit %u",
             t.level, t.sdeb, t.cred);
        return end_local(s, ESID_MODE);
    }
    if (t.auth == 'Y')
        return end_local(s, ESID_AUTH);

    /* our mode follows from the responder's answer */
    if (o.sr == 'B' && t.sr == 'S')
        s->mode = 'R';
    else if (o.sr == 'B' && t.sr == 'R')
        s->mode = 'S';
    else if ((o.sr == 'B' && t.sr == 'B') || (o.sr == 'S' && t.sr == 'R')
             || (o.sr == 'R' && t.sr == 'S'))
        s->mode = o.sr;
    else
        return end_local(s, ESID_MODE);

    if (s->level >= OFTP_LEV20 && t.sdeb < SDEB_MIN20) {
        slog(s, "buffer %u too small for level 5", t.sdeb);
        return end_local(s, ESID_MODE);
    }
    s->sdeb = t.sdeb;
    s->cred = t.cred;
    s->cmpr = t.cmpr == 'Y';
    return OK;
}

static int
start_responder(OFTP_SESS *s)
{
    OFTP_SSIDCMD o, t;
    unsigned n;
    char mode;
    int rc, level;

    s->sdeb = 0;
    rc = oftp_enc_ssrm(s->ob + 4, OFTP_SDEB_MAX);
    if (put_cmd(s, (unsigned)rc) != OK)
        return END;

    if (get_cmd(s, &n) != OK)
        return END;
    if ((rc = oftp_dec_ssid(s->ib + 4, n, &t)) == OFTP_ECMD)
        return unexpected(s, n, "SSID");
    if (rc != 0)
        return end_local(s, ESID_DATA);

    if (s->p.lookup(s->p.ctx, t.code, &s->peer) != 0) {
        slog(s, "unknown caller %s", t.code);
        return end_local(s, ESID_USER);
    }
    if (strcmp(t.pswd, s->peer.pswd_in) != 0) {
        slog(s, "wrong password from %s", s->peer.name);
        return end_local(s, ESID_PASSWORD);
    }

    /* the lowest level wins; 3 does not exist and is taken as 1.3 */
    level = t.level < s->peer.level ? t.level : s->peer.level;
    if (level == 3)
        level = OFTP_LEV13;
    if (!known_level(level) || t.sdeb < 128 || t.cred < 1)
        return end_local(s, ESID_MODE);
    s->level = level;
    if (t.auth == 'Y')
        return end_local(s, ESID_AUTH);

    if (t.sr == 'S')
        mode = 'R';
    else if (t.sr == 'R')
        mode = 'S';
    else
        mode = s->peer.mode;
    if (s->peer.mode != 'B' && s->peer.mode != mode)
        return end_local(s, ESID_MODE);

    s->sdeb = umin(umin(t.sdeb, s->peer.sdeb), OFTP_SDEB_MAX);
    if (level >= OFTP_LEV20 && s->sdeb < SDEB_MIN20) {
        slog(s, "buffer %u too small for level 5", s->sdeb);
        return end_local(s, ESID_MODE);
    }
    s->cred = umin(t.cred, s->peer.cred);
    s->cmpr = t.cmpr == 'Y' && s->peer.cmpr;
    s->mode = mode;

    our_ssid(s, &o, level, s->sdeb, s->cred, mode, s->cmpr);
    rc = oftp_enc_ssid(&o, s->ob + 4, OFTP_SDEB_MAX);
    if (rc < 0)
        return end_local(s, ESID_OTHER);
    return put_cmd(s, (unsigned)rc);
}

/* The record buffer of a file goes through s->xbuf, so that a session
** left by an abend can still be freed (oftp_sess_free)
*/
static void
drop_buf(OFTP_SESS *s)
{
    free(s->xbuf);
    s->xbuf = NULL;
}

/* ---- sending ----------------------------------------------------- */

/* Sends the DATA buffer in e, waiting for credit first if none is left */
static int
send_data(OFTP_SESS *s, OFTP_DENC *e, unsigned *credit)
{
    if (*credit == 0) {
        if (get_simple(s, OFTP_CDT, "CDT") != OK)
            return END;
        *credit = s->cred;
    }
    if (put_cmd(s, e->len) != OK)
        return END;
    (*credit)--;
    oftp_denc_init(e, s->ob + 4, s->sdeb, s->cmpr);
    return OK;
}

/* Puts n octets into DATA buffers, sending each full one */
static int
send_piece(OFTP_SESS *s, OFTP_DENC *e, unsigned *credit,
           const unsigned char *b, unsigned n, int eor)
{
    unsigned off = 0;
    int done;

    for (;;) {
        off += oftp_denc_put(e, b + off, n - off, eor, &done);
        if (done)
            return OK;
        if (send_data(s, e, credit) != OK)
            return END;
    }
}

/* One file: SFID ... EFPA/EFNA. *turn set when the partner asked for
** the turn.
*/
static int
send_file(OFTP_SESS *s, OFTP_SFIDCMD *f, OFTP_FH *fh, int *turn)
{
    OFTP_EFIDCMD efid;
    OFTP_REASCMD r;
    OFTP_DENC e;
    unsigned char *buf;
    unsigned cap, n, credit;
    unsigned long acnt;
    int rc, last = 0;
    char cd, code;

    f->rest = 0;
    f->osiz = f->fsiz;
    f->sec = f->ciph = f->comp = f->env = 0;
    f->sign = 'N';
    f->descl = 0;
    f->desc = NULL;
    rc = oftp_enc_sfid(s->level, f, s->ob + 4, s->sdeb);
    if (rc < 0) {
        slog(s, "%s: cannot be described in an SFID (%d)", f->dsn, rc);
        s->f.out_done(s->f.ctx, fh, 99, 'N');
        return OK;
    }
    if (put_cmd(s, (unsigned)rc) != OK)
        return END;
    if (get_cmd(s, &n) != OK)
        return END;
    code = oftp_cmd_code(s->ib + 4, n);
    if (code == OFTP_SFNA) {
        if (oftp_dec_reas(s->level, OFTP_SFNA, s->ib + 4, n, &r) != 0)
            return end_local(s, ESID_DATA);
        slog(s, "%s refused, SFNA %02d retry %c", f->dsn, r.reas, r.retry);
        s->f.out_done(s->f.ctx, fh, r.reas, r.retry);
        return OK;
    }
    if (code != OFTP_SFPA)
        return unexpected(s, n, "SFPA");
    if (oftp_dec_sfpa(s->level, s->ib + 4, n, &acnt) != 0)
        return end_local(s, ESID_DATA);
    if (acnt != 0) {
        slog(s, "restart at %lu offered without restart", acnt);
        return end_local(s, ESID_PROTOCOL);
    }

    cap = (f->fmt == 'F' || f->fmt == 'V') ? f->lrecl : CHUNK;
    if (cap == 0)
        cap = CHUNK;
    if ((buf = s->xbuf = malloc(cap)) == NULL)
        return end_local(s, ESID_RESOURCE);

    credit = s->cred;
    efid.rcnt = efid.ucnt = 0;
    oftp_denc_init(&e, s->ob + 4, s->sdeb, s->cmpr);
    while (!last) {
        rc = s->f.read(s->f.ctx, fh, buf, cap, &n, &last);
        if (rc < 0) {
            drop_buf(s);
            slog(s, "%s: read error, session ends", f->dsn);
            s->f.out_done(s->f.ctx, fh, 12, 'Y');
            return end_local(s, ESID_OTHER);
        }
        if (f->fmt == 'F' || f->fmt == 'V') {
            if (rc == 0)
                break;
            last = 0;
            efid.rcnt++;
            rc = send_piece(s, &e, &credit, buf, n, 1);
        } else
            rc = send_piece(s, &e, &credit, buf, n, last);
        efid.ucnt += n;
        if (rc != OK) {
            drop_buf(s);
            return END;
        }
    }
    drop_buf(s);
    if (e.len > 1 && send_data(s, &e, &credit) != OK)
        return END;
    /* no EFID while waiting for credit */
    if (credit == 0 && get_simple(s, OFTP_CDT, "CDT") != OK)
        return END;

    rc = oftp_enc_efid(s->level, &efid, s->ob + 4, s->sdeb);
    if (rc < 0 || put_cmd(s, (unsigned)rc) != OK)
        return END;
    if (get_cmd(s, &n) != OK)
        return END;
    code = oftp_cmd_code(s->ib + 4, n);
    if (code == OFTP_EFNA) {
        if (oftp_dec_reas(s->level, OFTP_EFNA, s->ib + 4, n, &r) != 0)
            return end_local(s, ESID_DATA);
        slog(s, "%s not accepted, EFNA %02d", f->dsn, r.reas);
        s->f.out_done(s->f.ctx, fh, r.reas, 'N');
        return OK;
    }
    if (code != OFTP_EFPA)
        return unexpected(s, n, "EFPA");
    if (oftp_dec_efpa(s->ib + 4, n, &cd) != 0)
        return end_local(s, ESID_DATA);
    slog(s, "%s sent, %lu records, %lu octets", f->dsn, efid.rcnt, efid.ucnt);
    s->files_out++;
    s->f.out_done(s->f.ctx, fh, 0, 'N');
    *turn = cd == 'Y';
    return OK;
}

/* Everything we have for the partner: responses first, then files */
static int
speak(OFTP_SESS *s)
{
    OFTP_ERPCMD erp;
    OFTP_SFIDCMD f;
    OFTP_FH *fh;
    int sent = 0, turn = 0, rc;
    char code;

    while (s->f.next_erp(s->f.ctx, &s->peer, &code, &erp, &fh) == 1) {
        rc = oftp_enc_erp(s->level, code, &erp, s->ob + 4, s->sdeb);
        if (rc < 0) {
            slog(s, "%s: response '%c' not possible at this level",
                 erp.dsn, code);
            s->f.erp_sent(s->f.ctx, fh);
            continue;
        }
        if (put_cmd(s, (unsigned)rc) != OK
            || get_simple(s, OFTP_RTR, "RTR") != OK)
            return END;
        s->f.erp_sent(s->f.ctx, fh);
        s->erps_out++;
        sent = 1;
    }

    if (s->mode == 'R')
        return sent ? SPK_SENT : SPK_NONE;
    while (!turn) {
        memset(&f, 0, sizeof(f));
        if (s->f.next_out(s->f.ctx, &s->peer, &f, &fh) != 1)
            break;
        if (send_file(s, &f, fh, &turn) != OK)
            return END;
        sent = 1;
    }
    if (turn)
        return SPK_TURN;
    return sent ? SPK_SENT : SPK_NONE;
}

/* ---- receiving --------------------------------------------------- */

/* Refusal reason for an SFID we cannot take, 0 if we can */
static int
refuse(OFTP_SESS *s, const OFTP_SFIDCMD *f)
{
    if (s->mode == 'S')
        return s->level >= OFTP_LEV14 ? 14 : 99;
    if ((f->fmt == 'F' || f->fmt == 'V')
        && (f->lrecl < 1 || f->lrecl > 32760))
        return 5;
    if (f->ciph != 0)
        return 15;
    if (f->sec & 1)
        return 16;
    if (f->comp != 0)
        return 18;
    if (f->sec & 2)
        return 19;
    if (f->env != 0)
        return 16;
    return 0;
}

static int
put_reas(OFTP_SESS *s, char code, int reas, char retry)
{
    OFTP_REASCMD r;
    int n;

    memset(&r, 0, sizeof(r));
    r.reas = reas;
    r.retry = retry;
    n = oftp_enc_reas(s->level, code, &r, s->ob + 4, s->sdeb);
    if (n < 0)
        return end_local(s, ESID_OTHER);
    return put_cmd(s, (unsigned)n);
}

/* One file, from its SFID (in s->ib) to EFPA/EFNA */
static int
recv_file(OFTP_SESS *s, unsigned n)
{
    OFTP_SFIDCMD f;
    OFTP_EFIDCMD efid;
    OFTP_DDEC d;
    OFTP_DSUB sub;
    OFTP_FH *fh;
    unsigned char *rec = NULL;
    unsigned char rep[OFTP_SR_MAX];
    unsigned rlen = 0, credit;
    unsigned long rcnt = 0, ucnt = 0;
    int recfmt, fail = 0, rc, reas;
    char retry = 'N', code;

    rc = oftp_dec_sfid(s->level, s->ib + 4, n, &f);
    if (rc == OFTP_ERANGE)
        return put_reas(s, OFTP_SFNA, 6, 'N');
    if (rc != 0)
        return end_local(s, ESID_DATA);
    if ((reas = refuse(s, &f)) != 0) {
        slog(s, "%s refused, SFNA %02d", f.dsn, reas);
        return put_reas(s, OFTP_SFNA, reas, 'N');
    }
    recfmt = f.fmt == 'F' || f.fmt == 'V';
    if (recfmt && (rec = s->xbuf = malloc(f.lrecl)) == NULL)
        return put_reas(s, OFTP_SFNA, 99, 'Y');
    if ((reas = s->f.open_in(s->f.ctx, &s->peer, &f, &fh, &retry)) != 0) {
        drop_buf(s);
        slog(s, "%s refused, SFNA %02d retry %c", f.dsn, reas, retry);
        return put_reas(s, OFTP_SFNA, reas, retry);
    }
    rc = oftp_enc_sfpa(s->level, 0, s->ob + 4, s->sdeb);
    if (put_cmd(s, (unsigned)rc) != OK) {
        drop_buf(s);
        s->f.close_in(s->f.ctx, fh, 0);
        return END;
    }

    credit = s->cred;
    for (;;) {
        if (get_cmd(s, &n) != OK)
            goto lost;
        code = oftp_cmd_code(s->ib + 4, n);
        if (code == OFTP_EFID)
            break;
        if (code != OFTP_DATA) {
            unexpected(s, n, "DATA or EFID");
            goto lost;
        }
        if (credit == 0) {
            slog(s, "DATA beyond the credit");
            end_local(s, ESID_PROTOCOL);
            goto lost;
        }
        credit--;
        oftp_ddec_init(&d, s->ib + 4, n);
        while ((rc = oftp_ddec_next(&d, s->cmpr, &sub)) == 1) {
            const unsigned char *p = sub.data;

            if (p == NULL) {
                memset(rep, sub.rep, sub.n);
                p = rep;
            }
            ucnt += sub.n;
            if (recfmt) {
                if (rlen + sub.n > f.lrecl) {
                    if (!fail)
                        fail = 5;
                } else {
                    memcpy(rec + rlen, p, sub.n);
                    rlen += sub.n;
                }
                if (sub.eor) {
                    if (!fail && (rc = s->f.write(s->f.ctx, fh, rec, rlen, 1)))
                        fail = rc;
                    rcnt++;
                    rlen = 0;
                }
            } else if (sub.n > 0 && !fail
                       && (rc = s->f.write(s->f.ctx, fh, p, sub.n, 0)) != 0)
                fail = rc;
        }
        if (rc < 0) {
            slog(s, "malformed DATA buffer (%d)", rc);
            end_local(s, rc == OFTP_EFIELD ? ESID_PROTOCOL : ESID_DATA);
            goto lost;
        }
        if (credit == 0) {
            if (put_simple(s, OFTP_CDT) != OK)
                goto lost;
            credit = s->cred;
        }
    }
    drop_buf(s);
    rec = NULL;

    if (oftp_dec_efid(s->level, s->ib + 4, n, &efid) != 0) {
        end_local(s, ESID_DATA);
        goto lost;
    }
    if (!fail && recfmt && (rlen != 0 || efid.rcnt != rcnt))
        fail = 10;
    if (!fail && efid.ucnt != ucnt)
        fail = 11;
    rc = s->f.close_in(s->f.ctx, fh, fail == 0);
    if (!fail && rc != 0)
        fail = rc;
    if (fail) {
        slog(s, "%s not accepted, EFNA %02d", f.dsn, fail);
        return put_reas(s, OFTP_EFNA, fail, 'N');
    }
    slog(s, "%s received, %lu records, %lu octets", f.dsn, rcnt, ucnt);
    s->files_in++;
    rc = oftp_enc_efpa('N', s->ob + 4, s->sdeb);
    return put_cmd(s, (unsigned)rc);

lost:
    drop_buf(s);
    s->f.close_in(s->f.ctx, fh, 0);
    return END;
}

/* Answers the speaker until it hands over the turn or ends */
static int
listen(OFTP_SESS *s)
{
    OFTP_ERPCMD erp;
    unsigned n;
    char code;

    for (;;) {
        if (get_cmd(s, &n) != OK)
            return END;
        code = oftp_cmd_code(s->ib + 4, n);
        switch (code) {
        case OFTP_SFID:
            if (recv_file(s, n) != OK)
                return END;
            break;
        case OFTP_EERP:
        case OFTP_NERP:
            if (oftp_dec_erp(s->level, code, s->ib + 4, n, &erp) != 0)
                return end_local(s, ESID_DATA);
            s->f.erp_recv(s->f.ctx, &s->peer, code, &erp);
            s->erps_in++;
            if (put_simple(s, OFTP_RTR) != OK)
                return END;
            break;
        case OFTP_CD:
            if (oftp_dec_cmd1(OFTP_CD, s->ib + 4, n) != 0)
                return end_local(s, ESID_DATA);
            return LSN_CD;
        default:
            return unexpected(s, n, "SFID, EERP, NERP or CD");
        }
    }
}

/* ---- the session ------------------------------------------------- */

int
oftp_sess_run(OFTP_SESS *s)
{
    int speaker, unsolicited = 0, rc;

    s->level = 0;
    s->sdeb = 0;
    s->esid = 0;
    s->esid_local = 0;
    s->files_out = s->files_in = s->erps_out = s->erps_in = 0;

    rc = s->initiator ? start_initiator(s) : start_responder(s);
    if (rc != OK)
        return s->esid;
    s->peer.level = s->level;
    slog(s, "session with %s: level %d, buffer %u, credit %u, mode %c%s",
         s->peer.name, s->level, s->sdeb, s->cred, s->mode,
         s->cmpr ? ", compressed" : "");

    speaker = s->initiator;
    for (;;) {
        if (speaker) {
            rc = speak(s);
            if (rc == END)
                return s->esid;
            if (rc == SPK_NONE && unsolicited) {
                OFTP_REASCMD r;
                int n;

                memset(&r, 0, sizeof(r));
                n = oftp_enc_reas(s->level, OFTP_ESID, &r, s->ob + 4, s->sdeb);
                put_cmd(s, (unsigned)n);
                s->esid = ESID_NORMAL;
                s->esid_local = 1;
                slog(s, "session ended normally");
                return 0;
            }
            if (put_simple(s, OFTP_CD) != OK)
                return s->esid;
            speaker = 0;
        } else {
            rc = listen(s);
            if (rc == END)
                return s->esid;
            speaker = 1;
            unsolicited = 1;            /* we never ask for the turn */
        }
    }
}

void
oftp_sess_free(OFTP_SESS *s)
{
    drop_buf(s);
}
