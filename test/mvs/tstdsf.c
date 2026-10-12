/* TSTDSF - data sets as virtual files: write, read back, compare (MVS)
**
** Each format goes through oftp_dsf_open_in/write/close into a new data
** set and comes back through oftp_dsf_open_out/read, as the session
** engine would drive them. PARM: the data set prefix, default
** IBMUSER.OFTP370.TDSF.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpdsf.h"
#include "oftpxlt.h"

static char prefix[40] = "IBMUSER.OFTP370.TDSF";
static unsigned char got[16384];
static unsigned char want[16384];

static OFTP_DSOPT opt = { 255, 4096, "", "" };

static void
dsname(char *buf, const char *last)
{
    sprintf(buf, "%s.%s", prefix, last);
    remove(buf);
}

/* Local text as the ASCII octets a partner would send */
static unsigned
ascii(unsigned char *out, const char *s)
{
    unsigned n = 0;

    /* '\n' is NEL (X'15') to cc370, which CP037 maps to X'85': spell
    ** the line end as the wire has it */
    for (; *s; s++)
        out[n++] = *s == '\n' ? 0x0A : *s == '\r' ? 0x0D : OFTP_L2W(*s);
    return n;
}

/* Reads everything back: records joined by '|' for F/V, the stream for
** U/T. Returns the length, *recs the read calls that returned data.
*/
static unsigned
readback(const char *dsn, char fmt, char *gotfmt, unsigned *lrecl,
         unsigned *calls, int *lastok)
{
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    unsigned char b[4096];
    unsigned n, len = 0;
    int last = 0, rc;

    memset(&sf, 0, sizeof(sf));
    *calls = 0;
    *lastok = 0;
    if (oftp_dsf_open_out(dsn, fmt, OFTP_XLATE_NONE, &sf, &f) != 0)
        return 0;
    *gotfmt = sf.fmt;
    *lrecl = sf.lrecl;
    while (!last && (rc = oftp_dsf_read(f, b, sf.fmt == 'F' || sf.fmt == 'V'
                                              ? sf.lrecl : sizeof(b),
                                       &n, &last)) == 1) {
        memcpy(got + len, b, n);
        len += n;
        (*calls)++;
        if (sf.fmt == 'F' || sf.fmt == 'V') {
            got[len++] = '|';
            last = 0;
        }
    }
    *lastok = sf.fmt == 'F' || sf.fmt == 'V' ? 1 : last;
    oftp_dsf_close(f, 1);
    return len;
}

static void
t_fixed(void)
{
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    char dsn[60], fmt;
    unsigned char rec[20];
    unsigned len, lrecl, calls;
    int i, lastok;

    dsname(dsn, "F20");
    memset(&sf, 0, sizeof(sf));
    sf.fmt = 'F';
    sf.lrecl = 20;
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_NONE, &opt, &f), 0, "F: allocate FB 20");
    for (i = 0; i < 3; i++) {
        memset(rec, 0xC1 + i, sizeof(rec));
        rec[0] = 0x00;                  /* binary stays binary */
        memcpy(want + i * 21, rec, 20);
        want[i * 21 + 20] = '|';
        CHECK_EQ(oftp_dsf_write(f, rec, 20, 1), 0, "F: write a record");
    }
    CHECK_EQ(oftp_dsf_close(f, 1), 0, "F: close");
    len = readback(dsn, 0, &fmt, &lrecl, &calls, &lastok);
    CHECK(fmt == 'F' && lrecl == 20, "F: read back as F, LRECL 20");
    CHECK(len == 63 && memcmp(got, want, 63) == 0, "F: records intact");
    remove(dsn);
}

static void
t_variable(void)
{
    static const char *recs[3] = { "short", "", "a record of the variable file "
                                   "that is longer than one subrecord of 63" };
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    char dsn[60], fmt;
    unsigned char big[120];
    unsigned len, lrecl, calls, wl = 0;
    int i, lastok;

    dsname(dsn, "V100");
    memset(&sf, 0, sizeof(sf));
    sf.fmt = 'V';
    sf.lrecl = 100;
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_NONE, &opt, &f), 0, "V: allocate VB 104");
    for (i = 0; i < 3; i++) {
        unsigned n = (unsigned)strlen(recs[i]);

        CHECK_EQ(oftp_dsf_write(f, (const unsigned char *)recs[i], n, 1), 0, "V: write a record");
        memcpy(want + wl, recs[i], n);
        wl += n;
        want[wl++] = '|';
    }
    memset(big, 'X', sizeof(big));
    CHECK_EQ(oftp_dsf_write(f, big, 101, 1), 5, "V: record beyond LRECL is reason 05");
    CHECK_EQ(oftp_dsf_close(f, 1), 0, "V: close");
    len = readback(dsn, 0, &fmt, &lrecl, &calls, &lastok);
    CHECK(fmt == 'V' && lrecl == 100, "V: read back as V, LRECL 100");
    CHECK(calls == 3, "V: three records, the empty one included");
    CHECK(len == wl && memcmp(got, want, wl) == 0, "V: records intact");
    printf("    (V: %u records read, %u octets)\n", calls, len);
    remove(dsn);
}

static void
t_undefined(void)
{
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    char dsn[60], fmt;
    unsigned len, lrecl, calls, i;
    int lastok;

    for (i = 0; i < 10000; i++)
        want[i] = (unsigned char)(i % 251);
    dsname(dsn, "U");
    memset(&sf, 0, sizeof(sf));
    sf.fmt = 'U';
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_NONE, &opt, &f), 0, "U: allocate U 4096");
    /* in pieces that do not line up with the blocks */
    for (i = 0; i < 10000; i += 777)
        oftp_dsf_write(f, want + i, i + 777 > 10000 ? 10000 - i : 777, 0);
    CHECK_EQ(oftp_dsf_close(f, 1), 0, "U: close writes the short last block");
    len = readback(dsn, 0, &fmt, &lrecl, &calls, &lastok);
    CHECK(fmt == 'U' && lrecl == 0, "U: read back as U");
    CHECK(len == 10000 && memcmp(got, want, 10000) == 0, "U: every octet back");
    CHECK(lastok, "U: last piece flagged");
    printf("    (U: %u octets in %u reads)\n", len, calls);
    remove(dsn);
}

static void
t_text(void)
{
    OFTP_SFIDCMD sf;
    OFTP_DSF *f;
    OFTP_DSOPT small = { 20, 4096, "", "" };
    unsigned char in[200];
    char dsn[60], fmt;
    unsigned n, wl, len, lrecl, calls;
    int lastok, rc;

    dsname(dsn, "T");
    memset(&sf, 0, sizeof(sf));
    sf.fmt = 'T';
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_TEXT, &opt, &f), 0, "T: allocate VB 255");
    n = ascii(in, "line one\r\nline two\r\n\r\nlast line, no CR-LF");
    /* split between CR and LF, and inside a line */
    oftp_dsf_write(f, in, 9, 0);
    oftp_dsf_write(f, in + 9, 5, 0);
    oftp_dsf_write(f, in + 14, n - 14, 1);
    CHECK_EQ(oftp_dsf_close(f, 1), 0, "T: close writes the last line");

    len = readback(dsn, 'T', &fmt, &lrecl, &calls, &lastok);
    wl = ascii(want, "line one\r\nline two\r\n\r\nlast line, no CR-LF\r\n");
    CHECK(fmt == 'T' && lrecl == 0, "T: read back as T");
    CHECK(len == wl && memcmp(got, want, wl) == 0, "T: lines back with CR-LF");
    if (len != wl || memcmp(got, want, wl) != 0) {
        unsigned i;

        printf("    (T: got %u octets, want %u)\n    got ", len, wl);
        for (i = 0; i < len && i < 60; i++)
            printf("%02X", got[i]);
        printf("\n    want");
        for (i = 0; i < wl && i < 60; i++)
            printf("%02X", want[i]);
        printf("\n");
    }
    CHECK(lastok, "T: last piece flagged");
    remove(dsn);

    dsname(dsn, "T20");
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_TEXT, &small, &f), 0, "T: allocate VB 20");
    n = ascii(in, "sixteen chars ok\r\nthis line is far too long\r\n");
    rc = oftp_dsf_write(f, in, n, 1);
    CHECK_EQ(rc, 5, "T: a line beyond LRECL is reason 05");
    rc = oftp_dsf_close(f, 1);
    CHECK_EQ(rc, 0, "T: the lines before it are kept");
    sprintf((char *)in, "'%s'", dsn);
    {
        FILE *fp = fopen((char *)in, "r");
        char line[100];

        n = 0;
        while (fp != NULL && fgets(line, sizeof(line), fp) != NULL)
            n++;
        CHECK_EQ(n, 1, "T: one line written before the long one");
        if (fp != NULL)
            fclose(fp);
    }
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_TEXT, &small, &f), 13,
             "T: an existing data set is reason 13");
    remove(dsn);
    CHECK_EQ(oftp_dsf_open_in(dsn, &sf, OFTP_XLATE_TEXT, &small, &f), 0, "T: allocate again");
    oftp_dsf_close(f, 0);
    {
        char q[64];
        FILE *fp;

        sprintf(q, "'%s'", dsn);
        fp = fopen(q, "r");
        CHECK(fp == NULL, "T: discarded file is deleted");
        if (fp != NULL)
            fclose(fp);
    }
}

int
main(int argc, char **argv)
{
    printf("=== TSTDSF data sets as virtual files ===\n");
    if (argc > 1 && strlen(argv[1]) < sizeof(prefix))
        strcpy(prefix, argv[1]);
    t_fixed();
    t_variable();
    t_undefined();
    t_text();
    return mbt_test_summary("TSTDSF");
}
