/* oftp#prg.c - which transfers the spool's housekeeping may delete
** (portable; the deleting is oftp_spl_purge() in src/mvs/oftp#spl.c)
**
** A transfer is finished when nothing more will happen to it by itself:
** delivered (EERP in), confirmed (EERP out), rejected or failed. Its age
** is its last change -- the latest of the queue, attempt and end times,
** since not every way to an end sets tdone. Only 'T' records count: the
** control record and the EERPs still to be sent are not transfers.
*/
#include "oftpspl.h"

int
oftp_spl_finished(const OFTP_SPREC *r)
{
    return r->state == SPL_DELIVERED || r->state == SPL_CONFIRMED
        || r->state == SPL_REJECTED || r->state == SPL_FAILED;
}

long
oftp_spl_changed(const OFTP_SPREC *r)
{
    long t = r->tqueued;

    if (r->tlast > t)
        t = r->tlast;
    if (r->tdone > t)
        t = r->tdone;
    return t;
}

int
oftp_spl_purgeable(const OFTP_SPREC *r, long before)
{
    return r->key[0] == 'T' && oftp_spl_finished(r)
        && oftp_spl_changed(r) < before;
}
