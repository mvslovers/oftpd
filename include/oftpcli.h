#ifndef OFTPCLI_H
#define OFTPCLI_H

/* oftpcli.h - the client side of OFTPD's requests (src/mvs/oftp#cli.c,
** src/oftp#rep.c)
**
** OFTPD answers a request with lines "1xx text" and ends with one final
** line "2xx/4xx/5xx text", whichever transport carried it. Two do:
**
** - The cross-memory SVC (internals/xmem.md), when no host is given.
**   The caller's userid is OFTPD's to read, not the caller's to say.
** - The TCP control port, one request per connection after a 220
**   greeting, when a host is given. During development; it has no
**   security and believes OWNER= (CLAUDE.md).
**
** There is no fallback from one to the other: a caller that relies on
** its identity being real must not land on the port without knowing.
** Used by the OFTP utility and the ISPF dialog.
*/

/* Called for each 1xx line, text without the code */
typedef void (*OFTP_CLILINE)(void *ctx, const char *text);

/* Sends req and collects the answer: over the SVC when host is NULL or
** "", else to host and port. final[cap] gets the final line's text.
** Returns the return code the utility documents:
**   0 done, 4 warning or refused for now, 8 error in the request,
**   12 severe (unknown partner, OFTPD not reached or not answering).
*/
extern int  oftp_cli_request(const char *host, int port, const char *req,
                             OFTP_CLILINE line, void *ctx, char *final,
                             int cap)                   asm("OFTPCLRQ");

/* The return code for one final line (the table in oftp#rep.c) */
extern int  oftp_cli_rc(const char *line)               asm("OFTPCLRC");

/* One piece of an answer: buf[len] holds whole lines, each ending '\n'.
** line() gets each 1xx line's text. Returns the return code when the
** piece ends with the final line, whose text goes to final[cap]; -1
** when it ends with a 1xx line, so that more is to come; -2 when it is
** not an answer (a line without its '\n', or a line after the final
** one).
*/
extern int  oftp_cli_piece(const char *buf, unsigned len, OFTP_CLILINE line,
                           void *ctx, char *final, int cap)
                                                        asm("OFTPCLPC");

/* The userid of this address space's ACEE into user[9], "" if none */
extern void oftp_cli_whoami(char *user)                 asm("OFTPCLWH");

#endif /* OFTPCLI_H */
