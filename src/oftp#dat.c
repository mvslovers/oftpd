/* oftp#dat.c - DATA subrecords: encoder and decoder
**
** Buffer compression codes a run of identical octets as HDR + the octet.
** No source sets a minimum run; at 2 octets per compressed subrecord a
** run of 3 merely breaks even, so runs of 4 and more are compressed.
*/
#include <string.h>

#include "oftpdata.h"
#include "oftpfld.h"
#include "oftpwire.h"

#define MINRUN          4

/* Length of the run of p[0], at most max and at most OFTP_SR_MAX */
static unsigned
run(const unsigned char *p, unsigned max)
{
    unsigned r = 1;

    if (max > OFTP_SR_MAX)
        max = OFTP_SR_MAX;
    while (r < max && p[r] == p[0])
        r++;
    return r;
}

void
oftp_denc_init(OFTP_DENC *e, unsigned char *buf, unsigned cap, int cmpr)
{
    e->buf = buf;
    e->cap = cap;
    e->cmpr = cmpr;
    oftp_put_c(buf, OFTP_DATA);
    e->len = 1;
}

unsigned
oftp_denc_put(OFTP_DENC *e, const unsigned char *data, unsigned n, int eor,
              int *done)
{
    unsigned taken = 0;

    *done = 0;
    if (n == 0) {
        if (eor) {
            if (e->len >= e->cap)
                return 0;
            e->buf[e->len++] = OFTP_SR_EOR;
        }
        *done = 1;
        return 0;
    }

    while (taken < n) {
        const unsigned char *p = data + taken;
        unsigned rest = n - taken;
        unsigned room = e->cap - e->len;
        unsigned k, max;
        unsigned char hdr;

        if (room < 2)
            return taken;

        if (e->cmpr) {
            unsigned r = run(p, rest);

            if (r >= MINRUN) {
                hdr = (unsigned char)(OFTP_SR_CMP | r);
                if (eor && r == rest)
                    hdr |= OFTP_SR_EOR;
                e->buf[e->len++] = hdr;
                e->buf[e->len++] = p[0];
                taken += r;
                continue;
            }
        }

        max = rest;
        if (max > OFTP_SR_MAX)
            max = OFTP_SR_MAX;
        if (max > room - 1)
            max = room - 1;
        k = 1;
        while (k < max && !(e->cmpr && run(p + k, rest - k) >= MINRUN))
            k++;

        hdr = (unsigned char)k;
        if (eor && k == rest)
            hdr |= OFTP_SR_EOR;
        e->buf[e->len++] = hdr;
        memcpy(e->buf + e->len, p, k);
        e->len += k;
        taken += k;
    }
    *done = 1;
    return taken;
}

int
oftp_ddec_init(OFTP_DDEC *d, const unsigned char *buf, unsigned len)
{
    if (len < 1 || oftp_get_c(buf) != OFTP_DATA)
        return OFTP_ECMD;
    d->buf = buf;
    d->len = len;
    d->pos = 1;
    return 0;
}

int
oftp_ddec_next(OFTP_DDEC *d, int cmpr, OFTP_DSUB *s)
{
    unsigned char hdr;

    if (d->pos >= d->len)
        return 0;
    hdr = d->buf[d->pos++];
    s->n = hdr & OFTP_SR_MAX;
    s->eor = (hdr & OFTP_SR_EOR) != 0;
    if (hdr & OFTP_SR_CMP) {
        if (!cmpr)
            return OFTP_EFIELD;
        if (d->pos + 1 > d->len)
            return OFTP_ELEN;
        s->data = NULL;
        s->rep = d->buf[d->pos++];
        return 1;
    }
    if (d->pos + s->n > d->len)
        return OFTP_ELEN;
    s->data = d->buf + d->pos;
    s->rep = 0;
    d->pos += s->n;
    return 1;
}
