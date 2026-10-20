#ifndef OFTPRCV_H
#define OFTPRCV_H

/* oftprcv.h - receive rules (src/oftp#rcv.c, portable)
**
** Member OFTPRCV0 behind DD OFTPRCV. Same syntax as the partner member:
** KEYWORD=VALUE pairs separated by blanks, commas or line ends, '*' in
** column 1 starts a comment, RULE= opens an entry and the keywords after
** it belong to it:
**
**   RULE=ORDERS  FROM=PARTNER1  VFN=ORDERS.*
**                DSN=PROD.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
**   RULE=INVOICE FROM=*  VFN=INV?????.DATA
**                DSN=ACCT.IN.%ID.%VFN  REPLACE=Y  VOLSER=WORK01  SPACE=30
**
**   RULE=     a name for the rule, 1-8 characters, unique; messages use it
**   FROM=     a partner name from OFTPPTN, or * (the default)
**   VFN=      the virtual file name: * any number of characters, ? exactly
**             one, case does not matter (the default is *)
**   DSN=      the data set name, with symbols (below); without it the file
**             gets INPREFIX.<mapped vfn> as it does with no rule at all
**   REPLACE=  Y: an existing data set is replaced once the new file has
**             been received and closed; N (default): it is refused
**   UNIT=  VOLSER=   where the data set goes (default: OFTPPRM's)
**   SPACE=    tracks, 1-9999, secondary half of it plus one; used only
**             when the SFID gives no size
**   XLATE=    Y or N, over the partner's XLATE= for this file
**
** The first rule in member order that matches the partner and the
** virtual file name decides; none matching, the file is treated as it was
** before rules existed.
**
** Symbols in DSN= (longest name wins; the rvs names are not supported):
**   %VFN the virtual file name   %Q1..%Q9 its qualifiers   %ID the partner
**   %YYYY %MM %DD %HH %MI %SS    the time of arrival
**   %CTR the 4-digit counter of the file's time field
**   %KEY the spool key, T + 7 digits
** Everything else in DSN= is literal: A-Z 0-9 @ # $ - and '.'. The name is
** checked as a data set name when the file arrives, so a date in one
** qualifier needs a year of two digits or less: %YYYY%MM%DD is eight
** characters and a letter in front of it makes nine.
**
** The first qualifier (up to the first '.') must not contain %VFN, %Q1..%Q9
** or %CTR, text the partner chooses: such a rule is an error. A DSN= that
** can name a data set under TMPPREFIX is an error too (oftp_rcv_finish).
**
** A member with any error is not used as a whole (the caller keeps the
** rules it had). internals/receive-rules.md.
*/

#include "oftpcfg.h"

#define OFTP_MAXRULE    64              /* rules in a member             */
#define OFTP_RCV_DSN    80              /* longest DSN= pattern          */

typedef struct oftp_rule {
    char        name[9];
    char        from[9];                /* partner name or "*"           */
    char        vfn[33];                /* pattern                       */
    char        dsn[OFTP_RCV_DSN + 1];  /* pattern; "" is the default    */
    int         replace;
    char        unit[9];
    char        volser[7];
    unsigned    space;                  /* tracks, 0: not given          */
    int         xlate;                  /* -1 partner's, 0 N, 1 Y        */
    int         line;                   /* where the rule starts         */
    int         bad;                    /* an error in one of its lines  */
} OFTP_RULE;

typedef struct oftp_rules {
    OFTP_RULE   r[OFTP_MAXRULE];
    int         n;
    int         errors;                 /* lines and rules refused       */
} OFTP_RULES;

/* What the symbols stand for. arrival is CCYYMMDDHHMMSS, sfidtime the
** SFID's HHMMSScccc.
*/
typedef struct oftp_rcvctx {
    const char *vfn;
    const char *partner;
    const char *key;
    const char *arrival;
    const char *sfidtime;
} OFTP_RCVCTX;

extern void oftp_rcv_clear(OFTP_RULES *s)               asm("OFTPRCVC");

/* One line of the member. Returns the number of errors in it. */
extern int  oftp_rcv_line(OFTP_RULES *s, const char *text, int lineno,
                          OFTP_CFGMSG msg, void *ctx)   asm("OFTPRCVL");

/* After the last line: checks each rule (partner known, name unique,
** pattern well formed). Returns the errors in the whole member, those of
** the lines included; 0 means the set can be used.
*/
extern int  oftp_rcv_finish(OFTP_RULES *s, const OFTP_CONFIG *c,
                            OFTP_CFGMSG msg, void *ctx) asm("OFTPRCVF");

/* The first rule that matches, or NULL */
extern const OFTP_RULE *oftp_rcv_match(const OFTP_RULES *s,
                                       const char *partner, const char *vfn)
                                                        asm("OFTPRCVM");

/* 1 when text matches pattern (* and ?, case does not matter) */
extern int  oftp_rcv_wild(const char *pattern, const char *text)
                                                        asm("OFTPRCVW");

/* The data set name the rule makes of the symbols, out[45]. 0, or -1
** when it comes out as no valid data set name (SFNA 01).
*/
extern int  oftp_rcv_name(const OFTP_RULE *r, const OFTP_RCVCTX *x,
                          char *out)                    asm("OFTPRCVN");

#endif /* OFTPRCV_H */
