/* TSTTAKE - what oftp_svc_take() leaves in a CSA request slot (MVS)
**
** A PARTNER request can carry a password, and the slot is readable from
** key 8. The slot is wiped when OFTPD has copied the request out, and
** also when the caller turned out to be gone and the request is dropped
** unread (#113); the sweep wipes a reply nobody will collect, and the
** stop wipes every slot before the anchor is freed (#121). None of it
** needs a running OFTPD: the anchor here is ordinary storage.
**
** oftp_svc_take() brackets its access in key 0, so this must run from
** an APF-authorized library with AC(1), like TSTPROT; the job fails its
** first check otherwise, and measures nothing. Gated on PARM='RUN'
** (test/mvs/tsttake.jcl).
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>
#include <mvs/apf.h>
#include <mvs/storage.h>
#include <mvs/xmem.h>

#include "oftpmon.h"
#include "oftpsvc.h"

#define ASCB_ASID   0x24                /* ASCBASID, halfword */

void
oftp_mon_msg(const char *fmt, ...)      // NOSONAR: must match the prototype
{
    (void)fmt;
}

static const char secret[] = "PARTNER ADD PARTNER=ACME PASSWORD=SECRET1";

static int
all_zero(const char *p, size_t n)
{
    while (n-- != 0)
        if (*p++ != 0)
            return 0;
    return 1;
}

/* A slot in the state given, request and reply full of text */
static void
fill(OFTP_ANCHOR *a, unsigned i, unsigned state)
{
    OFTP_SLOT *s = &a->slot[i];

    memset(s, 0, sizeof(*s));
    memcpy(s->req, secret, sizeof(secret) - 1);
    memset(s->reply, 'R', sizeof(s->reply));
    s->reqlen = (unsigned)(sizeof(secret) - 1);
    s->replen = (unsigned)sizeof(s->reply);
    s->state  = state;
}

/* Slot 0 pending with a request in it; the caller is the one given */
static void
arm(OFTP_ANCHOR *a, unsigned asid, void *ascb)
{
    OFTP_SLOT *s = &a->slot[0];

    memset(s, 0, sizeof(*s));
    memcpy(s->req, secret, sizeof(secret) - 1);
    s->reqlen = (unsigned)(sizeof(secret) - 1);
    s->asid   = asid;
    s->ascb   = ascb;
    memcpy(s->user, "IBMUSER ", 8);
    s->cap    = OFTP_SVC_REPMIN;
    s->state  = OFTP_SL_PENDING;
    a->inflight = 1;
}

int
main(int argc, char **argv)
{
    OFTP_MON    *m;
    OFTP_ANCHOR *a;
    OFTP_SVCJOB  j;
    void        *ascb;
    unsigned     asid;
    int          got;

    printf("=== TSTTAKE the CSA request slot ===\n");
    if (argc < 2 || strcmp(argv[1], "RUN") != 0) {
        printf("SKIP: PARM='RUN' from an APF-authorized library only\n");
        return 0;
    }
    CHECK(__isauth() != 0, "the library is APF-authorized");
    if (__isauth() == 0)
        return mbt_test_summary("TSTTAKE");

    m = calloc(1, sizeof(*m));
    a = calloc(1, sizeof(*a));
    CHECK(m != NULL && a != NULL, "storage");
    if (m == NULL || a == NULL)
        return mbt_test_summary("TSTTAKE");
    m->anchor = a;
    a->nslot  = OFTP_SVC_NSLOT;
    ascb = __ascb(0);
    asid = *(unsigned short *)((char *)ascb + ASCB_ASID);

    /* the owner is alive: the request is copied out and the slot wiped */
    arm(a, asid, ascb);
    memset(&j, 0, sizeof(j));
    got = oftp_svc_take(m, 0, &j);
    CHECK_EQ(got, 1, "a live caller's request is taken");
    CHECK(strcmp(j.req, secret) == 0, "and copied whole");
    CHECK(all_zero(a->slot[0].req, sizeof(a->slot[0].req)),
          "the slot no longer holds it");

    /* the owner is gone: nothing is carried out, and the slot must not
    ** keep the request either (#113) */
    arm(a, 0, NULL);
    memset(&j, 0, sizeof(j));
    got = oftp_svc_take(m, 0, &j);
    CHECK_EQ(got, 0, "a gone caller's request is not taken");
    CHECK_EQ((int)a->slot[0].state, OFTP_SL_FREE, "its slot is free");
    CHECK_EQ((int)a->inflight, 0, "and its in-flight count is given back");
    CHECK(all_zero(a->slot[0].req, sizeof(a->slot[0].req)),
          "and the request, with its password, is not left in it");

    /* a reply nobody collects: the caller is gone, and the sweep frees
    ** the slot (#121) */
    fill(a, 0, OFTP_SL_DONE);
    a->inflight = 1;
    oftp_svc_sweep(m);
    CHECK_EQ((int)a->slot[0].state, OFTP_SL_FREE, "a DONE slot of a gone caller is freed");
    CHECK_EQ((int)a->inflight, 0, "and its in-flight count is given back");
    CHECK(all_zero(a->slot[0].reply, sizeof(a->slot[0].reply)),
          "its reply is not left in it");
    CHECK(all_zero(a->slot[0].req, sizeof(a->slot[0].req)),
          "and nor is its request");

    /* the stop wipes every slot, whatever it holds, before the free */
    {
        unsigned i, clean = 0;

        fill(a, 0, OFTP_SL_FREE);
        fill(a, 1, OFTP_SL_PENDING);
        fill(a, 2, OFTP_SL_DONE);
        fill(a, 3, OFTP_SL_STOP);
        oftp_svc_scrub(m);
        for (i = 0; i < OFTP_SVC_NSLOT; i++)
            if (all_zero(a->slot[i].req, sizeof(a->slot[i].req))
                && all_zero(a->slot[i].reply, sizeof(a->slot[i].reply)))
                clean++;
        CHECK_EQ((int)clean, OFTP_SVC_NSLOT,
                 "oftp_svc_scrub() wipes request and reply of every slot");
    }

    free(a);
    free(m);
    return mbt_test_summary("TSTTAKE");
}
