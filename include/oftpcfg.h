#ifndef OFTPCFG_H
#define OFTPCFG_H

/* oftpcfg.h - the monitor's configuration and partner table
** (src/oftp#cfg.c, portable)
**
** Both members are KEYWORD=VALUE pairs separated by blanks or line ends;
** '*' in column 1 starts a comment line. In the partner member, PARTNER=
** opens a new entry and the keywords after it belong to that entry.
**
**   OFTPPRM:  ODETTEID= PORT= CONTROLPORT= SVC= INPREFIX= TMPPREFIX=
**             MAXSESS= SDEB= CREDIT= TIMEOUT= TLRECL= UBLKSIZE= UNIT=
**             VOLSER= RETRY= MAXRETRY= RELAY= ADMIN= KEEPDAYS=
**   OFTPPTN:  PARTNER= ODETTEID= PASSWORD= OURPASSWORD= OURID= HOST=
**             PORT= LEVEL= SDEB= CREDIT= MODE= COMPRESS= ENABLED= CALL=
**             ADDRESS= XLATE= VIA= SESSIONS=
**
** SESSIONS=n (1-16, default 1) is how many sessions with the partner may
** run at once, under MAXSESS. A call from a partner at its limit ends
** with ESID 08; while work no session holds is waiting, the monitor
** opens another session to it, up to the limit (internals/design.md 6.1).
**
** ADMIN=id names a userid that may see and change every transfer, and
** CALL and REFRESH, over the cross-memory transport; one ADMIN= per id,
** up to OFTP_MAXADM. Anyone else sees and changes only the transfers
** they queued. The TCP control port knows no caller and is not affected
** (internals/xmem.md section 7).
**
** KEEPDAYS=n (0-9999, default 7) is how long a finished transfer --
** delivered, confirmed, rejected or failed -- stays in the spool after its
** last change; the monitor deletes older ones once a day, and PURGE does
** it on request. 0 keeps them for ever. Data sets are never deleted.
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
#define OFTP_MAXSESS    16              /* sessions at once (MAXSESS)    */
#define OFTP_MAXADM     8               /* userids in ADMIN=             */

typedef struct oftp_partner {
    OFTP_PEER   peer;                   /* what the session engine needs */
    char        host[64];
    int         port;
    int         enabled;                /* ENABLED=Y and no error        */
    int         call;                   /* CALL=Y: we may call it        */
    int         sessions;               /* SESSIONS=: at once, at most   */
    char        address[16];            /* calls must come from here     */
    int         xlate;                  /* F/V/U translation default     */
    char        via[9];                 /* reached through this partner  */
    int         line;                   /* where the entry starts        */
} OFTP_PARTNER;

typedef struct oftp_config {
    char        odetteid[OFTP_IDLEN + 1];
    int         port;
    int         ctlport;
    /* SVC=nnn: the installation SVC the cross-memory transport reserves
    ** while OFTPD runs, 200-255. 0 means no transport, and that is the
    ** default on purpose -- two products each picking "a free one" at
    ** start take the same slot on the same stand. The number comes from
    ** the registry in the ecosystem CLAUDE.md (OFTPD's is 237), never
    ** from a scan. internals/xmem.md section 3.
    */
    int         svc;
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
    int         maxretry;               /* attempts per send, 0 no limit */
    int         keepdays;               /* finished transfers, 0 for ever */
    int         relay;                  /* RELAY=Y: act as a node        */
    char        admin[OFTP_MAXADM][9];  /* ADMIN=: may do everything     */
    int         nadmin;

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

/* Where a file for odetteid goes next: the enabled partner with that ID
** if it is direct, else the one it is reached through; NULL if unknown.
** *final gets the partner with the ID itself.
*/
extern OFTP_PARTNER *oftp_cfg_route(OFTP_CONFIG *c, const char *odetteid,
                                    OFTP_PARTNER **final) asm("OFTPCFGR");

/* After a failed attempt to send a file -- refused with retry Y, cut off
** in the session, or the partner not reached -- with attempts the
** attempts made so far, the first one included: 0 and *tnext the time of
** the next one, now + RETRY times attempts (progressive, at most a day);
** or 1 when MAXRETRY is reached and the file is not retried by itself.
*/
extern int  oftp_cfg_retry(const OFTP_CONFIG *c, unsigned attempts, long now,
                           long *tnext)                 asm("OFTPCFGT");

/* 1 when user is named in ADMIN=, 0 otherwise and for "" */
extern int  oftp_cfg_admin(const OFTP_CONFIG *c, const char *user)
                                                        asm("OFTPCFGA");

#endif /* OFTPCFG_H */
