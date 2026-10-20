/* oftp#sub.c - the job after receipt: ?symbols in a JCL record */
#include <ctype.h>
#include <string.h>

#include "oftpsub.h"

static const char *
nz(const char *s)
{
    return s == NULL ? "" : s;
}

/* The symbol at p (which points at the '?'): its value, or NULL when it
** is none. *len gets the length of its name including the '?', *theirs
** is set for the one that holds the partner's text. A name ends at a
** character that is no letter or digit: ?IDX is another name.
*/
static const char *
symbol(const char *p, const OFTP_SUBCTX *x, size_t *len, int *theirs)
{
    static const struct {
        const char *name;
        size_t      len;
    } names[] = {
        { "?PFILE", 6 }, { "?VFILE", 6 }, { "?ID", 3 }, { "?KEY", 4 }
    };
    const char *value[4];
    int i;

    value[0] = nz(x->pfile);
    value[1] = nz(x->vfile);
    value[2] = nz(x->partner);
    value[3] = nz(x->key);
    *theirs = 0;
    for (i = 0; i < 4; i++) {
        if (strncmp(p, names[i].name, names[i].len) == 0
            && !isalnum((unsigned char)p[names[i].len])) {
            *len = names[i].len;
            *theirs = i == 1;
            return value[i];
        }
    }
    return NULL;
}

static int
safe(const char *v)
{
    for (; *v; v++)
        if (!isalnum((unsigned char)*v) && *v != '.' && *v != '-'
            && *v != '_' && *v != '@' && *v != '#' && *v != '$')
            return 0;
    return 1;
}

int
oftp_sub_card(const char *rec, const OFTP_SUBCTX *x, char *card)
{
    size_t n = strlen(rec), width, at = 0, i;
    int subst = 0;
    char head[OFTP_SUB_CARD + 1], out[OFTP_SUB_CARD + 1];

    if (n > OFTP_SUB_CARD)
        n = OFTP_SUB_CARD;
    /* a JCL statement has its text in columns 1-71; 72 says whether it is
    ** continued and 73-80 are the sequence number: they stay where they
    ** are and only the text grows */
    width = rec[0] == '/' && rec[1] == '/' ? OFTP_SUB_TEXT : OFTP_SUB_CARD;
    memcpy(head, rec, n);
    head[n] = '\0';
    if (n > width)
        head[width] = '\0';
    n = strlen(head);
    while (n > 0 && head[n - 1] == ' ')
        n--;
    for (i = 0; i < n;) {
        size_t len = 1, vl;
        const char *v = head + i;
        int theirs = 0;

        if (head[i] == '?' && (v = symbol(head + i, x, &len, &theirs)) != NULL) {
            if (theirs && !safe(v))
                return OFTP_SUB_UNSAFE;
            subst++;
            vl = strlen(v);
        } else {
            v = head + i;
            len = vl = 1;
        }
        if (at + vl > width)
            return OFTP_SUB_LONG;
        memcpy(out + at, v, vl);
        at += vl;
        i += len;
    }
    n = strlen(rec);
    if (n > OFTP_SUB_CARD)
        n = OFTP_SUB_CARD;
    if (subst == 0) {
        memcpy(card, rec, n);
        memset(card + n, ' ', OFTP_SUB_CARD - n);
    } else {
        memset(card, ' ', OFTP_SUB_CARD);
        memcpy(card, out, at);
        if (n > width)
            memcpy(card + width, rec + width, n - width);
    }
    card[OFTP_SUB_CARD] = '\0';
    return 0;
}

size_t
oftp_sub_bufsize(int recfm, long lrecl)
{
    if (recfm < 0 || recfm > 1 || lrecl <= 0)
        return 0;
    return (size_t)lrecl + 4;
}
