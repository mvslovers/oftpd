/* TSTPRG - which transfers the spool's housekeeping may delete */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpspl.h"

static OFTP_SPREC
rec(char state, long queued, long last, long done)
{
    OFTP_SPREC r;

    memset(&r, 0, sizeof(r));
    memcpy(r.key, "T0000001", 8);
    r.dir = SPL_SEND;
    r.state = state;
    r.tqueued = queued;
    r.tlast = last;
    r.tdone = done;
    return r;
}

int
main(void)
{
    static const char done[] = { SPL_DELIVERED, SPL_CONFIRMED, SPL_REJECTED,
                                 SPL_FAILED };
    static const char open[] = { SPL_QUEUED, SPL_ACTIVE, SPL_WAITING,
                                 SPL_HELD, SPL_SENT, SPL_RECEIVED };
    OFTP_SPREC r;
    unsigned i;

    printf("=== TSTPRG spool housekeeping ===\n");

    for (i = 0; i < sizeof(done); i++) {
        r = rec(done[i], 100, 200, 300);
        CHECK(oftp_spl_finished(&r), "a finished state");
        CHECK(oftp_spl_purgeable(&r, 301), "finished and older: purged");
        CHECK(!oftp_spl_purgeable(&r, 300), "finished at the limit: kept");
    }
    for (i = 0; i < sizeof(open); i++) {
        r = rec(open[i], 100, 200, 300);
        CHECK(!oftp_spl_finished(&r), "an open state");
        CHECK(!oftp_spl_purgeable(&r, 1000000), "never purged, however old");
    }

    /* the age is the last change: whichever of the three times is latest */
    r = rec(SPL_DELIVERED, 100, 500, 300);
    CHECK_EQ(oftp_spl_changed(&r), 500, "a later attempt counts");
    CHECK(!oftp_spl_purgeable(&r, 400), "so it is younger than 400");
    r = rec(SPL_REJECTED, 100, 0, 0);
    CHECK_EQ(oftp_spl_changed(&r), 100, "no attempt, no end: the queue time");
    CHECK(oftp_spl_purgeable(&r, 101), "and that decides");

    /* only transfers: the control record and EERP records are not ours */
    r = rec(SPL_DELIVERED, 1, 1, 1);
    r.key[0] = 'E';
    CHECK(!oftp_spl_purgeable(&r, 1000), "an E record is never purged");

    /* HOLD/RELEASE by partner and name */
    r = rec(SPL_QUEUED, 1, 1, 1);
    strcpy(r.partner, "PEER");
    strcpy(r.dsn, "ORDERS.DAILY");
    CHECK(oftp_spl_match(&r, "PEER", ""), "the partner, every name");
    CHECK(oftp_spl_match(&r, "*", ""), "every partner");
    CHECK(oftp_spl_match(&r, "", ""), "no partner given: every partner");
    CHECK(!oftp_spl_match(&r, "OTHER", ""), "another partner: no");
    CHECK(!oftp_spl_match(&r, "PEE", ""), "a partner name is not a prefix");
    CHECK(oftp_spl_match(&r, "PEER", "ORDERS.DAILY"), "the exact name");
    CHECK(!oftp_spl_match(&r, "PEER", "ORDERS"), "a name is not a prefix");
    CHECK(oftp_spl_match(&r, "PEER", "ORDERS*"), "a prefix with *");
    CHECK(oftp_spl_match(&r, "*", "*"), "* alone is every name");
    CHECK(!oftp_spl_match(&r, "PEER", "INVOICES*"), "another prefix: no");

    return mbt_test_summary("TSTPRG");
}
