/* oftp#pag.c - a long answer in pieces, and the rows of LIST (portable;
** oftppag.h)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftpopr.h"
#include "oftppag.h"

void
oftp_page_init(OFTP_PAGE *pg, char *buf, unsigned cap, unsigned resume)
{
    memset(pg, 0, sizeof(*pg));
    pg->buf   = buf;
    pg->cap   = cap;
    pg->after = resume;
    pg->skip  = resume;
}

void
oftp_page_put(OFTP_PAGE *pg, const char *line)
{
    unsigned n;

    if (line == NULL)
        return;
    if (!pg->keyed && line[0] == '1' && pg->sent < pg->skip) {
        pg->sent++;                     /* the caller has this row    */
        return;
    }
    if (pg->more)
        return;
    n = (unsigned)strlen(line);
    if (pg->len + n + 1 > pg->cap) {
        pg->more = 1;
        return;
    }
    memcpy(pg->buf + pg->len, line, n);
    pg->len += n;
    pg->buf[pg->len++] = '\n';
    if (line[0] == '1')
        pg->sent++;
}

unsigned
oftp_page_resume(const OFTP_PAGE *pg)
{
    return pg->keyed ? pg->last : pg->sent;
}

/* Keys are 'T' and seven digits (oftpspl.h), so the number fits the
** resume word with room to spare.
*/
#define LST_MAXNO   9999999U

void
oftp_lst_run(const OFTP_LSTREQ *q)
{
    OFTP_PAGE *pg = q->pg;
    OFTP_SPREC r;
    char last[8], out[200], sd[OFTP_DSNLEN + 1];
    unsigned n = 0;

    last[0] = '\0';
    if (pg != NULL) {
        pg->keyed = 1;
        if (pg->after != 0) {
            /* the record behind the mark may be gone by now: the walk
            ** goes to the next one above it all the same
            */
            snprintf(out, sizeof(out), "T%07u",
                     pg->after > LST_MAXNO ? LST_MAXNO : pg->after);
            memcpy(last, out, 8);
            n = q->done;
        }
    }
    while (q->next(q->nctx, last, &r) == 0) {
        memcpy(last, r.key, 8);
        if (q->partner[0] && strcmp(q->partner, r.partner) != 0
            && strcmp(q->partner, r.target) != 0)
            continue;
        if (q->status[0] && strncmp(q->status, oftp_spl_statename(r.state),
                                    strlen(q->status)) != 0)
            continue;
        if (q->own && !(q->user[0] && strcmp(q->user, r.owner) == 0))
            continue;
        snprintf(out, sizeof(out), "100 %.8s %-4s %-9s %-8s %-26s %s",
                 r.key, r.dir == SPL_SEND ? "SEND" : "RECV",
                 oftp_spl_statename(r.state),
                 r.target[0] ? r.target : r.partner,
                 oftp_printable(sd, sizeof(sd), r.dsn), r.local);
        q->put(q->pctx, out);
        if (pg != NULL) {
            /* the row that did not fit is the next piece's first: the
            ** mark stays where the last delivered row was
            */
            if (pg->more)
                return;
            pg->last = (unsigned)strtoul(r.key + 1, NULL, 10);
        }
        n++;
    }
    snprintf(out, sizeof(out), "200 %u transfers", n);
    q->put(q->pctx, out);
}
