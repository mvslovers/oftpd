/* TSTKSDS - libc370's VSAM calls against a real KSDS (MVS only)
**
** The calls the spool will make: the initial load of an empty cluster,
** then insert, read by key, update, delete, generic positioning,
** sequential reading and a duplicate key.
**
** Needs DD KSDS: a KSDS with an 8-byte key at offset 0, defined and never
** loaded (test/mvs/tstksds.jcl does that). Without PARM='RUN' -- as under
** mbt test --mvs, whose steps carry no such DD -- it reports itself
** skipped and ends RC 0.
*/
#include <stdio.h>
#include <string.h>

#include <mvs/vsam.h>

#include <mbtcheck.h>

#define RECLEN  80

static char rec[RECLEN + 1];

/* A direct PUT: what an insert at an arbitrary key needs. libc370's
** vswrite() keeps the RPL's OPTCD=SEQ on a dynamic handle, so its PUT is
** sequential. Measured here side by side.
*/
static int
put_direct(VSFILE *vs, void *r, int len, void *key)
{
    unsigned pl[16] = {0};
    volatile int rc = 0;

    __vsmdfy(vs, r, len, key, 8);
    __asm__("MODCB RPL=(%0),OPTCD=(DIR,NUP),MF=(G,(%1))"
            : : "r"(&vs->rpl), "r"(pl) : "1", "14", "15", "memory");
    __asm__("LA\t2,PUTDDONE\n\t"
            "PUT\tRPL=(%0)\n"
   "PUTDDONE\tDS\t0H\n\t"
            "ST\t15,%1"
        : : "r"(&vs->rpl), "m"(rc) : "1", "2", "14", "15", "memory");
    __asm__("MODCB RPL=(%0),OPTCD=(SEQ,UPD),MF=(G,(%1))"
            : : "r"(&vs->rpl), "r"(pl) : "1", "14", "15", "memory");
    return rc;
}

static void
mkrec(const char *key, const char *text)
{
    memset(rec, ' ', RECLEN);
    memcpy(rec, key, 8);
    memcpy(rec + 8, text, strlen(text));
    rec[RECLEN] = '\0';
}

int
main(int argc, char **argv)
{
    static const char *keys[5] = {
        "T0000001", "T0000002", "T0000003", "T0000004", "T0000005" };
    VSFILE *vs;
    char buf[RECLEN + 1];
    int rc, i, n;

    printf("=== TSTKSDS VSAM KSDS ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("  SKIP: needs DD KSDS and PARM='RUN' (test/mvs/tstksds.jcl)\n");
        return 0;
    }

    /* 1. An empty cluster is loaded once, sequentially */
    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_OUT, &vs);
    CHECK_EQ(rc, 0, "open the empty cluster for loading");
    if (rc != 0)
        return mbt_test_summary("TSTKSDS");
    mkrec("\0\0\0\0\0\0\0\0", "CONTROL 0000000");
    CHECK_EQ(vswrite(vs, rec, RECLEN, NULL, 0), 0, "load the control record");
    vsclose(vs);

    /* 2. Dynamic access for update: what the spool server does */
    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, &vs);
    CHECK_EQ(rc, 0, "open for update, dynamic access");
    if (rc != 0)
        return mbt_test_summary("TSTKSDS");

    for (i = 0, n = 0; i < 5; i++) {
        mkrec(keys[i], "QUEUED");
        if (vswrite(vs, rec, RECLEN, (void *)keys[i], 8) == 0)
            n++;
    }
    CHECK_EQ(n, 5, "insert five transfer records");

    /* vswrite: a key below the position, then a duplicate */
    mkrec("E0000001", "EERP");
    rc = vswrite(vs, rec, RECLEN, "E0000001", 8);
    printf("    (vswrite lower key: rc %d reason %d)\n", rc, vs->rsn);
    CHECK(rc == 8 && vs->rsn == 12, "vswrite of a lower key: out of sequence (12)");
    vsclear(vs);
    mkrec(keys[2], "DUPLICATE");
    rc = vswrite(vs, rec, RECLEN, (void *)keys[2], 8);
    printf("    (vswrite duplicate: rc %d reason %d)\n", rc, vs->rsn);
    vsclear(vs);

    /* direct PUT: the same two */
    mkrec("E0000001", "EERP");
    rc = put_direct(vs, rec, RECLEN, "E0000001");
    printf("    (direct lower key: rc %d reason %d)\n", rc, vs->rsn);
    CHECK_EQ(rc, 0, "direct PUT of a lower key");
    vsclear(vs);
    mkrec(keys[2], "DUPLICATE");
    rc = put_direct(vs, rec, RECLEN, (void *)keys[2]);
    printf("    (direct duplicate: rc %d reason %d)\n", rc, vs->rsn);
    CHECK(rc == 8 && vs->rsn == 8, "direct PUT of a duplicate: reason 8");
    vsclear(vs);

    rc = vsread(vs, buf, RECLEN, (void *)keys[2], 8);
    CHECK(rc == RECLEN && memcmp(buf + 8, "QUEUED", 6) == 0, "read by key");
    mkrec(keys[2], "ACTIVE");
    CHECK_EQ(vsupdate(vs, rec, RECLEN), 0, "update the record just read");
    rc = vsread(vs, buf, RECLEN, (void *)keys[2], 8);
    CHECK(rc == RECLEN && memcmp(buf + 8, "ACTIVE", 6) == 0, "update is visible");

    rc = vsread(vs, buf, RECLEN, (void *)keys[1], 8);
    CHECK(rc == RECLEN, "read before delete");
    CHECK_EQ(vsdelete(vs, buf, RECLEN), 0, "delete the record just read");
    rc = vsread(vs, buf, RECLEN, (void *)keys[1], 8);
    printf("    (vsread deleted key: rc %d reason %d)\n", rc, vs->rsn);
    CHECK(rc == -2, "deleted key: vsread fails");
    vsclear(vs);
    rc = vssteq(vs, buf, RECLEN, (void *)keys[1], 8);
    printf("    (vssteq deleted key: rc %d reason %d)\n", rc, vs->rsn);
    CHECK(rc == 8 && vs->rsn == 16, "deleted key: vssteq says not found (16)");
    vsclear(vs);

    /* generic START on "T", then read on: the four transfers left */
    rc = vsstge(vs, buf, RECLEN, (void *)"T", 1);
    CHECK_EQ(rc, 0, "position at the first key >= 'T'");
    n = 0;
    while (vsread(vs, buf, RECLEN, NULL, 0) == RECLEN)
        if (buf[0] == 'T')
            n++;
    CHECK_EQ(n, 4, "four transfer records in key order");
    CHECK(vseof(vs), "end of data reached");
    vsclear(vs);
    vsclose(vs);

    /* 3. Everything survived the close */
    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    CHECK_EQ(rc, 0, "reopen for input");
    if (rc == 0) {
        n = 0;
        while (vsread(vs, buf, RECLEN, NULL, 0) == RECLEN)
            n++;
        CHECK_EQ(n, 6, "control, EERP and four transfers after reopen");
        vsclose(vs);
    }
    return mbt_test_summary("TSTKSDS");
}
