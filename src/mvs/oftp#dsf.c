/* oftp#dsf.c - data sets as OFTP virtual files (MVS only)
**
** Record I/O through libc370's ropen()/rread()/rwrite(): one logical
** record per call, nothing added or translated, a V record with its RDW.
** New data sets are allocated with __dsalcf() and opened by DD name.
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mvs/dscb.h>
#include <mvs/dynalloc.h>
#include <mvs/rfile.h>

#include "oftpdsf.h"
#include "oftpfld.h"
#include "oftpxlt.h"

#define WIRE_LF     0x0A        /* line end of a T file, after CR */

#define R_FORMAT    4           /* SFNA/EFNA reason codes used here */
#define R_LRECL     5
#define R_ACCESS    12
#define R_DUP       13

#define TRKLEN_3350 19254       /* track length when the volume's own is
                                ** not to be had (DS4DEVTK of a 3350) */

struct oftp_dsf {
    RFILE      *rf;
    char        dsn[56];
    char        ddname[9];      /* receiving: the DD __dsalcf() made */
    int         in;             /* 1: receiving                      */
    char        fmt;            /* virtual file format               */
    int         xlate;
    int         recfm;          /* RFILE_RECFM_*                     */
    unsigned    lrecl;          /* the data set's LRECL (U: BLKSIZE) */
    unsigned char *rec;         /* one record, RDW room included     */
    unsigned char *line;        /* sending T: the line built         */
    const unsigned char *pend;  /* sending U/T: what is left to give */
    unsigned    plen, poff;
    int         eof;
    unsigned    blen;           /* receiving U: block filled; T: line */
    int         toolong;        /* receiving T: current line too long */
    unsigned long recs;
};

static void
xlate(unsigned char *p, unsigned n, const unsigned char *tab)
{
    while (n-- > 0) {
        *p = tab[*p];
        p++;
    }
}

static void
dsf_free(OFTP_DSF *f)
{
    free(f->rec);
    free(f->line);
    free(f);
}

/* ---- sending ----------------------------------------------------- */

/* The SFID's size estimate in 1K blocks (#21): the tracks the data set
** uses, from DS1LSTAR, at the volume's full track length. That is more
** than the data, never less, which is the right side to err on for a
** receiver that allocates by it. 0 means "not known": a member (the VTOC
** knows only the library), a data set not catalogued, or a DSCB that
** cannot be read -- the receiver then takes its own default.
*/
static unsigned long
size_kb(const char *dsn)
{
    LOCWORK  lw;
    DSCB     d;
    unsigned trks, trklen = TRKLEN_3350;

    if (strchr(dsn, '(') != NULL || strlen(dsn) > 44)
        return 0;
    memset(&lw, 0, sizeof(lw));
    if (__locate(dsn, &lw) != 0)
        return 0;
    memset(&d, 0, sizeof(d));
    if (__dscbdv(dsn, lw.volser, &d) != 0)
        return 0;
    trks = (((unsigned)d.dscb1.lstar[0] << 8) | d.dscb1.lstar[1]) + 1;

    /* DS4DEVTK by offset, not through d.dscb4: __dscbv() returns the
    ** format-4 DSCB from its format byte on (F4 at work[0], measured),
    ** while libc370's struct dscb4 starts with the 44-byte key, so
    ** dscb4.devtk lands 44 bytes too far (it read 256 for 19254;
    ** libc370#173). 0x16
    ** is X'42' less the key.
    */
    memset(&d, 0, sizeof(d));
    if (__dscbv(lw.volser, &d) == 0 && (unsigned char)d.work[0] == 0xF4) {
        unsigned t = ((unsigned)(unsigned char)d.work[0x16] << 8)
                   | (unsigned char)d.work[0x17];

        if (t != 0)
            trklen = t;
    }
    /* 65536 tracks of a 3390 are 3.7e9 bytes: still inside 32 bits */
    return ((unsigned long)trks * trklen + 1023) / 1024;
}

int
oftp_dsf_open_out(const char *dsn, char fmt, int xl, OFTP_SFIDCMD *sfid,
                  OFTP_DSF **out)
{
    OFTP_DSF *f;
    char name[60];

    if ((f = calloc(1, sizeof(*f))) == NULL)
        return R_ACCESS;
    snprintf(name, sizeof(name), "'%s'", dsn);
    snprintf(f->dsn, sizeof(f->dsn), "%s", dsn);
    if (ropen(name, 0, &f->rf) != 0) {
        free(f);
        return R_ACCESS;
    }
    f->recfm = f->rf->recfm;
    f->lrecl = (unsigned)f->rf->lrecl;
    f->xlate = xl;
    if (f->lrecl == 0 || (f->rec = malloc(f->lrecl + 4)) == NULL) {
        rclose(f->rf);
        dsf_free(f);
        return R_ACCESS;
    }

    if (fmt == 0)
        fmt = f->recfm == RFILE_RECFM_F ? 'F'
            : f->recfm == RFILE_RECFM_V ? 'V' : 'U';
    f->fmt = fmt;
    if (fmt == 'T' && (f->line = malloc(f->lrecl + 2)) == NULL) {
        rclose(f->rf);
        dsf_free(f);
        return R_ACCESS;
    }

    sfid->fmt = fmt;
    sfid->lrecl = fmt == 'F' ? f->lrecl
                : fmt == 'V' ? f->lrecl - 4 : 0;
    sfid->fsiz = size_kb(dsn);
    *out = f;
    return 0;
}

/* Next record into f->rec: 1 read, 0 end of data, -1 error. *data and
** *len describe the user data (without the RDW of a V record).
*/
static int
next_rec(OFTP_DSF *f, unsigned char **data, unsigned *len)
{
    size_t got = 0;

    errno = 0;
    if (rread(f->rf, f->rec, &got) != 0)
        return errno != 0 ? -1 : 0;
    f->recs++;
    if (f->recfm == RFILE_RECFM_V) {
        *data = f->rec + 4;
        *len = got >= 4 ? (unsigned)got - 4 : 0;
    } else {
        *data = f->rec;
        *len = (unsigned)got;
    }
    return 1;
}

/* Refills what the stream (U, T) gives next: 1, 0 at the end, -1 */
static int
refill(OFTP_DSF *f)
{
    unsigned char *d;
    unsigned n;
    int rc;

    if ((rc = next_rec(f, &d, &n)) <= 0)
        return rc;
    if (f->fmt == 'T') {
        if (f->recfm == RFILE_RECFM_F)
            while (n > 0 && d[n - 1] == ' ')
                n--;
        memcpy(f->line, d, n);
        xlate(f->line, n, oftp_e2a);
        f->line[n] = OFTP_WIRE_CR;
        f->line[n + 1] = WIRE_LF;
        f->pend = f->line;
        f->plen = n + 2;
    } else {
        if (f->xlate)
            xlate(d, n, oftp_e2a);
        f->pend = d;
        f->plen = n;
    }
    f->poff = 0;
    return 1;
}

int
oftp_dsf_read(OFTP_DSF *f, unsigned char *b, unsigned cap, unsigned *n,
              int *last)
{
    unsigned char *d;
    unsigned len;
    int rc;

    *n = 0;
    *last = 0;
    if (f->fmt == 'F' || f->fmt == 'V') {
        if ((rc = next_rec(f, &d, &len)) <= 0)
            return rc;
        if (len > cap)
            return -1;
        memcpy(b, d, len);
        if (f->xlate)
            xlate(b, len, oftp_e2a);
        *n = len;
        return 1;
    }

    /* U and T: fill b, then look one record ahead for the end */
    while (*n < cap) {
        unsigned k;

        if (f->poff >= f->plen) {
            if (f->eof)
                break;
            if ((rc = refill(f)) < 0)
                return -1;
            if (rc == 0) {
                f->eof = 1;
                break;
            }
            continue;
        }
        k = f->plen - f->poff;
        if (k > cap - *n)
            k = cap - *n;
        memcpy(b + *n, f->pend + f->poff, k);
        f->poff += k;
        *n += k;
    }
    if (f->poff >= f->plen && !f->eof) {
        if ((rc = refill(f)) < 0)
            return -1;
        if (rc == 0)
            f->eof = 1;
    }
    *last = f->eof && f->poff >= f->plen;
    return 1;
}

/* ---- receiving --------------------------------------------------- */

int
oftp_dsf_open_in(const char *dsn, const OFTP_SFIDCMD *sfid, int xl,
                 const OFTP_DSOPT *opt, OFTP_DSF **out)
{
    OFTP_DSF *f;
    const char *recfm;
    unsigned lrecl, blksize, trk, sec;
    char opts[200], dd[16];
    int n;

    switch (sfid->fmt) {
    case 'F':
        if (sfid->lrecl < 1 || sfid->lrecl > 32760)
            return R_LRECL;
        recfm = "FB";
        lrecl = sfid->lrecl;
        blksize = lrecl < 6144 ? lrecl * (6144 / lrecl) : lrecl;
        break;
    case 'V':
        if (sfid->lrecl < 1 || sfid->lrecl > 32752)
            return R_LRECL;
        recfm = "VB";
        lrecl = sfid->lrecl + 4;
        blksize = lrecl + 4 > 6144 ? lrecl + 4 : 6144;
        break;
    case 'U':
        recfm = "U";
        blksize = opt->ublksize;
        lrecl = 0;
        break;
    case 'T':
        recfm = "VB";
        lrecl = opt->tlrecl;
        blksize = lrecl + 4 > 6144 ? lrecl + 4 : 6144;
        break;
    default:
        return R_FORMAT;
    }

    /* about 15 KB a track; the SFID's size is an estimate in 1K blocks.
    ** No size at all (0) is a sender that did not say, which is what
    ** OFTPD itself sent before #21: TRK(15,15), up to 240 tracks, about
    ** 4 MB on a 3350, instead of the 50 tracks the estimate rule gives.
    */
    if (sfid->fsiz == 0 && opt->space > 0) {
        trk = opt->space;               /* what a receive rule says */
        sec = trk / 2 + 1;
    } else if (sfid->fsiz == 0) {
        trk = 15;
        sec = 15;
    } else {
        trk = (unsigned)(sfid->fsiz / 15) + 1;
        if (trk < 5)
            trk = 5;
        sec = trk / 2 + 1;
    }
    n = snprintf(opts, sizeof(opts),
                 "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=%s;"
                 "LRECL=%u;BLKSIZE=%u;SPACE=TRK(%u,%u)",
                 dsn, recfm, lrecl, blksize, trk, sec);
    if (opt->unit[0] && n > 0 && n < (int)sizeof(opts))
        n += snprintf(opts + n, sizeof(opts) - (size_t)n, ";UNIT=%s", opt->unit);
    if (opt->volser[0] && n > 0 && n < (int)sizeof(opts))
        snprintf(opts + n, sizeof(opts) - (size_t)n, ";VOLSER=%s", opt->volser);

    /* DISP=NEW on a name already in the catalog is not refused by the
    ** allocation: __dsalcf() returned 0 for it on MVS (dev1 JOB02098).
    ** What it then allocated was not determined. So look first.
    */
    {
        RFILE *rf;
        char q[60];

        snprintf(q, sizeof(q), "'%s'", dsn);
        if (ropen(q, 0, &rf) == 0) {
            rclose(rf);
            return R_DUP;
        }
    }

    if ((f = calloc(1, sizeof(*f))) == NULL)
        return R_ACCESS;
    f->in = 1;
    f->fmt = sfid->fmt;
    f->xlate = xl;
    snprintf(f->dsn, sizeof(f->dsn), "%s", dsn);
    memset(dd, 0, sizeof(dd));
    if (__dsalcf(dd, "%s", opts) != 0) {
        free(f);
        return R_ACCESS;
    }
    snprintf(f->ddname, sizeof(f->ddname), "%s", dd);
    snprintf(opts, sizeof(opts), "dd:%s", f->ddname);
    if (ropen(opts, 1, &f->rf) != 0) {
        __dsfree(f->ddname);
        remove(f->dsn);
        free(f);
        return R_ACCESS;
    }
    f->recfm = f->rf->recfm;
    f->lrecl = (unsigned)f->rf->lrecl;
    if (f->lrecl == 0)
        f->lrecl = blksize;
    if ((f->rec = malloc(f->lrecl + 4)) == NULL) {
        oftp_dsf_close(f, 0);
        return R_ACCESS;
    }
    *out = f;
    return 0;
}

static int
put_rec(OFTP_DSF *f, const unsigned char *p, unsigned n)
{
    f->recs++;
    return rwrite(f->rf, p, n) == 0 ? 0 : R_ACCESS;
}

/* A T line of len octets in f->rec + 4, CR already removed */
static int
put_line(OFTP_DSF *f, unsigned len)
{
    if (f->toolong) {
        f->toolong = 0;
        return R_LRECL;
    }
    xlate(f->rec + 4, len, oftp_a2e);
    f->rec[0] = (unsigned char)(((len + 4) >> 8) & 0xFF);
    f->rec[1] = (unsigned char)((len + 4) & 0xFF);
    f->rec[2] = f->rec[3] = 0;
    return put_rec(f, f->rec, len + 4);
}

int
oftp_dsf_write(OFTP_DSF *f, const unsigned char *b, unsigned n, int eor)
{
    unsigned i;
    int rc;

    switch (f->fmt) {
    case 'F':
        if (n > f->lrecl)
            return R_LRECL;
        memcpy(f->rec, b, n);
        if (n < f->lrecl)
            memset(f->rec + n, ' ', f->lrecl - n);
        if (f->xlate)
            xlate(f->rec, n, oftp_a2e);
        return put_rec(f, f->rec, f->lrecl);
    case 'V':
        if (n + 4 > f->lrecl)
            return R_LRECL;
        f->rec[0] = (unsigned char)(((n + 4) >> 8) & 0xFF);
        f->rec[1] = (unsigned char)((n + 4) & 0xFF);
        f->rec[2] = f->rec[3] = 0;
        memcpy(f->rec + 4, b, n);
        if (f->xlate)
            xlate(f->rec + 4, n, oftp_a2e);
        return put_rec(f, f->rec, n + 4);
    case 'U':
        (void)eor;
        while (n > 0) {
            unsigned k = f->lrecl - f->blen;

            if (k > n)
                k = n;
            memcpy(f->rec + f->blen, b, k);
            if (f->xlate)
                xlate(f->rec + f->blen, k, oftp_a2e);
            f->blen += k;
            b += k;
            n -= k;
            if (f->blen == f->lrecl) {
                if ((rc = put_rec(f, f->rec, f->blen)) != 0)
                    return rc;
                f->blen = 0;
            }
        }
        return 0;
    default:                            /* T */
        for (i = 0; i < n; i++) {
            if (b[i] == WIRE_LF) {
                unsigned len = f->blen;

                if (len > 0 && f->rec[4 + len - 1] == OFTP_WIRE_CR)
                    len--;
                f->blen = 0;
                if ((rc = put_line(f, len)) != 0)
                    return rc;
            } else if (f->blen + 4 < f->lrecl)
                f->rec[4 + f->blen++] = b[i];
            else if (f->blen + 4 == f->lrecl && b[i] == OFTP_WIRE_CR)
                f->rec[4 + f->blen++] = b[i];   /* the CR before LF still fits */
            else
                f->toolong = 1;
        }
        return 0;
    }
}

int
oftp_dsf_close(OFTP_DSF *f, int keep)
{
    int rc = 0;

    if (f->in && keep) {
        /* what is still buffered: a short last block, a last line */
        if (f->fmt == 'U' && f->blen > 0)
            rc = put_rec(f, f->rec, f->blen);
        else if (f->fmt == 'T' && (f->blen > 0 || f->toolong))
            rc = put_line(f, f->blen);
    }
    if (rclose(f->rf) != 0 && rc == 0)
        rc = R_ACCESS;
    if (f->in) {
        __dsfree(f->ddname);
        if (!keep || rc != 0)
            remove(f->dsn);
    }
    dsf_free(f);
    return keep ? rc : 0;
}

unsigned long
oftp_dsf_records(const OFTP_DSF *f)
{
    return f->recs;
}
