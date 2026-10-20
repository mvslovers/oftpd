#ifndef OFTPSUB_H
#define OFTPSUB_H

/* oftpsub.h - the job after receipt: ?symbols in a JCL record
** (src/oftp#sub.c, portable; internals/receive-rules.md section 4)
**
** A rule with JOB=pds(member) has the member read record by record and
** every ?symbol replaced before the record goes to the internal reader:
**
**   ?PFILE  the data set the file was received as
**   ?VFILE  the virtual file name
**   ?ID     the partner
**   ?KEY    the spool key
**
** The names are upper case and nothing else is a symbol: a ? followed by
** another name, or by nothing, stays as it is. A JCL statement ("//") has
** 72 columns of text and the rest is sequence numbers, any other record
** has 80. A record that comes out longer is not cut: the job is not
** submitted (OFTP_SUB_LONG). The virtual file name is the partner's text
** and ends up in JCL, so ?VFILE stands only for letters, digits and . - _
** @ # $ (OFTP_SUB_UNSAFE otherwise); the other values are made by OFTPD.
*/

#define OFTP_SUB_CARD   80              /* the card, without the NUL     */

#define OFTP_SUB_LONG   (-1)            /* grows past its columns        */
#define OFTP_SUB_UNSAFE (-2)            /* ?VFILE with a character JCL
                                        ** must not get from a partner  */

typedef struct oftp_subctx {
    const char *pfile;
    const char *vfile;
    const char *partner;
    const char *key;
} OFTP_SUBCTX;

/* One record of the member (text, no newline, at most 80 columns of it)
** to the card[OFTP_SUB_CARD + 1], blank padded, NUL terminated. 0, or one
** of the codes above; card is not defined then. A record without a symbol
** goes through as it is, sequence numbers included.
*/
extern int  oftp_sub_card(const char *rec, const OFTP_SUBCTX *x, char *card)
                                                        asm("OFTPSUBC");

#endif /* OFTPSUB_H */
