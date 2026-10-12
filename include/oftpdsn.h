#ifndef OFTPDSN_H
#define OFTPDSN_H

/* oftpdsn.h - data set names from virtual file names (src/oftp#dsn.c)
**
** A virtual file name may hold anything (a PC partner sends HELLO.TXT,
** report-2026.csv or NOT-CATEGORIZED). The received data set is named
** prefix + the name made valid, qualifier by qualifier:
**
**   - letters upper-cased; A-Z 0-9 @ # $ - kept, anything else dropped;
**   - cut to 8 characters; a qualifier that does not start with a letter
**     or national character gets an X in front (then cut again);
**   - empty qualifiers left out; a name with none left is DATA;
**   - qualifiers dropped from the end until prefix + name + the optional
**     suffix fit into 44 characters.
**
**   NOT-CATEGORIZED  -> NOT-CATE        report-2026.csv -> REPORT-2.CSV
**   2026.LOG         -> X2026.LOG       a..b            -> A.B
*/

/* 1 when d is a valid data set name (no member) */
extern int  oftp_dsn_valid(const char *d)               asm("OFTPDSNV");

/* out[45]: prefix "." mapped vfn, and "." suffix when suffix is not "".
** 0, or -1 when even prefix + DATA + suffix do not fit.
*/
extern int  oftp_dsn_map(const char *prefix, const char *vfn,
                         const char *suffix, char *out) asm("OFTPDSNM");

#endif /* OFTPDSN_H */
