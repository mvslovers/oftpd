#ifndef OFTPPAG_H
#define OFTPPAG_H

/* oftppag.h - a long answer in pieces (src/oftp#pag.c, portable)
**
** The reply area of a cross-memory request is small, so a long answer
** comes in pieces and OFTPD keeps nothing between them (internals/xmem.md
** section 4): the client sends the request again with the resume word the
** last piece gave. The routine only carries that word; what it means is
** the handler's. Most handlers count the 1xx lines the caller has. LIST
** resumes by the spool key of the last row it delivered, so a piece starts
** where the last one ended instead of scanning the spool again.
*/

#include "oftpspl.h"

typedef struct oftp_page {
    char     *buf;
    unsigned  cap;
    unsigned  len;
    unsigned  after;    /* the resume word the caller sent                */
    unsigned  skip;     /* counted resume: 1xx lines the caller has       */
    unsigned  sent;     /* counted resume: 1xx lines passed so far        */
    int       keyed;    /* the handler resumes by key itself              */
    unsigned  last;     /* keyed: number of the last row delivered        */
    int       more;     /* the area is full: this is a piece              */
} OFTP_PAGE;

extern void oftp_page_init(OFTP_PAGE *pg, char *buf, unsigned cap,
                           unsigned resume)             asm("OFTPPGIN");

/* One line into the area. A piece never ends inside a line: when the line
** does not fit, `more` is set and the line and every one after it is
** dropped. A counted resume drops the 1xx lines the caller already has;
** a 1xx line is a row of the answer, and the single 2xx/4xx/5xx line
** that ends one is its status, belongs to every piece that ends, and is
** never counted.
*/
extern void oftp_page_put(OFTP_PAGE *pg, const char *line)
                                                        asm("OFTPPGPT");

/* The resume word for the next piece, when `more` is set */
extern unsigned oftp_page_resume(const OFTP_PAGE *pg)   asm("OFTPPGRS");

/* The next transfer with a key above key ("" for the first of all):
** oftp_spl_next() behind a function, so that a test can stand in for the
** spool. 0, or not 0 at the end.
*/
typedef int (*OFTP_LSTNEXT)(void *ctx, const char *key, OFTP_SPREC *rec);
typedef void (*OFTP_LSTPUT)(void *ctx, const char *line);

typedef struct oftp_lstreq {
    OFTP_LSTNEXT next;
    void        *nctx;
    OFTP_LSTPUT  put;
    void        *pctx;
    OFTP_PAGE   *pg;        /* NULL: the whole answer in one go           */
    const char  *partner;   /* "" any                                     */
    const char  *status;    /* "" any, else a prefix of the state's name  */
    const char  *user;      /* the asker, for `own`                       */
    int          own;       /* only the asker's transfers                 */
    unsigned     done;      /* rows the caller has (DONE=), on a resume   */
} OFTP_LSTREQ;

/* The rows of LIST and its final line "200 n transfers", n counting the
** rows the caller already had. With a page, the walk starts after the key
** the resume word names and stops as soon as the area is full.
*/
extern void oftp_lst_run(const OFTP_LSTREQ *q)          asm("OFTPLSRN");

#endif /* OFTPPAG_H */
