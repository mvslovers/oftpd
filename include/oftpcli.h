#ifndef OFTPCLI_H
#define OFTPCLI_H

/* oftpcli.h - the client side of OFTPD's requests (src/mvs/oftp#cli.c,
** src/oftp#rep.c)
**
** OFTPD answers a request with lines "1xx text" and ends with one final
** line "2xx/4xx/5xx text". The request travels by the cross-memory SVC
** (internals/xmem.md); the caller's userid is OFTPD's to read, not the
** caller's to say. Used by the OFTP utility and the ISPF dialog.
*/

/* Called for each 1xx line, text without the code */
typedef void (*OFTP_CLILINE)(void *ctx, const char *text);

/* Sends req over the SVC and collects the answer. final[cap] gets the
** final line's text, and *code (when not NULL) the three digits of its
** code, 0 when OFTPD gave no answer.
** Returns the return code the utility documents:
**   0 done, 4 warning or refused for now, 8 error in the request,
**   12 severe (unknown partner, OFTPD not reached or not answering).
*/
extern int  oftp_cli_request(const char *req, OFTP_CLILINE line, void *ctx,
                             char *final, int cap, int *code)
                                                        asm("OFTPCLRQ");

/* The return code for one final line (the table in oftp#rep.c) */
extern int  oftp_cli_rc(const char *line)               asm("OFTPCLRC");

/* One piece of an answer: buf[len] holds whole lines, each ending '\n'.
** line() gets each 1xx line's text. Returns the return code when the
** piece ends with the final line, whose text goes to final[cap]; -1
** when it ends with a 1xx line, so that more is to come; -2 when it is
** not an answer (a line without its '\n', or a line after the final
** one). *code (when not NULL) gets the final line's code.
*/
extern int  oftp_cli_piece(const char *buf, unsigned len, OFTP_CLILINE line,
                           void *ctx, char *final, int cap, int *code)
                                                        asm("OFTPCLPC");

#endif /* OFTPCLI_H */
