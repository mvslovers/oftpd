#ifndef OFTPDSF_H
#define OFTPDSF_H

/* oftpdsf.h - data sets as OFTP virtual files (src/mvs/oftp#dsf.c, MVS)
**
** Sending: a sequential data set or member is read record by record (F,
** V) or as a stream (U: its blocks; T: its records as ASCII lines with
** CR-LF). Receiving: a new data set is allocated with attributes derived
** from the SFID and written the same way back. internals/design.md 5.
**
** Return values are SFNA/EFNA reason codes (0 = fine), so a caller hands
** them to the session engine as they are.
*/

#include "oftpwire.h"

typedef struct oftp_dsf OFTP_DSF;

/* Translation of record data for F, V and U files. T files are always
** translated; their CR-LF are added and removed here.
*/
#define OFTP_XLATE_NONE 0
#define OFTP_XLATE_TEXT 1               /* CP037 <-> ISO 8859-1          */

/* How a received file is allocated */
typedef struct oftp_dsopt {
    unsigned    tlrecl;                 /* T files: LRECL (VB), e.g. 255 */
    unsigned    ublksize;               /* U files: BLKSIZE, e.g. 6144   */
    char        unit[9];                /* "" lets the system choose     */
    char        volser[7];
    unsigned    space;                  /* tracks when the SFID has no
                                        ** size, 0: the default          */
} OFTP_DSOPT;

/* Opens dsn ("A.B.C" or "A.B(MEM)") for sending. fmt 0 takes the format
** from the RECFM (F, V or U); 'T' sends text. Fills sfid->fmt, lrecl and
** fsiz: the tracks used at the volume's track length in 1K blocks, an
** estimate on the high side; 0 for a member or when the VTOC cannot tell.
*/
extern int  oftp_dsf_open_out(const char *dsn, char fmt, int xlate,
                              OFTP_SFIDCMD *sfid, OFTP_DSF **f)
                                                        asm("OFTPDSOO");

/* The session engine's read callback, see oftpsess.h */
extern int  oftp_dsf_read(OFTP_DSF *f, unsigned char *b, unsigned cap,
                          unsigned *n, int *last)       asm("OFTPDSRD");

/* Allocates dsn (which must not exist) for the file the SFID describes
** and opens it. Reasons: 04 format, 05 record length, 12 allocation,
** 13 the data set exists.
*/
extern int  oftp_dsf_open_in(const char *dsn, const OFTP_SFIDCMD *sfid,
                             int xlate, const OFTP_DSOPT *opt,
                             OFTP_DSF **f)              asm("OFTPDSOI");

/* The session engine's write callback. Reasons: 05 a record or line
** longer than the data set takes, 12 a write failed.
*/
extern int  oftp_dsf_write(OFTP_DSF *f, const unsigned char *b, unsigned n,
                           int eor)                     asm("OFTPDSWR");

/* Closes and frees. For a received file keep 0 deletes the data set.
** 12 when the last block could not be written (a full volume shows here
** and nowhere else); the data set is then deleted as well.
*/
extern int  oftp_dsf_close(OFTP_DSF *f, int keep)       asm("OFTPDSCL");

/* Counts, for the log and the spool */
extern unsigned long oftp_dsf_records(const OFTP_DSF *f) asm("OFTPDSRC");

#endif /* OFTPDSF_H */
