#ifndef OFTPCLI_H
#define OFTPCLI_H

/* oftpcli.h - the client side of OFTPD's control port (src/mvs/oftp#cli.c)
**
** One request per connection: OFTPD greets with 220, the client sends
** one line, OFTPD answers with lines "1xx text" and ends with one final
** line "2xx/4xx/5xx text". Used by the OFTP utility and the ISPF dialog.
** During development the port has no security (CLAUDE.md).
*/

/* Called for each 1xx line, text without the code */
typedef void (*OFTP_CLILINE)(void *ctx, const char *text);

/* Sends req and collects the answer. final[cap] gets the final line's
** text. Returns the return code the utility documents:
**   0 done, 4 warning or refused for now, 8 error in the request,
**   12 severe (unknown partner, OFTPD not reached or not answering).
*/
extern int  oftp_cli_request(const char *host, int port, const char *req,
                             OFTP_CLILINE line, void *ctx, char *final,
                             int cap)                   asm("OFTPCLRQ");

/* The userid of this address space's ACEE into user[9], "" if none */
extern void oftp_cli_whoami(char *user)                 asm("OFTPCLWH");

#endif /* OFTPCLI_H */
