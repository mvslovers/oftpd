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
** is set for the one that holds the partner's text.
*/
static const char *
symbol(const char *p, const OFTP_SUBCTX *x, size_t *len, int *theirs)
{
    *theirs = 0;
    if (strncmp(p, "?PFILE", 6) == 0) {
        *len = 6;
        return nz(x->pfile);
    }
    if (strncmp(p, "?VFILE", 6) == 0) {
        *len = 6;
        *theirs = 1;
        return nz(x->vfile);
    }
    if (strncmp(p, "?ID", 3) == 0) {
        *len = 3;
        return nz(x->partner);
    }
    if (strncmp(p, "?KEY", 4) == 0) {
        *len = 4;
        return nz(x->key);
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
    char out[OFTP_SUB_CARD + 1];

    if (n > OFTP_SUB_CARD)
        n = OFTP_SUB_CARD;
    width = rec[0] == '/' && rec[1] == '/' ? 72 : OFTP_SUB_CARD;
    if (n > width)
        n = width;
    while (n > 0 && rec[n - 1] == ' ')
        n--;
    for (i = 0; i < n;) {
        size_t len = 1, vl;
        const char *v = rec + i;
        int theirs = 0;

        if (rec[i] == '?' && (v = symbol(rec + i, x, &len, &theirs)) != NULL) {
            if (theirs && !safe(v))
                return OFTP_SUB_UNSAFE;
            subst++;
            vl = strlen(v);
        } else {
            v = rec + i;
            len = vl = 1;
        }
        if (at + vl > width)
            return OFTP_SUB_LONG;
        memcpy(out + at, v, vl);
        at += vl;
        i += len;
    }
    if (subst == 0) {
        /* as it came, columns 73-80 of a statement included */
        n = strlen(rec);
        if (n > OFTP_SUB_CARD)
            n = OFTP_SUB_CARD;
        memcpy(card, rec, n);
        memset(card + n, ' ', OFTP_SUB_CARD - n);
    } else {
        memcpy(card, out, at);
        memset(card + at, ' ', OFTP_SUB_CARD - at);
    }
    card[OFTP_SUB_CARD] = '\0';
    return 0;
}
