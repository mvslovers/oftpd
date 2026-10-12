/* TSTCMD - command codec against independently built vectors */
#include <string.h>

#include <mbtcheck.h>

#include "oftpwire.h"
#include "vectors.h"

static unsigned char out[256];

/* Encodes to out and compares with the vector */
#define CHECK_WIRE(rc, vec, msg)                                        \
    do {                                                                \
        int r_ = (rc);                                                  \
        CHECK(r_ == (int)sizeof(vec)                                    \
              && memcmp(out, vec, sizeof(vec)) == 0, msg);              \
    } while (0)

static void
t_ssrm(void)
{
    CHECK_WIRE(oftp_enc_ssrm(out, sizeof(out)), v_ssrm, "SSRM encodes");
    CHECK_EQ(oftp_dec_ssrm(v_ssrm, sizeof(v_ssrm)), 0, "SSRM decodes");
    CHECK_EQ(oftp_enc_ssrm(out, 18), OFTP_ESHORT, "SSRM: short buffer");
}

static void
t_ssid(void)
{
    OFTP_SSIDCMD s;
    unsigned char bad[61];

    memset(&s, 0, sizeof(s));
    s.level = OFTP_LEV20;
    strcpy(s.code, "O0013000000MVS1");
    strcpy(s.pswd, "SECRET");
    s.sdeb = 4096;
    s.sr = 'B';
    s.cmpr = 'N';
    s.rest = 'Y';
    s.spec = 'N';
    s.cred = 99;
    s.auth = 'N';
    strcpy(s.user, "USER1");
    CHECK_WIRE(oftp_enc_ssid(&s, out, sizeof(out)), v_ssid5, "SSID 2.0 encodes");

    memset(&s, 0, sizeof(s));
    CHECK_EQ(oftp_dec_ssid(v_ssid5, sizeof(v_ssid5), &s), 0, "SSID 2.0 decodes");
    CHECK_EQ(s.level, 5, "SSID level 5");
    CHECK(strcmp(s.code, "O0013000000MVS1") == 0, "SSID code, blanks trimmed");
    CHECK(strcmp(s.pswd, "SECRET") == 0, "SSID password");
    CHECK_EQ(s.sdeb, 4096, "SSID buffer size");
    CHECK(s.sr == 'B' && s.rest == 'Y' && s.auth == 'N', "SSID flags");
    CHECK_EQ(s.cred, 99, "SSID credit");
    CHECK(strcmp(s.user, "USER1") == 0, "SSID user");

    /* 1.3: a blank where 2.0 has SSIDAUTH */
    CHECK_EQ(oftp_dec_ssid(v_ssid2, sizeof(v_ssid2), &s), 0, "SSID 1.3 decodes");
    CHECK(s.level == 2 && s.auth == 'N', "SSID 1.3: blank auth reads N");
    CHECK(s.user[0] == '\0', "SSID 1.3: empty user");
    CHECK_WIRE(oftp_enc_ssid(&s, out, sizeof(out)), v_ssid2, "SSID 1.3 round trip");

    memcpy(bad, v_ssid5, sizeof(bad));
    bad[37] = bad[40];                  /* a letter in SSIDSDEB */
    CHECK_EQ(oftp_dec_ssid(bad, sizeof(bad), &s), OFTP_EFIELD, "SSID: letter in a number");
    CHECK_EQ(oftp_dec_ssid(v_ssid5, 60, &s), OFTP_ELEN, "SSID: short");
    CHECK_EQ(oftp_dec_ssid(v_sfid14, sizeof(v_sfid14), &s), OFTP_ECMD, "SSID: wrong command");
    memcpy(bad, v_ssid5, sizeof(bad));
    bad[60] = bad[59];                  /* no carriage return */
    CHECK_EQ(oftp_dec_ssid(bad, sizeof(bad), &s), OFTP_EFIELD, "SSID: missing CR");
}

static void
fill_sfid(OFTP_SFIDCMD *f, char fmt, unsigned lrecl)
{
    memset(f, 0, sizeof(*f));
    strcpy(f->dsn, "PAYROLL.DATA");
    strcpy(f->date, "20261012");
    strcpy(f->time, "1745010001");
    strcpy(f->user, "U1");
    strcpy(f->dest, "O0013000000MVS2");
    strcpy(f->orig, "O0013000000MVS1");
    f->fmt = fmt;
    f->lrecl = lrecl;
    f->fsiz = f->osiz = 12;
    f->sign = 'N';
}

static void
t_sfid(void)
{
    static const unsigned char desc[4] = { 0x4C, 0xC3, 0xB6, 0x77 };
    OFTP_SFIDCMD f;

    fill_sfid(&f, 'V', 255);
    CHECK_WIRE(oftp_enc_sfid(OFTP_LEV14, &f, out, sizeof(out)), v_sfid14, "SFID 1.4 encodes");

    fill_sfid(&f, 'T', 0);
    f.descl = sizeof(desc);
    f.desc = desc;
    CHECK_WIRE(oftp_enc_sfid(OFTP_LEV20, &f, out, sizeof(out)), v_sfid20, "SFID 2.0 encodes");

    fill_sfid(&f, 'F', 80);
    strcpy(f.time, "1745010000");
    CHECK_WIRE(oftp_enc_sfid(OFTP_LEV13, &f, out, sizeof(out)), v_sfid13, "SFID 1.3 encodes");

    memset(&f, 0, sizeof(f));
    CHECK_EQ(oftp_dec_sfid(OFTP_LEV13, v_sfid13, sizeof(v_sfid13), &f), 0, "SFID 1.3 decodes");
    CHECK(strcmp(f.date, "20261012") == 0, "SFID 1.3: century by window");
    CHECK(strcmp(f.time, "1745010000") == 0, "SFID 1.3: counter 0000");
    CHECK(f.fmt == 'F' && f.lrecl == 80, "SFID 1.3: format and LRECL");

    memset(&f, 0, sizeof(f));
    CHECK_EQ(oftp_dec_sfid(OFTP_LEV14, v_sfid14, sizeof(v_sfid14), &f), 0, "SFID 1.4 decodes");
    CHECK(strcmp(f.dsn, "PAYROLL.DATA") == 0, "SFID 1.4: name");
    CHECK(strcmp(f.time, "1745010001") == 0, "SFID 1.4: time");
    CHECK(strcmp(f.dest, "O0013000000MVS2") == 0 && strcmp(f.orig, "O0013000000MVS1") == 0,
          "SFID 1.4: destination and origin");
    CHECK(f.fsiz == 12 && f.rest == 0, "SFID 1.4: size and restart");

    memset(&f, 0, sizeof(f));
    CHECK_EQ(oftp_dec_sfid(OFTP_LEV20, v_sfid20, sizeof(v_sfid20), &f), 0, "SFID 2.0 decodes");
    CHECK(f.fmt == 'T' && f.osiz == 12 && f.sec == 0 && f.sign == 'N', "SFID 2.0 fields");
    CHECK(f.descl == 4 && f.desc == v_sfid20 + 165, "SFID 2.0: description points into the buffer");

    CHECK_EQ(oftp_dec_sfid(OFTP_LEV20, v_sfid20, sizeof(v_sfid20) - 1, &f), OFTP_ELEN,
             "SFID 2.0: description runs past the buffer");
    CHECK_EQ(oftp_dec_sfid(OFTP_LEV14, v_sfid20, sizeof(v_sfid20), &f), OFTP_ELEN,
             "SFID 2.0 command at level 1.4");

    fill_sfid(&f, 'F', 80);
    f.fsiz = 12345678;                  /* 8 digits: too wide for 9(7) */
    CHECK_EQ(oftp_enc_sfid(OFTP_LEV14, &f, out, sizeof(out)), OFTP_EFIELD,
             "SFID 1.4: size beyond 9(7)");
    CHECK(oftp_enc_sfid(OFTP_LEV20, &f, out, sizeof(out)) > 0, "SFID 2.0: same size fits 9(13)");
    CHECK_EQ(oftp_enc_sfid(OFTP_LEV14, &f, out, 127), OFTP_ESHORT, "SFID: short buffer");
}

static void
t_small(void)
{
    static const unsigned char big[18] = {
        0x32, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x34,
        0x32, 0x39, 0x34, 0x39, 0x36, 0x37, 0x32, 0x39, 0x36 };
    OFTP_REASCMD r;
    OFTP_EFIDCMD e;
    unsigned long n;
    char cd;

    CHECK_WIRE(oftp_enc_sfpa(OFTP_LEV14, 42, out, sizeof(out)), v_sfpa14, "SFPA 1.4");
    CHECK_WIRE(oftp_enc_sfpa(OFTP_LEV20, 42, out, sizeof(out)), v_sfpa20, "SFPA 2.0");
    CHECK(oftp_dec_sfpa(OFTP_LEV20, v_sfpa20, sizeof(v_sfpa20), &n) == 0 && n == 42, "SFPA 2.0 decodes");
    /* "2" + 00000004294967296: one beyond 32 bits */
    CHECK_EQ(oftp_dec_sfpa(OFTP_LEV20, big, sizeof(big), &n), OFTP_ERANGE, "SFPA: count beyond 32 bits");

    memset(&r, 0, sizeof(r));
    r.reas = 13;
    r.retry = 'Y';
    CHECK_WIRE(oftp_enc_reas(OFTP_LEV14, OFTP_SFNA, &r, out, sizeof(out)), v_sfna14, "SFNA 1.4");
    r.text = (const unsigned char *)v_sfna20 + 7;
    r.textl = 9;
    CHECK_WIRE(oftp_enc_reas(OFTP_LEV20, OFTP_SFNA, &r, out, sizeof(out)), v_sfna20, "SFNA 2.0");
    memset(&r, 0, sizeof(r));
    CHECK_EQ(oftp_dec_reas(OFTP_LEV20, OFTP_SFNA, v_sfna20, sizeof(v_sfna20), &r), 0, "SFNA 2.0 decodes");
    CHECK(r.reas == 13 && r.retry == 'Y' && r.textl == 9, "SFNA 2.0 fields");

    memset(&r, 0, sizeof(r));
    r.reas = 11;
    CHECK_WIRE(oftp_enc_reas(OFTP_LEV20, OFTP_EFNA, &r, out, sizeof(out)), v_efna20, "EFNA 2.0");
    r.reas = 0;
    CHECK_WIRE(oftp_enc_reas(OFTP_LEV14, OFTP_ESID, &r, out, sizeof(out)), v_esid14, "ESID 1.4");
    CHECK_EQ(oftp_dec_reas(OFTP_LEV20, OFTP_ESID, v_esid20, sizeof(v_esid20), &r), 0, "ESID 2.0 decodes");
    CHECK(r.reas == 3 && r.textl == 2 && r.text == v_esid20 + 6, "ESID 2.0: text before the CR");
    CHECK_EQ(oftp_dec_reas(OFTP_LEV14, OFTP_ESID, v_esid20, sizeof(v_esid20), &r), OFTP_ELEN,
             "ESID 2.0 command at level 1.4");

    e.rcnt = 1000;
    e.ucnt = 80000;
    CHECK_WIRE(oftp_enc_efid(OFTP_LEV14, &e, out, sizeof(out)), v_efid14, "EFID 1.4");
    CHECK_WIRE(oftp_enc_efid(OFTP_LEV20, &e, out, sizeof(out)), v_efid20, "EFID 2.0");
    memset(&e, 0, sizeof(e));
    CHECK(oftp_dec_efid(OFTP_LEV14, v_efid14, sizeof(v_efid14), &e) == 0
          && e.rcnt == 1000 && e.ucnt == 80000, "EFID 1.4 decodes");

    CHECK_EQ(oftp_enc_efpa('Y', out, sizeof(out)), 2, "EFPA encodes");
    CHECK(oftp_dec_efpa(out, 2, &cd) == 0 && cd == 'Y', "EFPA round trip");
    CHECK_EQ(oftp_enc_cdt(out, sizeof(out)), 3, "CDT encodes");
    CHECK_EQ(oftp_dec_cdt(out, 3), 0, "CDT round trip");
    CHECK_EQ(oftp_enc_cmd1(OFTP_CD, out, sizeof(out)), 1, "CD encodes");
    CHECK_EQ(oftp_dec_cmd1(OFTP_CD, out, 1), 0, "CD round trip");
    CHECK_EQ(oftp_dec_cmd1(OFTP_RTR, out, 1), OFTP_ECMD, "CD is not RTR");
    CHECK(oftp_cmd_code(v_efid14, sizeof(v_efid14)) == OFTP_EFID, "command code of EFID");
}

static void
fill_erp(OFTP_ERPCMD *c)
{
    memset(c, 0, sizeof(*c));
    strcpy(c->dsn, "PAYROLL.DATA");
    strcpy(c->date, "20261012");
    strcpy(c->time, "1745010001");
    strcpy(c->user, "U1");
    strcpy(c->dest, "O0013000000MVS1");
    strcpy(c->orig, "O0013000000MVS2");
}

static void
t_erp(void)
{
    OFTP_ERPCMD c;

    fill_erp(&c);
    CHECK_WIRE(oftp_enc_erp(OFTP_LEV20, OFTP_EERP, &c, out, sizeof(out)), v_eerp20,
               "EERP 2.0, unsigned");
    CHECK_WIRE(oftp_enc_erp(OFTP_LEV13, OFTP_EERP, &c, out, sizeof(out)), v_eerp13, "EERP 1.3");

    memset(&c, 0, sizeof(c));
    CHECK_EQ(oftp_dec_erp(OFTP_LEV20, OFTP_EERP, v_eerp20, sizeof(v_eerp20), &c), 0,
             "EERP 2.0 decodes");
    CHECK(c.hshl == 0 && c.sigl == 0 && strcmp(c.user, "U1") == 0, "EERP 2.0 fields");
    CHECK_EQ(oftp_dec_erp(OFTP_LEV13, OFTP_EERP, v_eerp13, sizeof(v_eerp13), &c), 0,
             "EERP 1.3 decodes");
    CHECK(strcmp(c.date, "20261012") == 0 && strcmp(c.time, "1745010000") == 0,
          "EERP 1.3: date and time in the 1.4 form");

    fill_erp(&c);
    strcpy(c.crea, "O0013000000MVS2");
    c.reas = 12;
    CHECK_WIRE(oftp_enc_erp(OFTP_LEV20, OFTP_NERP, &c, out, sizeof(out)), v_nerp20, "NERP 2.0");
    CHECK_EQ(oftp_enc_erp(OFTP_LEV13, OFTP_NERP, &c, out, sizeof(out)), OFTP_ELEVEL,
             "NERP does not exist at 1.3");
    memset(&c, 0, sizeof(c));
    CHECK_EQ(oftp_dec_erp(OFTP_LEV20, OFTP_NERP, v_nerp20, sizeof(v_nerp20), &c), 0,
             "NERP 2.0 decodes");
    CHECK(c.reas == 12 && strcmp(c.crea, "O0013000000MVS2") == 0, "NERP 2.0 fields");
}

int
main(void)
{
    printf("=== TSTCMD OFTP command codec ===\n");
    t_ssrm();
    t_ssid();
    t_sfid();
    t_small();
    t_erp();
    return mbt_test_summary("TSTCMD");
}
