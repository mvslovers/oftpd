/* oftp#cmd.c - OFTP command encoder and decoder, per protocol level
**
** Offsets follow internals/protocol.md section 2 (RFC 2204 for 1.3, the
** 1.4 draft, RFC 5024 for 2.0). Each function checks the room it is
** given before it writes and the length it is given before it reads.
*/
#include <string.h>

#include "oftpfld.h"
#include "oftpwire.h"

/* "ODETTE FTP READY " -- 17 characters, the last a blank */
static const char ssrm_msg[] = "ODETTE FTP READY ";

#define SSRM_LEN        19
#define SSID_LEN        61
#define SFID_LEN1       128     /* 1.3 and 1.4 */
#define SFID_LEN2       165     /* 2.0 without the description */
#define EERP_LEN        106
#define NERP_LEN        128

static int
is20(int level)
{
    return level >= OFTP_LEV20;
}

static int
is14(int level)
{
    return level >= OFTP_LEV14;
}

/* 1.3 dates and times <-> the 1.4 form held in the structs */
static void
date_from6(const unsigned char *b, char *date)
{
    char yy[7] = "";

    oftp_get_x(b, 6, yy);
    date[0] = (yy[0] >= '7' && yy[0] <= '9') ? '1' : '2';
    date[1] = (date[0] == '1') ? '9' : '0';
    memcpy(date + 2, yy, 6);
    date[OFTP_DATLEN] = '\0';
}

static void
time_from6(const unsigned char *b, char *time)
{
    oftp_get_x(b, 6, time);
    memcpy(time + 6, "0000", 5);
}

/* Date and time of a virtual file at its level's offsets: the 1.3 form
** at d13/t13, the 1.4 form at d14/t14.
*/
static void
put_dt(int level, unsigned char *b, const char *date, const char *time,
       unsigned d13, unsigned t13, unsigned d14, unsigned t14)
{
    if (is14(level)) {
        oftp_put_x(b + d14, date, OFTP_DATLEN);
        oftp_put_x(b + t14, time, OFTP_TIMLEN);
    } else {
        oftp_put_x(b + d13, strlen(date) > 2 ? date + 2 : date, 6);
        oftp_put_x(b + t13, time, 6);
    }
}

static void
get_dt(int level, const unsigned char *b, char *date, char *time,
       unsigned d13, unsigned t13, unsigned d14, unsigned t14)
{
    if (is14(level)) {
        oftp_get_x(b + d14, OFTP_DATLEN, date);
        oftp_get_x(b + t14, OFTP_TIMLEN, time);
    } else {
        date_from6(b + d13, date);
        time_from6(b + t13, time);
    }
}

/* A 2.0 text tail: 9(3) length + text. Returns the octets written. */
static int
put_text(unsigned char *b, unsigned cap, unsigned textl,
         const unsigned char *text)
{
    if (textl > 999)
        return OFTP_EFIELD;
    if (cap < 3 + textl)
        return OFTP_ESHORT;
    oftp_put_9(b, textl, 3);
    if (textl > 0)
        memcpy(b + 3, text, textl);
    return 3 + (int)textl;
}

/* Reads a 2.0 text tail at b; avail is what the buffer still holds */
static int
get_text(const unsigned char *b, unsigned avail, unsigned *textl,
         const unsigned char **text)
{
    unsigned long n;
    int rc;

    if (avail < 3)
        return OFTP_ELEN;
    if ((rc = oftp_get_9(b, 3, &n)) < 0)
        return rc;
    if (avail < 3 + n)
        return OFTP_ELEN;
    *textl = (unsigned)n;
    *text = n > 0 ? b + 3 : NULL;
    return 3 + (int)n;
}

static int
is_yn(char c)
{
    return c == 'Y' || c == 'N';
}

/* ------------------------------------------------------------------ */

char
oftp_cmd_code(const unsigned char *b, unsigned len)
{
    return len > 0 ? oftp_get_c(b) : '\0';
}

int
oftp_enc_cmd1(char code, unsigned char *b, unsigned cap)
{
    if (cap < 1)
        return OFTP_ESHORT;
    oftp_put_c(b, code);
    return 1;
}

int
oftp_dec_cmd1(char code, const unsigned char *b, unsigned len)
{
    if (len < 1 || oftp_get_c(b) != code)
        return OFTP_ECMD;
    return len == 1 ? 0 : OFTP_ELEN;
}

int
oftp_enc_cdt(unsigned char *b, unsigned cap)
{
    if (cap < 3)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_CDT);
    oftp_put_x(b + 1, "", 2);
    return 3;
}

int
oftp_dec_cdt(const unsigned char *b, unsigned len)
{
    if (len < 1 || oftp_get_c(b) != OFTP_CDT)
        return OFTP_ECMD;
    return len == 3 ? 0 : OFTP_ELEN;
}

/* ------------------------------------------------------------------ */

int
oftp_enc_ssrm(unsigned char *b, unsigned cap)
{
    if (cap < SSRM_LEN)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_SSRM);
    oftp_put_x(b + 1, ssrm_msg, 17);
    b[18] = OFTP_WIRE_CR;
    return SSRM_LEN;
}

int
oftp_dec_ssrm(const unsigned char *b, unsigned len)
{
    char msg[18];

    if (len < 1 || oftp_get_c(b) != OFTP_SSRM)
        return OFTP_ECMD;
    if (len != SSRM_LEN)
        return OFTP_ELEN;
    oftp_get_x(b + 1, 17, msg);         /* the trailing blank goes */
    if (strncmp(msg, ssrm_msg, 16) != 0 || !OFTP_IS_CR(b[18]))
        return OFTP_EFIELD;
    return 0;
}

/* ------------------------------------------------------------------ */

int
oftp_enc_ssid(const OFTP_SSIDCMD *c, unsigned char *b, unsigned cap)
{
    if (cap < SSID_LEN)
        return OFTP_ESHORT;
    if (c->level < 1 || c->level > 9)
        return OFTP_EFIELD;
    oftp_put_c(b, OFTP_SSID);
    oftp_put_9(b + 1, (unsigned long)c->level, 1);
    oftp_put_x(b + 2, c->code, OFTP_IDLEN);
    oftp_put_x(b + 27, c->pswd, OFTP_PSWLEN);
    if (oftp_put_9(b + 35, c->sdeb, 5) < 0)
        return OFTP_EFIELD;
    oftp_put_c(b + 40, c->sr);
    oftp_put_c(b + 41, c->cmpr);
    oftp_put_c(b + 42, c->rest);
    oftp_put_c(b + 43, c->spec);
    if (oftp_put_9(b + 44, c->cred, 3) < 0)
        return OFTP_EFIELD;
    oftp_put_c(b + 47, is20(c->level) ? c->auth : ' ');
    oftp_put_x(b + 48, "", 4);
    oftp_put_x(b + 52, c->user, OFTP_USRLEN);
    b[60] = OFTP_WIRE_CR;
    return SSID_LEN;
}

int
oftp_dec_ssid(const unsigned char *b, unsigned len, OFTP_SSIDCMD *c)
{
    unsigned long n;
    int rc;

    if (len < 1 || oftp_get_c(b) != OFTP_SSID)
        return OFTP_ECMD;
    if (len != SSID_LEN)
        return OFTP_ELEN;
    if ((rc = oftp_get_9(b + 1, 1, &n)) < 0)
        return rc;
    c->level = (int)n;
    oftp_get_x(b + 2, OFTP_IDLEN, c->code);
    oftp_get_x(b + 27, OFTP_PSWLEN, c->pswd);
    if ((rc = oftp_get_9(b + 35, 5, &n)) < 0)
        return rc;
    c->sdeb = (unsigned)n;
    c->sr = oftp_get_c(b + 40);
    c->cmpr = oftp_get_c(b + 41);
    c->rest = oftp_get_c(b + 42);
    c->spec = oftp_get_c(b + 43);
    if ((rc = oftp_get_9(b + 44, 3, &n)) < 0)
        return rc;
    c->cred = (unsigned)n;
    c->auth = oftp_get_c(b + 47);
    if (!is20(c->level) || c->auth == ' ')
        c->auth = 'N';
    oftp_get_x(b + 52, OFTP_USRLEN, c->user);
    if (!OFTP_IS_CR(b[60]))
        return OFTP_EFIELD;
    if ((c->sr != 'S' && c->sr != 'R' && c->sr != 'B') || !is_yn(c->cmpr)
        || !is_yn(c->rest) || !is_yn(c->spec) || !is_yn(c->auth))
        return OFTP_EFIELD;
    return 0;
}

/* ------------------------------------------------------------------ */

int
oftp_enc_sfid(int level, const OFTP_SFIDCMD *c, unsigned char *b,
              unsigned cap)
{
    unsigned need = is20(level) ? SFID_LEN2 + c->descl : SFID_LEN1;

    if (is20(level) && c->descl > 999)
        return OFTP_EFIELD;
    if (cap < need)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_SFID);
    oftp_put_x(b + 1, c->dsn, OFTP_DSNLEN);
    oftp_put_x(b + 27, "", is14(level) ? 3 : 9);
    put_dt(level, b, c->date, c->time, 36, 42, 30, 38);
    oftp_put_x(b + 48, c->user, OFTP_USRLEN);
    oftp_put_x(b + 56, c->dest, OFTP_IDLEN);
    oftp_put_x(b + 81, c->orig, OFTP_IDLEN);
    oftp_put_c(b + 106, c->fmt);
    if (oftp_put_9(b + 107, c->lrecl, 5) < 0)
        return OFTP_EFIELD;
    if (is20(level)) {
        if (oftp_put_9(b + 112, c->fsiz, 13) < 0
            || oftp_put_9(b + 125, c->osiz, 13) < 0
            || oftp_put_9(b + 138, c->rest, 17) < 0
            || oftp_put_9(b + 155, (unsigned long)c->sec, 2) < 0
            || oftp_put_9(b + 157, (unsigned long)c->ciph, 2) < 0
            || oftp_put_9(b + 159, (unsigned long)c->comp, 1) < 0
            || oftp_put_9(b + 160, (unsigned long)c->env, 1) < 0)
            return OFTP_EFIELD;
        oftp_put_c(b + 161, c->sign);
        oftp_put_9(b + 162, c->descl, 3);
        if (c->descl > 0)
            memcpy(b + SFID_LEN2, c->desc, c->descl);
    } else {
        if (oftp_put_9(b + 112, c->fsiz, 7) < 0
            || oftp_put_9(b + 119, c->rest, 9) < 0)
            return OFTP_EFIELD;
    }
    return (int)need;
}

int
oftp_dec_sfid(int level, const unsigned char *b, unsigned len,
              OFTP_SFIDCMD *c)
{
    unsigned long n;
    int rc;

    if (len < 1 || oftp_get_c(b) != OFTP_SFID)
        return OFTP_ECMD;
    if (is20(level) ? len < SFID_LEN2 : len != SFID_LEN1)
        return OFTP_ELEN;
    oftp_get_x(b + 1, OFTP_DSNLEN, c->dsn);
    get_dt(level, b, c->date, c->time, 36, 42, 30, 38);
    oftp_get_x(b + 48, OFTP_USRLEN, c->user);
    oftp_get_x(b + 56, OFTP_IDLEN, c->dest);
    oftp_get_x(b + 81, OFTP_IDLEN, c->orig);
    c->fmt = oftp_get_c(b + 106);
    if (c->fmt != 'F' && c->fmt != 'V' && c->fmt != 'U' && c->fmt != 'T')
        return OFTP_EFIELD;
    if ((rc = oftp_get_9(b + 107, 5, &n)) < 0)
        return rc;
    c->lrecl = (unsigned)n;
    c->sec = c->ciph = c->comp = c->env = 0;
    c->sign = 'N';
    c->descl = 0;
    c->desc = NULL;
    if (!is20(level)) {
        if ((rc = oftp_get_9(b + 112, 7, &c->fsiz)) < 0
            || (rc = oftp_get_9(b + 119, 9, &c->rest)) < 0)
            return rc;
        c->osiz = c->fsiz;
        return 0;
    }
    if ((rc = oftp_get_9(b + 112, 13, &c->fsiz)) < 0
        || (rc = oftp_get_9(b + 125, 13, &c->osiz)) < 0
        || (rc = oftp_get_9(b + 138, 17, &c->rest)) < 0)
        return rc;
    if ((rc = oftp_get_9(b + 155, 2, &n)) < 0)
        return rc;
    c->sec = (int)n;
    if ((rc = oftp_get_9(b + 157, 2, &n)) < 0)
        return rc;
    c->ciph = (int)n;
    if ((rc = oftp_get_9(b + 159, 1, &n)) < 0)
        return rc;
    c->comp = (int)n;
    if ((rc = oftp_get_9(b + 160, 1, &n)) < 0)
        return rc;
    c->env = (int)n;
    c->sign = oftp_get_c(b + 161);
    if (!is_yn(c->sign))
        return OFTP_EFIELD;
    if ((rc = get_text(b + 162, len - 162, &c->descl, &c->desc)) < 0)
        return rc;
    return 162 + (unsigned)rc == len ? 0 : OFTP_ELEN;
}

/* ------------------------------------------------------------------ */

int
oftp_enc_sfpa(int level, unsigned long acnt, unsigned char *b,
              unsigned cap)
{
    unsigned w = is20(level) ? 17 : 9;

    if (cap < 1 + w)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_SFPA);
    if (oftp_put_9(b + 1, acnt, w) < 0)
        return OFTP_EFIELD;
    return 1 + (int)w;
}

int
oftp_dec_sfpa(int level, const unsigned char *b, unsigned len,
              unsigned long *acnt)
{
    unsigned w = is20(level) ? 17 : 9;

    if (len < 1 || oftp_get_c(b) != OFTP_SFPA)
        return OFTP_ECMD;
    if (len != 1 + w)
        return OFTP_ELEN;
    return oftp_get_9(b + 1, w, acnt);
}

/* ------------------------------------------------------------------ */

/* SFNA: cmd reas retry [textl text]
** EFNA: cmd reas [textl text]
** ESID: cmd reas [textl text] CR
*/
int
oftp_enc_reas(int level, char code, const OFTP_REASCMD *c,
              unsigned char *b, unsigned cap)
{
    unsigned n = 3;
    int rc;

    if (code != OFTP_SFNA && code != OFTP_EFNA && code != OFTP_ESID)
        return OFTP_ECMD;
    if (cap < 3)
        return OFTP_ESHORT;
    oftp_put_c(b, code);
    if (oftp_put_9(b + 1, (unsigned long)c->reas, 2) < 0)
        return OFTP_EFIELD;
    if (code == OFTP_SFNA) {
        if (cap < 4)
            return OFTP_ESHORT;
        oftp_put_c(b + 3, c->retry);
        n = 4;
    }
    if (is20(level)) {
        if ((rc = put_text(b + n, cap - n, c->textl, c->text)) < 0)
            return rc;
        n += (unsigned)rc;
    }
    if (code == OFTP_ESID) {
        if (cap < n + 1)
            return OFTP_ESHORT;
        b[n++] = OFTP_WIRE_CR;
    }
    return (int)n;
}

int
oftp_dec_reas(int level, char code, const unsigned char *b, unsigned len,
              OFTP_REASCMD *c)
{
    unsigned long n;
    unsigned pos = 3;
    unsigned end = len;
    int rc;

    if (len < 1 || oftp_get_c(b) != code
        || (code != OFTP_SFNA && code != OFTP_EFNA && code != OFTP_ESID))
        return OFTP_ECMD;
    if (code == OFTP_ESID) {
        if (len < 4 || !OFTP_IS_CR(b[len - 1]))
            return OFTP_ELEN;
        end = len - 1;
    }
    if (end < 3)
        return OFTP_ELEN;
    if ((rc = oftp_get_9(b + 1, 2, &n)) < 0)
        return rc;
    c->reas = (int)n;
    c->retry = 'N';
    c->textl = 0;
    c->text = NULL;
    if (code == OFTP_SFNA) {
        if (end < 4)
            return OFTP_ELEN;
        c->retry = oftp_get_c(b + 3);
        if (!is_yn(c->retry))
            return OFTP_EFIELD;
        pos = 4;
    }
    if (is20(level)) {
        if ((rc = get_text(b + pos, end - pos, &c->textl, &c->text)) < 0)
            return rc;
        pos += (unsigned)rc;
    }
    return pos == end ? 0 : OFTP_ELEN;
}

/* ------------------------------------------------------------------ */

int
oftp_enc_efid(int level, const OFTP_EFIDCMD *c, unsigned char *b,
              unsigned cap)
{
    unsigned wr = is20(level) ? 17 : 9;
    unsigned wu = is20(level) ? 17 : 12;

    if (cap < 1 + wr + wu)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_EFID);
    if (oftp_put_9(b + 1, c->rcnt, wr) < 0
        || oftp_put_9(b + 1 + wr, c->ucnt, wu) < 0)
        return OFTP_EFIELD;
    return 1 + (int)(wr + wu);
}

int
oftp_dec_efid(int level, const unsigned char *b, unsigned len,
              OFTP_EFIDCMD *c)
{
    unsigned wr = is20(level) ? 17 : 9;
    unsigned wu = is20(level) ? 17 : 12;
    int rc;

    if (len < 1 || oftp_get_c(b) != OFTP_EFID)
        return OFTP_ECMD;
    if (len != 1 + wr + wu)
        return OFTP_ELEN;
    if ((rc = oftp_get_9(b + 1, wr, &c->rcnt)) < 0)
        return rc;
    return oftp_get_9(b + 1 + wr, wu, &c->ucnt);
}

int
oftp_enc_efpa(char cd, unsigned char *b, unsigned cap)
{
    if (cap < 2)
        return OFTP_ESHORT;
    oftp_put_c(b, OFTP_EFPA);
    oftp_put_c(b + 1, cd);
    return 2;
}

int
oftp_dec_efpa(const unsigned char *b, unsigned len, char *cd)
{
    if (len < 1 || oftp_get_c(b) != OFTP_EFPA)
        return OFTP_ECMD;
    if (len != 2)
        return OFTP_ELEN;
    *cd = oftp_get_c(b + 1);
    return is_yn(*cd) ? 0 : OFTP_EFIELD;
}

/* ------------------------------------------------------------------ */

/* The unsigned 2.0 tail of EERP and NERP: hash length 0, signature
** length 0.
*/
static int
put_nosig(unsigned char *b, unsigned cap)
{
    if (cap < 4)
        return OFTP_ESHORT;
    oftp_put_u2(b, 0);
    oftp_put_u2(b + 2, 0);
    return 4;
}

static int
get_sig(const unsigned char *b, unsigned avail, OFTP_ERPCMD *c)
{
    unsigned pos = 0;

    if (avail < 2)
        return OFTP_ELEN;
    c->hshl = oftp_get_u2(b);
    pos = 2;
    if (avail < pos + c->hshl + 2)
        return OFTP_ELEN;
    c->hsh = c->hshl > 0 ? b + pos : NULL;
    pos += c->hshl;
    c->sigl = oftp_get_u2(b + pos);
    pos += 2;
    if (avail < pos + c->sigl)
        return OFTP_ELEN;
    c->sig = c->sigl > 0 ? b + pos : NULL;
    return (int)(pos + c->sigl);
}

int
oftp_enc_erp(int level, char code, const OFTP_ERPCMD *c, unsigned char *b,
             unsigned cap)
{
    unsigned n;
    int rc;

    if (code == OFTP_EERP) {
        if (cap < EERP_LEN)
            return OFTP_ESHORT;
        oftp_put_c(b, OFTP_EERP);
        oftp_put_x(b + 1, c->dsn, OFTP_DSNLEN);
        oftp_put_x(b + 27, "", is14(level) ? 3 : 9);
        put_dt(level, b, c->date, c->time, 36, 42, 30, 38);
        oftp_put_x(b + 48, c->user, OFTP_USRLEN);
        oftp_put_x(b + 56, c->dest, OFTP_IDLEN);
        oftp_put_x(b + 81, c->orig, OFTP_IDLEN);
        n = EERP_LEN;
    } else if (code == OFTP_NERP) {
        if (!is14(level))
            return OFTP_ELEVEL;
        if (cap < NERP_LEN)
            return OFTP_ESHORT;
        oftp_put_c(b, OFTP_NERP);
        oftp_put_x(b + 1, c->dsn, OFTP_DSNLEN);
        oftp_put_x(b + 27, "", 6);
        oftp_put_x(b + 33, c->date, OFTP_DATLEN);
        oftp_put_x(b + 41, c->time, OFTP_TIMLEN);
        oftp_put_x(b + 51, c->dest, OFTP_IDLEN);
        oftp_put_x(b + 76, c->orig, OFTP_IDLEN);
        oftp_put_x(b + 101, c->crea, OFTP_IDLEN);
        if (oftp_put_9(b + 126, (unsigned long)c->reas, 2) < 0)
            return OFTP_EFIELD;
        n = NERP_LEN;
        if (is20(level)) {
            if ((rc = put_text(b + n, cap - n, c->textl, c->text)) < 0)
                return rc;
            n += (unsigned)rc;
        }
    } else
        return OFTP_ECMD;

    if (is20(level)) {
        if ((rc = put_nosig(b + n, cap - n)) < 0)
            return rc;
        n += (unsigned)rc;
    }
    return (int)n;
}

int
oftp_dec_erp(int level, char code, const unsigned char *b, unsigned len,
             OFTP_ERPCMD *c)
{
    unsigned long n;
    unsigned pos;
    int rc;

    if (len < 1 || oftp_get_c(b) != code)
        return OFTP_ECMD;
    c->user[0] = c->crea[0] = '\0';
    c->reas = 0;
    c->textl = c->hshl = c->sigl = 0;
    c->text = c->hsh = c->sig = NULL;

    if (code == OFTP_EERP) {
        if (is20(level) ? len < EERP_LEN : len != EERP_LEN)
            return OFTP_ELEN;
        oftp_get_x(b + 1, OFTP_DSNLEN, c->dsn);
        get_dt(level, b, c->date, c->time, 36, 42, 30, 38);
        oftp_get_x(b + 48, OFTP_USRLEN, c->user);
        oftp_get_x(b + 56, OFTP_IDLEN, c->dest);
        oftp_get_x(b + 81, OFTP_IDLEN, c->orig);
        pos = EERP_LEN;
    } else if (code == OFTP_NERP) {
        if (!is14(level))
            return OFTP_ELEVEL;
        if (is20(level) ? len < NERP_LEN : len != NERP_LEN)
            return OFTP_ELEN;
        oftp_get_x(b + 1, OFTP_DSNLEN, c->dsn);
        oftp_get_x(b + 33, OFTP_DATLEN, c->date);
        oftp_get_x(b + 41, OFTP_TIMLEN, c->time);
        oftp_get_x(b + 51, OFTP_IDLEN, c->dest);
        oftp_get_x(b + 76, OFTP_IDLEN, c->orig);
        oftp_get_x(b + 101, OFTP_IDLEN, c->crea);
        if ((rc = oftp_get_9(b + 126, 2, &n)) < 0)
            return rc;
        c->reas = (int)n;
        pos = NERP_LEN;
        if (is20(level)) {
            rc = get_text(b + pos, len - pos, &c->textl, &c->text);
            if (rc < 0)
                return rc;
            pos += (unsigned)rc;
        }
    } else
        return OFTP_ECMD;

    if (is20(level)) {
        if ((rc = get_sig(b + pos, len - pos, c)) < 0)
            return rc;
        pos += (unsigned)rc;
    }
    return pos == len ? 0 : OFTP_ELEN;
}
