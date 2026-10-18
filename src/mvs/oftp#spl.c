/* oftp#spl.c - the spool KSDS, owned by one thread (MVS only)
**
** Requests are queued under a lock and the spool thread is posted; the
** caller waits on its own ECB. Only the spool thread issues VSAM
** requests, so every change to the spool happens in one sequence. OPEN
** and CLOSE are the exception: both run in the task that calls
** oftp_spl_open() and oftp_spl_close(), because CLOSE frees what OPEN's
** TCB owns -- closed from the spool thread, it ended in S30A (#12).
**
** Inserts need libc370 2.10.1 or later: before it, vswrite() on a
** dynamic handle was a sequential PUT and could not insert below its
** position (libc370#514). Existence is tested with vssteq(), which
** reports a missing key as reason 16.
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mvs/ecb.h>
#include <mvs/idcams.h>
#include <mvs/lock.h>
#include <mvs/thread.h>
#include <mvs/vsam.h>

#include "oftpspl.h"

/* the struct must fit the fixed record */
typedef char sprec_fits[sizeof(OFTP_SPREC) <= OFTP_SPLRECL ? 1 : -1];

enum { OP_ADD, OP_GET, OP_PUT, OP_PUTIF, OP_DEL, OP_NEXT, OP_PURGE, OP_STOP };

typedef struct spl_req {
    struct spl_req *next;
    int         op;
    char        key[8];
    OFTP_SPREC *rec;
    long        before;                 /* OP_PURGE                      */
    char        was;                    /* OP_PUTIF: the state expected  */
    unsigned    count;                  /* OP_PURGE: records deleted     */
    int         rc;
    ECB         done;
} SPL_REQ;

struct oftp_spool {
    char        eye[8];
    char        ddname[9];
    VSFILE     *vs;
    SPL_REQ    *head, *tail;
    ECB         work;
    CTHDTASK   *task;
    int         lastrc, lastrsn;        /* VSAM codes of the last failure */
    int         verified;               /* open rc that made us VERIFY    */
    unsigned char buf[OFTP_SPLRECL];
};

static const char ctlkey[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Reads the record with this key into sp->buf: 0, NOTFOUND or ERROR */
static int
read_key(OFTP_SPOOL *sp, const char *key)
{
    int rc;

    vsclear(sp->vs);
    rc = vssteq(sp->vs, sp->buf, OFTP_SPLRECL, (void *)key, 8);
    if (rc != 0) {
        sp->lastrc = sp->vs->rc;
        sp->lastrsn = sp->vs->rsn;
        rc = sp->vs->rsn == 16 ? OFTP_SPL_NOTFOUND : OFTP_SPL_ERROR;
        vsclear(sp->vs);
        return rc;
    }
    if (vsread(sp->vs, sp->buf, OFTP_SPLRECL, NULL, 0) != OFTP_SPLRECL) {
        sp->lastrc = sp->vs->rc;
        sp->lastrsn = sp->vs->rsn;
        vsclear(sp->vs);
        return OFTP_SPL_ERROR;
    }
    return 0;
}

static int
do_add(OFTP_SPOOL *sp, OFTP_SPREC *rec)
{
    unsigned long no;
    char num[12];

    if (read_key(sp, ctlkey) != 0)
        return OFTP_SPL_ERROR;
    /* control record: the key, then the last number as 7 digits */
    no = strtoul((char *)sp->buf + 8, NULL, 10) + 1;
    if (no > 9999999)
        no = 1;
    snprintf(num, sizeof(num), "%07lu", no);
    memcpy(sp->buf + 8, num, 7);
    if (vsupdate(sp->vs, sp->buf, OFTP_SPLRECL) != 0) {
        vsclear(sp->vs);
        return OFTP_SPL_ERROR;
    }
    rec->key[0] = 'T';
    memcpy(rec->key + 1, num, 7);
    memset(sp->buf, 0, OFTP_SPLRECL);
    memcpy(sp->buf, rec, sizeof(*rec));
    if (vswrite(sp->vs, sp->buf, OFTP_SPLRECL, rec->key, 8) != 0) {
        vsclear(sp->vs);
        return OFTP_SPL_ERROR;
    }
    return 0;
}

static int
do_next(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
{
    char from[8];
    int n;

    if (key[0] == '\0') {
        memset(from, 0, sizeof(from));
        from[0] = 'T';
    } else
        memcpy(from, key, 8);
    vsclear(sp->vs);
    if (vsstge(sp->vs, sp->buf, OFTP_SPLRECL, from, 8) != 0) {
        vsclear(sp->vs);
        return OFTP_SPL_NOTFOUND;
    }
    for (;;) {
        n = vsread(sp->vs, sp->buf, OFTP_SPLRECL, NULL, 0);
        if (n != OFTP_SPLRECL) {
            vsclear(sp->vs);
            return n == -1 ? OFTP_SPL_NOTFOUND : OFTP_SPL_ERROR;
        }
        if (sp->buf[0] != 'T')
            return OFTP_SPL_NOTFOUND;
        if (key[0] == '\0' || memcmp(sp->buf, key, 8) > 0)
            break;
    }
    memcpy(rec, sp->buf, sizeof(*rec));
    return 0;
}

/* Walks every transfer and deletes the ones oftp_spl_purgeable() lets
** go. Each delete is a fresh keyed read and an ERASE, and the walk goes
** on from the key just seen, so a deleted record never moves it back.
*/
static int
do_purge(OFTP_SPOOL *sp, long before, unsigned *count, OFTP_SPREC *rec)
{
    char last[8];
    int rc;

    *count = 0;
    last[0] = '\0';
    while ((rc = do_next(sp, last, rec)) == 0) {
        memcpy(last, rec->key, 8);
        if (!oftp_spl_purgeable(rec, before))
            continue;
        if (read_key(sp, rec->key) != 0
            || vsdelete(sp->vs, sp->buf, OFTP_SPLRECL) != 0) {
            vsclear(sp->vs);
            return OFTP_SPL_ERROR;
        }
        (*count)++;
    }
    return rc == OFTP_SPL_NOTFOUND ? 0 : rc;
}

static void
serve(OFTP_SPOOL *sp, SPL_REQ *r)
{
    switch (r->op) {
    case OP_ADD:
        r->rc = do_add(sp, r->rec);
        break;
    case OP_GET:
        if ((r->rc = read_key(sp, r->key)) == 0)
            memcpy(r->rec, sp->buf, sizeof(*r->rec));
        break;
    case OP_PUT:
        if ((r->rc = read_key(sp, r->key)) == 0) {
            memset(sp->buf, 0, OFTP_SPLRECL);
            memcpy(sp->buf, r->rec, sizeof(*r->rec));
            if (vsupdate(sp->vs, sp->buf, OFTP_SPLRECL) != 0) {
                vsclear(sp->vs);
                r->rc = OFTP_SPL_ERROR;
            }
        }
        break;
    case OP_PUTIF:
        if ((r->rc = read_key(sp, r->key)) == 0) {
            if (sp->buf[offsetof(OFTP_SPREC, state)] != (unsigned char)r->was) {
                r->rc = OFTP_SPL_CHANGED;
                break;
            }
            memset(sp->buf, 0, OFTP_SPLRECL);
            memcpy(sp->buf, r->rec, sizeof(*r->rec));
            if (vsupdate(sp->vs, sp->buf, OFTP_SPLRECL) != 0) {
                vsclear(sp->vs);
                r->rc = OFTP_SPL_ERROR;
            }
        }
        break;
    case OP_DEL:
        if ((r->rc = read_key(sp, r->key)) == 0
            && vsdelete(sp->vs, sp->buf, OFTP_SPLRECL) != 0) {
            vsclear(sp->vs);
            r->rc = OFTP_SPL_ERROR;
        }
        break;
    case OP_NEXT:
        r->rc = do_next(sp, r->key, r->rec);
        break;
    case OP_PURGE:
        r->rc = do_purge(sp, r->before, &r->count, r->rec);
        break;
    default:
        r->rc = 0;
    }
}

static int
spool_thread(void *arg1, void *arg2)
{
    OFTP_SPOOL *sp = arg1;
    int stop = 0;

    (void)arg2;
    while (!stop) {
        SPL_REQ *r;

        ecb_wait(&sp->work);
        sp->work = 0;
        for (;;) {
            lock(sp, 0);
            r = sp->head;
            if (r != NULL) {
                sp->head = r->next;
                if (sp->head == NULL)
                    sp->tail = NULL;
            }
            unlock(sp, 0);
            if (r == NULL)
                break;
            if (r->op == OP_STOP)
                stop = 1;
            else
                serve(sp, r);
            ecb_post(&r->done, 0);
        }
    }
    /* No vsclose() here: the cluster is closed by the task that opened
    ** it, in oftp_spl_close(). VSAM's CLOSE frees control blocks the
    ** opening TCB owns, and from this subtask that FREEMAIN failed: an
    ** S30A at every end of OFTPD, in CLOSE (SVC 20) issuing SVC 10 (#12).
    */
    return 0;
}

/* Queues r for the spool thread and waits until it is served */
static int
enqueue(OFTP_SPOOL *sp, SPL_REQ *r)
{
    lock(sp, 0);
    if (sp->tail)
        sp->tail->next = r;
    else
        sp->head = r;
    sp->tail = r;
    unlock(sp, 0);
    ecb_post(&sp->work, 0);
    ecb_wait(&r->done);
    return r->rc;
}

static int
request(OFTP_SPOOL *sp, int op, const char *key, OFTP_SPREC *rec)
{
    SPL_REQ r;

    memset(&r, 0, sizeof(r));
    r.op = op;
    if (key != NULL)
        memcpy(r.key, key, key[0] ? 8 : 1);
    r.rec = rec;
    return enqueue(sp, &r);
}

/* Opens for update; a cluster never loaded first gets its control record
** through a sequential load. A cluster left open by an abend fails the
** open (IEC161I 056-084 on the console): VERIFY it through the DD, once,
** and open again. A failed open never leads to the load: on a cluster
** with REUSE that would reset it.
*/
static int
open_update(OFTP_SPOOL *sp, char *why, int whylen)
{
    int rc, orc, vrc;

    if ((orc = vsopen(sp->ddname, VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD,
                      &sp->vs)) != 0) {
        vrc = idcams(" VERIFY FILE(%s)", sp->ddname);
        if (vrc != 0) {
            snprintf(why, (size_t)whylen,
                     "open rc %d, VERIFY condition code %d", orc, vrc);
            return -1;
        }
        if ((rc = vsopen(sp->ddname, VSTYPE_KSDS, VSACCESS_DYNAM,
                         VSMODE_UPD, &sp->vs)) != 0) {
            snprintf(why, (size_t)whylen,
                     "open rc %d, again rc %d after VERIFY", orc, rc);
            return -1;
        }
        sp->verified = orc;
    }
    rc = read_key(sp, ctlkey);
    if (rc == 0)
        return 0;
    /* A cluster never loaded opens for update, but in load mode:
    ** POINT and a direct PUT then both fail with reason 116 (X'74',
    ** measured on MVS). Load it sequentially below instead.
    */
    if (sp->lastrsn == 116) {
        vsclose(sp->vs);
        goto load;
    }
    /* loaded once, control record missing: put it back */
    memset(sp->buf, 0, OFTP_SPLRECL);
    memcpy(sp->buf + 8, "0000000", 7);
    if (vswrite(sp->vs, sp->buf, OFTP_SPLRECL, (void *)ctlkey, 8) == 0)
        return 0;
    snprintf(why, (size_t)whylen,
             "control record: lookup rc %d reason %d, insert rc %d reason %d",
             sp->lastrc, sp->lastrsn, sp->vs->rc, sp->vs->rsn);
    vsclose(sp->vs);
    return -1;
load:
    /* never loaded: load the control record, then reopen */
    if (vsopen(sp->ddname, VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_OUT, &sp->vs) != 0) {
        snprintf(why, (size_t)whylen, "DD %s cannot be opened", sp->ddname);
        return -1;
    }
    memset(sp->buf, 0, OFTP_SPLRECL);
    memcpy(sp->buf + 8, "0000000", 7);
    rc = vswrite(sp->vs, sp->buf, OFTP_SPLRECL, NULL, 0);
    vsclose(sp->vs);
    if (rc != 0) {
        snprintf(why, (size_t)whylen, "initial load failed, rc %d", rc);
        return -1;
    }
    if (vsopen(sp->ddname, VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, &sp->vs) != 0) {
        snprintf(why, (size_t)whylen, "reopen after initial load failed");
        return -1;
    }
    return 0;
}

int
oftp_spl_verified(const OFTP_SPOOL *sp)
{
    return sp->verified;
}

OFTP_SPOOL *
oftp_spl_open(const char *ddname, char *why, int whylen)
{
    OFTP_SPOOL *sp;

    if ((sp = calloc(1, sizeof(*sp))) == NULL) {
        snprintf(why, (size_t)whylen, "no storage");
        return NULL;
    }
    memcpy(sp->eye, "OFTPSPL ", 8);
    snprintf(sp->ddname, sizeof(sp->ddname), "%s", ddname);
    if (open_update(sp, why, whylen) != 0) {
        free(sp);
        return NULL;
    }
    sp->task = cthread_create_ex(spool_thread, sp, NULL, 32 * 1024);
    if (sp->task == NULL) {
        vsclose(sp->vs);
        free(sp);
        snprintf(why, (size_t)whylen, "spool thread not started");
        return NULL;
    }
    return sp;
}

void
oftp_spl_close(OFTP_SPOOL *sp)
{
    if (sp == NULL)
        return;
    request(sp, OP_STOP, NULL, NULL);
    ecb_wait(&sp->task->termecb);
    cthread_delete(&sp->task);
    /* here, in the task oftp_spl_open() ran in, not in the spool thread */
    vsclose(sp->vs);
    sp->vs = NULL;
    free(sp);
}

/* unlock() is a DEQ RET=HAVE: on a lock held by another task, or by
** none, it only returns a code */
void
oftp_spl_release(OFTP_SPOOL *sp)
{
    if (sp != NULL)
        unlock(sp, 0);
}

int
oftp_spl_add(OFTP_SPOOL *sp, OFTP_SPREC *rec)
{
    return request(sp, OP_ADD, NULL, rec);
}

int
oftp_spl_get(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
{
    return request(sp, OP_GET, key, rec);
}

int
oftp_spl_put(OFTP_SPOOL *sp, const OFTP_SPREC *rec)
{
    return request(sp, OP_PUT, rec->key, (OFTP_SPREC *)rec);
}

int
oftp_spl_putif(OFTP_SPOOL *sp, const OFTP_SPREC *rec, char was)
{
    SPL_REQ r;

    memset(&r, 0, sizeof(r));
    r.op = OP_PUTIF;
    memcpy(r.key, rec->key, 8);
    r.rec = (OFTP_SPREC *)rec;
    r.was = was;
    return enqueue(sp, &r);
}

int
oftp_spl_del(OFTP_SPOOL *sp, const char *key)
{
    return request(sp, OP_DEL, key, NULL);
}

int
oftp_spl_next(OFTP_SPOOL *sp, const char *key, OFTP_SPREC *rec)
{
    return request(sp, OP_NEXT, key, rec);
}

int
oftp_spl_purge(OFTP_SPOOL *sp, long before, unsigned *n)
{
    SPL_REQ r;
    OFTP_SPREC rec;

    memset(&r, 0, sizeof(r));
    r.op = OP_PURGE;
    r.before = before;
    r.rec = &rec;                       /* the walk's work record */
    enqueue(sp, &r);
    *n = r.count;
    return r.rc;
}
