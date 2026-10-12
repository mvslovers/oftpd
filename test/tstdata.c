/* TSTDATA - DATA subrecords: the RFC 5024 example, compression, splitting */
#include <string.h>

#include <mbtcheck.h>

#include "oftpdata.h"
#include "oftpwire.h"
#include "rime.h"

static unsigned char user[1024];
static unsigned char buf[1024];
static unsigned char back[2048];

/* Decodes one buffer, appending to back; counts records ended */
static int
decode(const unsigned char *b, unsigned len, int cmpr, unsigned *blen,
       int *recs)
{
    OFTP_DDEC d;
    OFTP_DSUB s;
    int rc;

    if (oftp_ddec_init(&d, b, len) != 0)
        return -1;
    while ((rc = oftp_ddec_next(&d, cmpr, &s)) == 1) {
        if (s.data != NULL)
            memcpy(back + *blen, s.data, s.n);
        else
            memset(back + *blen, s.rep, s.n);
        *blen += s.n;
        if (s.eor)
            (*recs)++;
    }
    return rc;
}

static void
t_rime(void)
{
    OFTP_DDEC d;
    OFTP_DSUB s;
    OFTP_DENC e;
    unsigned n = 0, subs = 0, eors = 0;
    int done;

    CHECK_EQ(oftp_ddec_init(&d, rime_buf, sizeof(rime_buf)), 0, "RFC example is a DATA buffer");
    while (oftp_ddec_next(&d, 0, &s) == 1) {
        memcpy(user + n, s.data, s.n);
        n += s.n;
        subs++;
        if (s.eor)
            eors++;
    }
    CHECK_EQ(subs, 13, "RFC example: 13 subrecords");
    CHECK_EQ(n, 807, "RFC example: 807 octets of text");
    CHECK(eors == 1 && s.eor, "RFC example: one end of record, on the last");
    /* CR-LF travels inside a T file: the first line ends at octet 26 */
    CHECK(user[25] == 0x0D && user[26] == 0x0A, "RFC example: CR-LF in the data");

    /* Encoding the same 807 octets as one record gives the same buffer */
    oftp_denc_init(&e, buf, sizeof(rime_buf), 0);
    CHECK_EQ(oftp_denc_put(&e, user, n, 1, &done), n, "re-encode takes all octets");
    CHECK(done && e.len == sizeof(rime_buf)
          && memcmp(buf, rime_buf, sizeof(rime_buf)) == 0,
          "re-encoded buffer is byte-identical to the RFC");
}

static void
t_compress(void)
{
    /* RFC 2204 App. A: X'45' X'20' is five blanks */
    static const unsigned char rfc[5] = { 0x44, 0x45, 0x20, 0x81, 0x78 };
    OFTP_DENC e;
    unsigned blen = 0, i;
    int recs = 0, done;

    CHECK_EQ(decode(rfc, sizeof(rfc), 1, &blen, &recs), 0, "RFC 2204 compressed subrecord");
    CHECK(blen == 6 && memcmp(back, "\x20\x20\x20\x20\x20\x78", 6) == 0 && recs == 1,
          "five blanks and one octet, one record");
    blen = 0;
    CHECK_EQ(decode(rfc, sizeof(rfc), 0, &blen, &recs), OFTP_EFIELD,
             "compressed subrecord refused when not negotiated");

    /* 300 octets: a run of 200, a short run of 3, varied data */
    memset(user, 0x40, 200);
    memset(user + 200, 0x41, 3);
    for (i = 203; i < 300; i++)
        user[i] = (unsigned char)i;
    oftp_denc_init(&e, buf, sizeof(buf), 1);
    CHECK_EQ(oftp_denc_put(&e, user, 300, 1, &done), 300, "compressed encode takes all");
    CHECK(done && e.len < 120, "a run of 200 shrinks to a few subrecords");
    blen = 0;
    recs = 0;
    CHECK_EQ(decode(buf, e.len, 1, &blen, &recs), 0, "compressed buffer decodes");
    CHECK(blen == 300 && memcmp(back, user, 300) == 0 && recs == 1,
          "compressed round trip is exact");
}

static void
t_split(void)
{
    /* Records of 0, 1, 63, 64, 200 octets through 32-octet buffers */
    static const unsigned lens[5] = { 0, 1, 63, 64, 200 };
    OFTP_DENC e;
    unsigned i, total = 0, blen = 0, bufs = 0;
    int recs = 0, done, bad = 0;

    for (i = 0; i < sizeof(user); i++)
        user[i] = (unsigned char)(i * 7);

    oftp_denc_init(&e, buf, 32, 0);
    for (i = 0; i < 5; i++) {
        unsigned off = 0;

        for (;;) {
            off += oftp_denc_put(&e, user + total + off, lens[i] - off, 1, &done);
            if (done)
                break;
            if (decode(buf, e.len, 0, &blen, &recs) != 0)
                bad++;
            bufs++;
            oftp_denc_init(&e, buf, 32, 0);
        }
        total += lens[i];
    }
    if (decode(buf, e.len, 0, &blen, &recs) != 0)
        bad++;
    bufs++;

    CHECK_EQ(bad, 0, "every buffer decodes");
    CHECK(bufs > 9, "the records span several buffers");
    CHECK_EQ(recs, 5, "five records, the empty one included");
    CHECK(blen == total && memcmp(back, user, total) == 0, "split data is exact");
}

int
main(void)
{
    printf("=== TSTDATA DATA subrecords ===\n");
    t_rime();
    t_compress();
    t_split();
    return mbt_test_summary("TSTDATA");
}
