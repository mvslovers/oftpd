#ifndef OFTPWIRE_H
#define OFTPWIRE_H

/* oftpwire.h - OFTP commands on the wire, encoded and decoded per level.
**
** Layouts: internals/protocol.md section 2. Character fields are held
** here as local NUL-terminated strings with the trailing blanks removed;
** the codec translates to and from ISO 646 itself. 2.0 text fields
** (T(n), UTF-8) are never translated: the structs carry a pointer into
** the buffer and a length.
**
** Counts the wire carries as 9(13) or 9(17) are held in 32 bits. No
** data set on MVS 3.8j comes near 4 GB; a larger value decodes as
** OFTP_ERANGE, which a receiver answers with SFNA 06 (file too big).
**
** Encoders return the number of octets written, or a negative OFTP_E*
** value; decoders return 0 or a negative OFTP_E* value. Neither writes
** past the length it was given.
*/

/* Protocol release levels (SSIDLEV) */
#define OFTP_LEV12      1       /* 1.2; spoken as 1.3 */
#define OFTP_LEV13      2
#define OFTP_LEV14      4
#define OFTP_LEV20      5

/* Errors */
#define OFTP_ESHORT     (-1)    /* buffer too short for the command      */
#define OFTP_ECMD       (-2)    /* not the command expected              */
#define OFTP_EFIELD     (-3)    /* a field holds an invalid value        */
#define OFTP_ERANGE     (-4)    /* a count does not fit in 32 bits       */
#define OFTP_ELEVEL     (-5)    /* command or level not supported        */
#define OFTP_ELEN       (-6)    /* length does not match the command     */

/* Command codes, as local characters */
#define OFTP_SSRM       'I'
#define OFTP_SSID       'X'
#define OFTP_SFID       'H'
#define OFTP_SFPA       '2'
#define OFTP_SFNA       '3'
#define OFTP_DATA       'D'
#define OFTP_CDT        'C'
#define OFTP_EFID       'T'
#define OFTP_EFPA       '4'
#define OFTP_EFNA       '5'
#define OFTP_ESID       'F'
#define OFTP_CD         'R'
#define OFTP_EERP       'E'
#define OFTP_NERP       'N'
#define OFTP_RTR        'P'
#define OFTP_SECD       'J'
#define OFTP_AUCH       'A'
#define OFTP_AURP       'S'

/* Field sizes (without the NUL) */
#define OFTP_IDLEN      25      /* Odette ID                            */
#define OFTP_PSWLEN     8
#define OFTP_USRLEN     8
#define OFTP_DSNLEN     26      /* virtual file name                    */
#define OFTP_DATLEN     8       /* CCYYMMDD                             */
#define OFTP_TIMLEN     10      /* HHMMSScccc                           */

/* Largest command the codec builds, excluding 2.0 text tails */
#define OFTP_MAXCMD     165

typedef struct oftp_ssid {
    int         level;                  /* SSIDLEV, OFTP_LEV*            */
    char        code[OFTP_IDLEN + 1];   /* the sender's Odette ID        */
    char        pswd[OFTP_PSWLEN + 1];
    unsigned    sdeb;                   /* exchange buffer, 128..99999   */
    char        sr;                     /* 'S', 'R' or 'B'               */
    char        cmpr;                   /* 'Y' / 'N'                     */
    char        rest;                   /* 'Y' / 'N'                     */
    char        spec;                   /* 'Y' / 'N'; 'N' on TCP         */
    unsigned    cred;                   /* 1..999                        */
    char        auth;                   /* 'Y' / 'N'; 'N' below 2.0      */
    char        user[OFTP_USRLEN + 1];
} OFTP_SSIDCMD;

/* Date and time are always held in the 1.4 form. A 1.3 YYMMDD gets its
** century by window (70-99 -> 19, else 20), a 1.3 HHMMSS a counter of
** 0000; encoding at 1.3 drops both again, so a 1.3 identity round-trips.
*/
typedef struct oftp_sfid {
    char        dsn[OFTP_DSNLEN + 1];
    char        date[OFTP_DATLEN + 1];
    char        time[OFTP_TIMLEN + 1];
    char        user[OFTP_USRLEN + 1];
    char        dest[OFTP_IDLEN + 1];
    char        orig[OFTP_IDLEN + 1];
    char        fmt;                    /* 'F', 'V', 'U', 'T'            */
    unsigned    lrecl;
    unsigned long fsiz;                 /* 1K blocks                     */
    unsigned long osiz;                 /* 2.0; = fsiz below 2.0         */
    unsigned long rest;                 /* restart position              */
    int         sec;                    /* 2.0: 0 none, 1 enc, 2 sig, 3  */
    int         ciph;                   /* 2.0: cipher suite, 0 none     */
    int         comp;                   /* 2.0: 0 none, 1 zlib           */
    int         env;                    /* 2.0: 0 none, 1 CMS            */
    char        sign;                   /* 2.0: signed EERP wanted Y/N   */
    unsigned    descl;                  /* 2.0: description, UTF-8       */
    const unsigned char *desc;
} OFTP_SFIDCMD;

/* SFNA, EFNA, ESID: a reason code and, at 2.0, a reason text */
typedef struct oftp_reas {
    int         reas;
    char        retry;                  /* SFNA only: 'Y' / 'N'          */
    unsigned    textl;                  /* 2.0: UTF-8 text               */
    const unsigned char *text;
} OFTP_REASCMD;

typedef struct oftp_efid {
    unsigned long rcnt;                 /* records (F/V), 0 for U/T      */
    unsigned long ucnt;                 /* octets of user data           */
} OFTP_EFIDCMD;

/* EERP and NERP. dest is the originator of the virtual file, orig the
** final recipient that answers -- the reverse of the SFID. A NERP has no
** user field and carries a creator and a reason; an EERP neither. The
** 2.0 hash and signature are decoded as pointers and never produced.
*/
typedef struct oftp_erp {
    char        dsn[OFTP_DSNLEN + 1];
    char        date[OFTP_DATLEN + 1];
    char        time[OFTP_TIMLEN + 1];
    char        user[OFTP_USRLEN + 1];  /* EERP only                     */
    char        dest[OFTP_IDLEN + 1];
    char        orig[OFTP_IDLEN + 1];
    char        crea[OFTP_IDLEN + 1];   /* NERP only                     */
    int         reas;                   /* NERP only                     */
    unsigned    textl;                  /* NERP, 2.0                     */
    const unsigned char *text;
    unsigned    hshl;                   /* 2.0                           */
    const unsigned char *hsh;
    unsigned    sigl;                   /* 2.0                           */
    const unsigned char *sig;
} OFTP_ERPCMD;

/* The command code of a buffer, as a local character; 0 if empty */
extern char oftp_cmd_code(const unsigned char *b, unsigned len)
                                                        asm("OFTPCODE");

/* One-octet commands (CD, RTR, SECD) and the 3-octet CDT */
extern int  oftp_enc_cmd1(char code, unsigned char *b, unsigned cap)
                                                        asm("OFTPEC1");
extern int  oftp_enc_cdt(unsigned char *b, unsigned cap) asm("OFTPECDT");
extern int  oftp_dec_cmd1(char code, const unsigned char *b, unsigned len)
                                                        asm("OFTPDC1");
extern int  oftp_dec_cdt(const unsigned char *b, unsigned len)
                                                        asm("OFTPDCDT");

extern int  oftp_enc_ssrm(unsigned char *b, unsigned cap) asm("OFTPESRM");
extern int  oftp_dec_ssrm(const unsigned char *b, unsigned len)
                                                        asm("OFTPDSRM");

extern int  oftp_enc_ssid(const OFTP_SSIDCMD *c, unsigned char *b,
                          unsigned cap)                 asm("OFTPESID");
extern int  oftp_dec_ssid(const unsigned char *b, unsigned len,
                          OFTP_SSIDCMD *c)              asm("OFTPDSID");

extern int  oftp_enc_sfid(int level, const OFTP_SFIDCMD *c,
                          unsigned char *b, unsigned cap) asm("OFTPESFI");
extern int  oftp_dec_sfid(int level, const unsigned char *b, unsigned len,
                          OFTP_SFIDCMD *c)              asm("OFTPDSFI");

extern int  oftp_enc_sfpa(int level, unsigned long acnt, unsigned char *b,
                          unsigned cap)                 asm("OFTPESPA");
extern int  oftp_dec_sfpa(int level, const unsigned char *b, unsigned len,
                          unsigned long *acnt)          asm("OFTPDSPA");

/* code: OFTP_SFNA, OFTP_EFNA or OFTP_ESID */
extern int  oftp_enc_reas(int level, char code, const OFTP_REASCMD *c,
                          unsigned char *b, unsigned cap) asm("OFTPERSN");
extern int  oftp_dec_reas(int level, char code, const unsigned char *b,
                          unsigned len, OFTP_REASCMD *c) asm("OFTPDRSN");

extern int  oftp_enc_efid(int level, const OFTP_EFIDCMD *c,
                          unsigned char *b, unsigned cap) asm("OFTPEEFI");
extern int  oftp_dec_efid(int level, const unsigned char *b, unsigned len,
                          OFTP_EFIDCMD *c)              asm("OFTPDEFI");

/* EFPA: cd = 'Y' when the listener wants to become the speaker */
extern int  oftp_enc_efpa(char cd, unsigned char *b, unsigned cap)
                                                        asm("OFTPEEPA");
extern int  oftp_dec_efpa(const unsigned char *b, unsigned len, char *cd)
                                                        asm("OFTPDEPA");

/* code: OFTP_EERP or OFTP_NERP (NERP from 1.4 on) */
extern int  oftp_enc_erp(int level, char code, const OFTP_ERPCMD *c,
                         unsigned char *b, unsigned cap) asm("OFTPEERP");
extern int  oftp_dec_erp(int level, char code, const unsigned char *b,
                         unsigned len, OFTP_ERPCMD *c)  asm("OFTPDERP");

#endif /* OFTPWIRE_H */
