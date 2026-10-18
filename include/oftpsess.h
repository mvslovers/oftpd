#ifndef OFTPSESS_H
#define OFTPSESS_H

/* oftpsess.h - one OFTP session, from SSRM to ESID (src/oftp#ses.c)
**
** The engine is sequential: one session runs in one thread, from start
** to end, and talks to the world through three tables of callbacks --
** the transport (a connected stream), the files (spool and data sets),
** the partners. That keeps the protocol testable on the workstation and
** free of MVS. internals/protocol.md section 1 has the rules it follows.
**
** MVP scope: no restart (SSIDREST=N), no secure authentication, EFPA
** never asks for the turn.
*/

#include "oftpwire.h"

/* Transport: a connected byte stream. recv reads exactly n octets. */
typedef struct oftp_xport {
    void       *ctx;
    int       (*send)(void *ctx, const unsigned char *b, unsigned n);
    /* 0 ok, OFTP_XTIMEOUT, OFTP_XCLOSED or another negative error */
    int       (*recv)(void *ctx, unsigned char *b, unsigned n, int secs);
} OFTP_XPORT;

#define OFTP_XTIMEOUT   (-20)
#define OFTP_XCLOSED    (-21)

/* What the session knows about the partner, from the partner table */
typedef struct oftp_peer {
    char        name[9];                /* partner name, for the log     */
    char        code[OFTP_IDLEN + 1];   /* the partner's Odette ID       */
    char        pswd_in[OFTP_PSWLEN + 1];   /* expected from it          */
    char        ourcode[OFTP_IDLEN + 1];    /* our ID towards it         */
    char        pswd_out[OFTP_PSWLEN + 1];  /* sent to it                */
    int         level;                  /* highest level to offer/accept */
    unsigned    sdeb;                   /* exchange buffer cap           */
    unsigned    cred;                   /* credit cap                    */
    char        mode;                   /* 'S', 'R' or 'B'               */
    int         cmpr;                   /* buffer compression allowed    */
} OFTP_PEER;

/* Partners: the responder looks the caller up by its Odette ID.
** 0 found, -1 unknown (ESID 03), or an ESID reason code to end the
** session with, such as 8 when the partner is already in a session.
*/
typedef struct oftp_peers {
    void       *ctx;
    int       (*lookup)(void *ctx, const char *code, OFTP_PEER *peer);
} OFTP_PEERS;

/* A file to send or being received, as the file layer sees it */
typedef void OFTP_FH;

/* Files: the spool and the data sets behind it. Reason codes are SFNA /
** EFNA codes (internals/protocol.md 7.2, 7.3).
*/
typedef struct oftp_files {
    void       *ctx;

    /* Sending. next_out: 1 and *fh set when a file waits for the
    ** partner, sfid filled in except for the counts the engine owns;
    ** 0 when none waits.
    */
    int       (*next_out)(void *ctx, const OFTP_PEER *peer, OFTP_SFIDCMD *sfid,
                          OFTP_FH **fh);
    /* One record (F, V) or a piece of the stream (U, T) into b. *n its
    ** length, *last set on the final piece of a U or T file. Returns 1
    ** with data, 0 at the end of an F or V file, negative on error.
    */
    int       (*read)(void *ctx, OFTP_FH *fh, unsigned char *b, unsigned cap,
                      unsigned *n, int *last);
    /* The outcome: 0 delivered to the partner (EFPA), else the SFNA or
    ** EFNA reason with retry 'Y' or 'N'.
    */
    void      (*out_done)(void *ctx, OFTP_FH *fh, int reas, char retry);

    /* Receiving. open_in: 0 accepted, or the SFNA reason with *retry. */
    int       (*open_in)(void *ctx, const OFTP_PEER *peer,
                         const OFTP_SFIDCMD *sfid, OFTP_FH **fh, char *retry);
    /* A record (F, V; eor 1) or a piece of the stream (U, T).
    ** 0 ok, else the EFNA reason to answer with.
    */
    int       (*write)(void *ctx, OFTP_FH *fh, const unsigned char *b,
                       unsigned n, int eor);
    /* End of file. ok: the counts matched and every write succeeded; the
    ** file layer then closes and returns 0 (EFPA) or an EFNA reason. With
    ** ok 0 it discards the file and its return is ignored.
    */
    int       (*close_in)(void *ctx, OFTP_FH *fh, int ok);

    /* End-to-end responses. next_erp: 1 and *code ('E' / 'N') when one
    ** waits for the partner; erp_sent once the partner answered RTR.
    */
    int       (*next_erp)(void *ctx, const OFTP_PEER *peer, char *code,
                          OFTP_ERPCMD *erp, OFTP_FH **h);
    void      (*erp_sent)(void *ctx, OFTP_FH *h);
    void      (*erp_recv)(void *ctx, const OFTP_PEER *peer, char code,
                          const OFTP_ERPCMD *erp);
} OFTP_FILES;

/* Log callback: one line, without the newline */
typedef void (*OFTP_LOGFN)(void *ctx, const char *line);

/* Largest buffer the engine is built for: 99999 is the protocol's limit,
** a cap far below it keeps a session small. A partner offering more is
** answered with this.
*/
#define OFTP_SDEB_MAX   8192

typedef struct oftp_sess {
    /* set by the caller */
    OFTP_XPORT  x;
    OFTP_FILES  f;
    OFTP_PEERS  p;
    OFTP_LOGFN  log;
    void       *logctx;
    int         initiator;              /* 1: we called                  */
    OFTP_PEER   peer;                   /* initiator: the partner called */
    char        localcode[OFTP_IDLEN + 1];  /* responder: our ID         */
    int         timeout;                /* seconds per expected answer   */

    /* negotiated */
    int         level;
    unsigned    sdeb;
    unsigned    cred;
    int         cmpr;
    char        mode;

    /* results */
    int         esid;                   /* reason the session ended with */
    int         esid_local;             /* 1: we sent it, 0: received    */
    unsigned    files_out, files_in, erps_out, erps_in;

    /* the record buffer of the file in transfer, NULL between files: an
    ** abend in the middle leaves it here for oftp_sess_free() */
    unsigned char *xbuf;

    /* buffers: STH + exchange buffer */
    unsigned char ib[4 + OFTP_SDEB_MAX];
    unsigned char ob[4 + OFTP_SDEB_MAX];
} OFTP_SESS;

/* Runs the session to its end. 0 after a normal end (ESID 00 either
** way), else the ESID reason. The transport is not closed.
*/
extern int oftp_sess_run(OFTP_SESS *s)                  asm("OFTPSRUN");

/* After a session left by an abend (longjmp, try()): frees what the
** engine still holds. Does nothing after a normal end.
*/
extern void oftp_sess_free(OFTP_SESS *s)                asm("OFTPSFRE");

#endif /* OFTPSESS_H */
