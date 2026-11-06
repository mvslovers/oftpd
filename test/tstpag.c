/* TSTPAG - a long answer in pieces: LIST resuming by spool key (#101)
**
** The spool is a table in memory behind the call oftp_spl_next() answers,
** with a counter on it, so the test can say how many records a piece read
** as well as what it returned. The client's loop is played here too: the
** same request again with the resume word the last piece gave.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpcli.h"
#include "oftppag.h"

#define MAXREC  300

static OFTP_SPREC tab[MAXREC];          /* in key order                 */
static int        ntab;
static unsigned   reads;                /* calls of next() since reset  */

static void
add(unsigned no, const char *partner, char state, const char *owner)
{
    OFTP_SPREC *r;
    int i = ntab++;

    while (i > 0 && atoi(tab[i - 1].key + 1) > (int)no) {
        tab[i] = tab[i - 1];
        i--;
    }
    r = &tab[i];
    memset(r, 0, sizeof(*r));
    snprintf(r->key, 9, "T%07u", no);
    r->dir = SPL_SEND;
    r->state = state;
    snprintf(r->partner, sizeof(r->partner), "%s", partner);
    snprintf(r->owner, sizeof(r->owner), "%s", owner);
    snprintf(r->dsn, sizeof(r->dsn), "FILE.%u", no);
    snprintf(r->local, sizeof(r->local), "TEST.DATA.N%u", no);
}

static void
drop(unsigned no)
{
    char key[9];
    int i;

    snprintf(key, sizeof(key), "T%07u", no);
    for (i = 0; i < ntab; i++)
        if (memcmp(tab[i].key, key, 8) == 0) {
            memmove(&tab[i], &tab[i + 1],
                    (size_t)(ntab - i - 1) * sizeof(tab[0]));
            ntab--;
            return;
        }
}

static int
spool_next(void *ctx, const char *key, OFTP_SPREC *rec)
{
    int i;

    (void)ctx;
    reads++;
    for (i = 0; i < ntab; i++)
        if (key[0] == '\0' || memcmp(tab[i].key, key, 8) > 0) {
            *rec = tab[i];
            return 0;
        }
    return -1;
}

static void
put_page(void *ctx, const char *line)
{
    oftp_page_put((OFTP_PAGE *)ctx, line);
}

typedef struct filt {
    const char *partner, *status, *user;
    int own;
} FILT;

static const FILT nofilt = { "", "", "", 0 };

/* What the client has gathered */
static char     all[40000];
static unsigned alllen;
static int      pieces;
static int      rows;
static int      emptymore;              /* MORE pieces without a row    */
static int      stuck;                  /* the resume word did not move */
static unsigned maxreads;               /* reads of the worst piece     */
static int      maxrows;                /* rows of the fullest piece    */
static unsigned totreads;

static int
count_rows(const char *buf, unsigned len)
{
    const char *p = buf;
    int n = 0;

    while (p < buf + len) {
        if (p[0] == '1')
            n++;
        p = strchr(p, '\n') + 1;
    }
    return n;
}

/* One piece, as the monitor makes it; returns the page */
static OFTP_PAGE
one_piece(const FILT *f, char *buf, unsigned cap, unsigned resume,
          unsigned done)
{
    OFTP_PAGE pg;
    OFTP_LSTREQ q;

    oftp_page_init(&pg, buf, cap, resume);
    memset(&q, 0, sizeof(q));
    q.next = spool_next;
    q.put = put_page;
    q.pctx = &pg;
    q.pg = &pg;
    q.partner = f->partner;
    q.status = f->status;
    q.user = f->user;
    q.own = f->own;
    q.done = done;
    reads = 0;
    oftp_lst_run(&q);
    return pg;
}

typedef void (*BETWEEN)(int piece);

/* The client's loop, from resume (0 at the start) until a piece ends
** without MORE; `rows` is what DONE= is made of.
*/
static int
ask_from(const FILT *f, unsigned cap, unsigned resume, int had,
         BETWEEN between)
{
    char buf[2048];
    int guard = 0;

    alllen = 0;
    pieces = emptymore = stuck = 0;
    rows = had;
    maxreads = totreads = 0;
    maxrows = 0;
    all[0] = '\0';
    for (;;) {
        OFTP_PAGE pg = one_piece(f, buf, cap, resume, (unsigned)rows);
        int got = count_rows(buf, pg.len);

        totreads += reads;
        if (reads > maxreads)
            maxreads = reads;
        memcpy(all + alllen, buf, pg.len);
        alllen += pg.len;
        all[alllen] = '\0';
        rows += got;
        if (got > maxrows)
            maxrows = got;
        pieces++;
        if (!pg.more)
            return 0;
        if (got == 0)
            emptymore++;
        if (oftp_page_resume(&pg) <= resume)
            stuck++;
        resume = oftp_page_resume(&pg);
        if (between != NULL)
            between(pieces);
        if (++guard > 500)
            return -1;
    }
}

static int
ask(const FILT *f, unsigned cap)
{
    return ask_from(f, cap, 0, 0, NULL);
}

static int
count_str(const char *s, const char *what)
{
    int n = 0;

    while ((s = strstr(s, what)) != NULL) {
        n++;
        s += strlen(what);
    }
    return n;
}

static void
fill(int n)
{
    int i;

    ntab = 0;
    /* key numbers with gaps; two partners, two states, every third
    ** transfer someone else's */
    for (i = 1; i <= n; i++)
        add((unsigned)(i * 3), i % 5 == 0 ? "PTNB" : "PTNA",
            i % 4 == 0 ? SPL_HELD : SPL_QUEUED, i % 3 == 0 ? "OTHER" : "ME");
}

static int
matching(const FILT *f)
{
    int i, n = 0;

    for (i = 0; i < ntab; i++) {
        if (f->partner[0] && strcmp(f->partner, tab[i].partner) != 0)
            continue;
        if (f->status[0] && tab[i].state != SPL_HELD)
            continue;
        if (f->own && strcmp(f->user, tab[i].owner) != 0)
            continue;
        n++;
    }
    return n;
}

static unsigned mark;                   /* last row of piece 1          */

static void
change_between(int piece)
{
    if (piece != 1)
        return;
    /* the mark's own record goes, and a new one turns up just above it */
    drop(mark);
    add(mark + 1, "PTNA", SPL_QUEUED, "ME");
}

int
main(void)
{
    static char whole[40000];
    FILT f;
    char out[300];
    const char *p;
    unsigned prev, no;
    int dup, unordered;

    printf("=== TSTPAG a long answer in pieces ===\n");

    /* 1. The pieces together are the whole answer whatever their size,
    ** and the final line comes once, last, with the total.
    */
    fill(60);
    CHECK_EQ(ask(&nofilt, 2048), 0, "60 transfers, 2048-byte area: ends");
    CHECK(pieces >= 2, "it came in pieces");
    CHECK_EQ(rows, 60, "every transfer once");
    CHECK(strstr(all, "200 60 transfers\n") != NULL, "the total is 60");
    CHECK(strcmp(all + alllen - 17, "200 60 transfers\n") == 0,
          "and it is the last line");
    CHECK_EQ(count_str(all, "200 "), 1, "one final line only");
    memcpy(whole, all, alllen + 1);

    CHECK_EQ(ask(&nofilt, 256), 0, "60 transfers, 256-byte area: ends");
    CHECK(pieces >= 15, "in many pieces");
    CHECK_EQ(rows, 60, "every transfer once");
    CHECK(strcmp(all, whole) == 0, "the same text as with the large area");
    CHECK_EQ(stuck, 0, "the resume word moved on at every piece");
    CHECK_EQ(emptymore, 0, "no piece without a row, but the last");

    /* 2. The work of a piece is the work up to its own end. */
    CHECK(maxreads <= (unsigned)maxrows + 2,
          "a piece reads its own rows and the one that did not fit");
    CHECK(totreads <= (unsigned)(60 + 2 * pieces),
          "all pieces together read the spool about once");
    fill(200);
    CHECK_EQ(ask(&nofilt, 2048), 0, "200 transfers: ends");
    CHECK_EQ(rows, 200, "every transfer once");
    CHECK(maxrows >= 20 && maxreads <= (unsigned)maxrows + 2,
          "a 2048-byte piece reads about 22 records, not 200");
    CHECK(totreads <= (unsigned)(200 + 2 * pieces),
          "the reads add up to the rows, not to pieces x rows");

    /* 3. The filters give the rows they gave. */
    fill(60);
    f = nofilt;
    f.partner = "PTNB";
    CHECK_EQ(ask(&f, 256), 0, "PARTNER= in small pieces");
    CHECK_EQ(rows, matching(&f), "the transfers of that partner");
    snprintf(out, sizeof(out), "200 %d transfers\n", matching(&f));
    CHECK(strstr(all, out) != NULL, "and the total is theirs");
    CHECK_EQ(emptymore, 0, "no empty piece");
    f = nofilt;
    f.status = "HEL";
    CHECK_EQ(ask(&f, 256), 0, "STATUS= in small pieces");
    CHECK_EQ(rows, matching(&f), "the held transfers");
    f = nofilt;
    f.own = 1;
    f.user = "ME";
    CHECK_EQ(ask(&f, 256), 0, "one's own in small pieces");
    CHECK_EQ(rows, matching(&f), "the caller's transfers only");
    f.user = "";
    CHECK_EQ(ask(&f, 256), 0, "an asker nobody knows");
    CHECK_EQ(rows, 0, "owns nothing");
    CHECK(strcmp(all, "200 0 transfers\n") == 0, "the final line alone");

    fill(120);
    snprintf(tab[ntab - 1].partner, sizeof(tab[0].partner), "LONELY");
    f = nofilt;
    f.partner = "LONELY";
    CHECK_EQ(ask(&f, 256), 0, "one match behind 119 others");
    CHECK_EQ(rows, 1, "found");
    CHECK_EQ(pieces, 1, "in one piece: rows left out make no pieces");

    /* 4. The spool changes between two pieces: the mark's record goes
    ** and another is added above it. The rest follows the mark, in key
    ** order, with nothing twice.
    */
    fill(60);
    {
        char buf[2048];
        OFTP_PAGE pg = one_piece(&nofilt, buf, 256, 0, 0);

        mark = oftp_page_resume(&pg);
        CHECK(pg.more && mark != 0, "piece 1 of a changing spool is a piece");
    }
    CHECK_EQ(ask_from(&nofilt, 256, 0, 0, change_between), 0,
             "the spool changed after piece 1: still ends");
    prev = 0;
    dup = unordered = 0;
    for (p = all; *p; p = strchr(p, '\n') + 1) {
        if (p[0] != '1')
            continue;
        no = (unsigned)atoi(p + 5);
        if (no == prev)
            dup++;
        if (no < prev)
            unordered++;
        prev = no;
    }
    CHECK_EQ(dup, 0, "no row twice");
    snprintf(out, sizeof(out), "T%07u", mark);
    CHECK_EQ(count_str(all, out), 1,
             "the row of the dropped mark was given once, in piece 1");
    CHECK_EQ(unordered, 0, "in key order");
    snprintf(out, sizeof(out), "T%07u", mark + 1);
    CHECK(strstr(all, out) != NULL, "the transfer added above the mark is in");
    snprintf(out, sizeof(out), "200 %d transfers\n", rows);
    CHECK(strstr(all, out) != NULL,
          "and the total is what was given plus what was there");

    /* 5. The counted resume of the other answers is as it was. */
    {
        char buf[2048], line[40];
        unsigned resume = 0;
        int k, got = 0, pcs = 0, more;

        do {
            OFTP_PAGE pg;

            oftp_page_init(&pg, buf, 100, resume);
            for (k = 0; k < 9; k++) {
                snprintf(line, sizeof(line), "100 row %d of nine", k);
                oftp_page_put(&pg, line);
            }
            oftp_page_put(&pg, "200 end");
            got += count_rows(buf, pg.len);
            more = pg.more;
            if (more && oftp_page_resume(&pg) <= resume)
                break;
            resume = oftp_page_resume(&pg);
            pcs++;
        } while (more && pcs < 20);
        CHECK_EQ(got, 9, "counted resume: nine rows, each once");
        CHECK(pcs >= 2, "in pieces");
    }

    /* 6. What the client adds to ask for the next piece */
    CHECK_EQ(oftp_cli_continue("LIST PARTNER=ABC", 44, out, sizeof(out)), 0,
             "the request for the next piece fits");
    CHECK(strcmp(out, "LIST PARTNER=ABC DONE=44") == 0,
          "DONE= after the request");
    CHECK_EQ(oftp_cli_continue("LIST", 1, out, 8), -1,
             "no room: refused, not cut");

    return mbt_test_summary("TSTPAG");
}
