#ifndef OFTPDATA_H
#define OFTPDATA_H

/* oftpdata.h - the DATA command's subrecords (internals/protocol.md 4.1)
**
**   'D' | HDR | subrecord | HDR | subrecord | ...
**   HDR X'80' end of record, X'40' compressed (one octet, repeated),
**       X'3F' count 0..63
**
** User data passes through untranslated: whether a file is translated is
** the caller's decision, per file.
*/

#define OFTP_SR_EOR     0x80
#define OFTP_SR_CMP     0x40
#define OFTP_SR_MAX     63

/* Encoder: fills one exchange buffer. */
typedef struct oftp_denc {
    unsigned char *buf;
    unsigned    cap;            /* the negotiated SDEB                   */
    unsigned    len;            /* octets used, the 'D' included         */
    int         cmpr;           /* buffer compression negotiated         */
} OFTP_DENC;

/* Starts a buffer of cap octets (cap >= 3); writes the 'D' */
extern void oftp_denc_init(OFTP_DENC *e, unsigned char *buf, unsigned cap,
                           int cmpr)                    asm("OFTPDEIN");

/* Adds n octets of a record. eor: they end the record (n may be 0).
** Returns the octets taken, which is less than n when the buffer is full;
** *done is set when all n octets and the end of record are in. Send the
** buffer, start the next, and call again with the rest.
*/
extern unsigned oftp_denc_put(OFTP_DENC *e, const unsigned char *data,
                              unsigned n, int eor, int *done)
                                                        asm("OFTPDEPU");

/* Decoder: walks the subrecords of one received buffer. */
typedef struct oftp_ddec {
    const unsigned char *buf;
    unsigned    len;
    unsigned    pos;
} OFTP_DDEC;

typedef struct oftp_dsub {
    const unsigned char *data;  /* NULL for a compressed subrecord       */
    unsigned    n;              /* octets of user data it stands for     */
    unsigned char rep;          /* the repeated octet, if compressed     */
    int         eor;
} OFTP_DSUB;

/* OFTP_ECMD if buf is not a DATA command */
extern int  oftp_ddec_init(OFTP_DDEC *d, const unsigned char *buf,
                           unsigned len)                asm("OFTPDDIN");

/* 1: s holds the next subrecord; 0: buffer done; OFTP_ELEN: a subrecord
** runs past the buffer; OFTP_EFIELD: compressed while cmpr is 0.
*/
extern int  oftp_ddec_next(OFTP_DDEC *d, int cmpr, OFTP_DSUB *s)
                                                        asm("OFTPDDNX");

#endif /* OFTPDATA_H */
