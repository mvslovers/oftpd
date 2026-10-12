#ifndef OFTPCFG_H
#define OFTPCFG_H

/* oftpcfg.h - the monitor's configuration and partner table
** (src/oftp#cfg.c, portable)
**
** Both members are KEYWORD=VALUE pairs separated by blanks or line ends;
** '*' in column 1 starts a comment line. In the partner member, PARTNER=
** opens a new entry and the keywords after it belong to that entry.
**
**   OFTPPRM:  ODETTEID= PORT= CONTROLPORT= INPREFIX= TMPPREFIX= MAXSESS=
**             SDEB= CREDIT= TIMEOUT= TLRECL= UBLKSIZE= UNIT= VOLSER=
**             RETRY=
**   OFTPPTN:  PARTNER= ODETTEID= PASSWORD= OURPASSWORD= OURID= HOST=
**             PORT= LEVEL= SDEB= CREDIT= MODE= COMPRESS= ENABLED= CALL=
**             ADDRESS= XLATE= VIA=
**
** VIA=name makes a partner one that is reached through another: files for
** it go in sessions with name, addressed to its Odette ID (SFIDDEST). It
** has no HOST, PORT or passwords of its own and is never called nor
** accepted as a caller itself. One level only: name must be a direct
** partner.
**
** Values are upper-cased except HOST. A wrong keyword or value is
** reported with its line number and, in the partner member, disables
** that partner only.
*/

#include "oftpsess.h"

#define OFTP_MAXPTN     32              /* partners in the table         */

typedef struct oftp_partner {
    OFTP_PEER   peer;                   /* what the session engine needs */
    char        host[64];
    int         port;
    int         enabled;                /* ENABLED=Y and no error        */
    int         call;                   /* CALL=Y: we may call it        */
    char        address[16];            /* calls must come from here     */
    int         xlate;                  /* F/V/U translation default     */
    char        via[9];                 /* reached through this partner  */
    int         line;                   /* where the entry starts        */
} OFTP_PARTNER;

typedef struct oftp_config {
    char        odetteid[OFTP_IDLEN + 1];
    int         port;
    int         ctlport;
    char        inprefix[36];
    char        tmpprefix[36];
    int         maxsess;
    unsigned    sdeb;
    unsigned    cred;
    int         timeout;
    unsigned    tlrecl;
    unsigned    ublksize;
    char        unit[9];
    char        volser[7];
    int         retry;                  /* seconds before a retry        */

    OFTP_PARTNER ptn[OFTP_MAXPTN];
    int         nptn;
    int         errors;                 /* lines refused                 */
} OFTP_CONFIG;

/* Messages go through this, one line each */
typedef void (*OFTP_CFGMSG)(void *ctx, const char *line);

extern void oftp_cfg_defaults(OFTP_CONFIG *c)           asm("OFTPCFGD");

/* One line of OFTPPRM (partner 0) or OFTPPTN (partner 1). lineno for
** messages. Returns the number of errors in the line.
*/
extern int  oftp_cfg_line(OFTP_CONFIG *c, int partner, const char *text,
                          int lineno, OFTP_CFGMSG msg, void *ctx)
                                                        asm("OFTPCFGL");

/* After the last line: checks each partner, disables the incomplete
** ones, fills its peer from the defaults. Returns the partners enabled.
*/
extern int  oftp_cfg_finish(OFTP_CONFIG *c, OFTP_CFGMSG msg, void *ctx)
                                                        asm("OFTPCFGF");

/* The enabled partner with this name or Odette ID, or NULL. byid finds
** direct partners only: a partner behind another never calls us itself.
*/
extern OFTP_PARTNER *oftp_cfg_find(OFTP_CONFIG *c, const char *name)
                                                        asm("OFTPCFGN");
extern OFTP_PARTNER *oftp_cfg_byid(OFTP_CONFIG *c, const char *odetteid)
                                                        asm("OFTPCFGI");

#endif /* OFTPCFG_H */
