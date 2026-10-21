/* oftp#dsn.c - data set names from virtual file names
**
** Character classes are spelled as ranges that hold in EBCDIC (where the
** alphabet has gaps after I and R) as well as in ASCII.
*/
#include <string.h>

#include "oftpdsn.h"

static int
letter(char c)
{
    return (c >= 'A' && c <= 'I') || (c >= 'J' && c <= 'R')
           || (c >= 'S' && c <= 'Z');
}

static int
national(char c)
{
    return c == '@' || c == '#' || c == '$';
}

static int
digit(char c)
{
    return c >= '0' && c <= '9';
}

static char
upper(char c)
{
    if (c >= 'a' && c <= 'i')
        return (char)(c - 'a' + 'A');
    if (c >= 'j' && c <= 'r')
        return (char)(c - 'j' + 'J');
    if (c >= 's' && c <= 'z')
        return (char)(c - 's' + 'S');
    return c;
}

int
oftp_dsn_valid(const char *d)
{
    int q = 0, len = 0;

    for (; *d; d++, len++) {
        char c = *d;

        if (c == '.') {
            if (q == 0)
                return 0;
            q = 0;
            continue;
        }
        if (q == 0 && !letter(c) && !national(c))
            return 0;
        if (!letter(c) && !national(c) && !digit(c) && c != '-')
            return 0;
        if (++q > 8)
            return 0;
    }
    return q > 0 && len <= 44;
}

int
oftp_dsn_map(const char *prefix, const char *vfn, const char *suffix,
             char *out)
{
    char qual[16][9];
    int nq = 0, i;
    size_t room, need;
    const char *p = vfn;

    /* the qualifiers of the virtual file name, made valid */
    while (*p && nq < 16) {
        char q[9];
        int n = 0;

        for (; *p && *p != '.'; p++) {
            char c = upper(*p);

            if ((letter(c) || national(c) || digit(c) || c == '-') && n < 8)
                q[n++] = c;
        }
        if (*p == '.')
            p++;
        if (n == 0)
            continue;
        q[n] = '\0';
        if (!letter(q[0]) && !national(q[0])) {
            memmove(q + 1, q, (size_t)(n < 7 ? n + 1 : 8));
            q[0] = 'X';
            q[8] = '\0';
        }
        strcpy(qual[nq++], q);
    }
    if (nq == 0) {
        strcpy(qual[0], "DATA");
        nq = 1;
    }

    /* drop qualifiers from the end until it fits */
    room = 44 - strlen(prefix) - (suffix[0] ? strlen(suffix) + 1 : 0);
    if (strlen(prefix) + (suffix[0] ? strlen(suffix) + 1 : 0) > 44)
        return -1;
    for (;;) {
        need = 0;
        for (i = 0; i < nq; i++)
            need += strlen(qual[i]) + 1;        /* the dot before each */
        if (need <= room || nq == 1)
            break;
        nq--;
    }
    if (need > room)
        return -1;

    strcpy(out, prefix);
    for (i = 0; i < nq; i++) {
        strcat(out, ".");
        strcat(out, qual[i]);
    }
    if (suffix[0]) {
        strcat(out, ".");
        strcat(out, suffix);
    }
    return oftp_dsn_valid(out) ? 0 : -1;
}

void
oftp_dsn_member(const char *dsnm, const char *elnm, char *out)
{
    size_t n = 0, m = 0;

    while (n < 44 && dsnm[n] != ' ' && dsnm[n] != '\0')
        n++;
    while (m < 8 && elnm[m] != ' ' && elnm[m] != '\0')
        m++;
    memcpy(out, dsnm, n);
    if (n > 0 && m > 0) {
        out[n++] = '(';
        memcpy(out + n, elnm, m);
        n += m;
        out[n++] = ')';
    }
    out[n] = '\0';
}
