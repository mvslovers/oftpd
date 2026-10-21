/* oftp#cli.c - the client side of OFTPD's requests, over the SVC
** (MVS only; oftpcli.h)
*/
#include <stdio.h>
#include <string.h>

#include "oftpcli.h"
#include "oftpsvc.h"

/* Over the SVC: the same request again for each further piece, with
** the resume index the last one gave, until a piece ends with the final
** line. OFTPD keeps nothing per caller, so it builds the answer anew
** each time and skips the rows the caller already has (xmem.md
** section 4). A spool that changes in between can move a row from one
** piece into the next; that is the price of a stateless OFTPD.
*/
static int
svc_request(const char *req, OFTP_CLILINE line, void *ctx, char *final,
            int cap, int *code)
{
    char buf[OFTP_SVC_REPMAX];
    unsigned len, next = 0;
    int rc, pieces = 0;

    /* the routine takes at most REQMAX bytes, and a request cut short
    ** would be another request
    */
    if (strlen(req) > OFTP_SVC_REQMAX) {
        snprintf(final, (size_t)cap, "request longer than %d bytes",
                 OFTP_SVC_REQMAX);
        return 8;
    }
    for (;;) {
        unsigned was = next;

        rc = oftp_svc_call(req, buf, sizeof(buf), next, &len, &next);
        switch (rc) {
        case OFTP_SVC_OK:
            rc = oftp_cli_piece(buf, len, line, ctx, final, cap, code);
            if (rc >= 0)
                return rc;
            return oftp_cli_broken(final, cap, code);
        case OFTP_SVC_MORE:
            if (oftp_cli_piece(buf, len, line, ctx, final, cap, code) != -1)
                return oftp_cli_broken(final, cap, code);
            /* a piece that does not move on would be asked for ever */
            if (next <= was || ++pieces >= 1000) {
                snprintf(final, (size_t)cap, "answer from OFTPD does not end");
                return 12;
            }
            continue;
        case OFTP_SVC_BUSY:
            snprintf(final, (size_t)cap, "OFTPD busy, try again");
            return 4;
        case OFTP_SVC_DOWN:
            snprintf(final, (size_t)cap,
                     "OFTPD not reached: not running, or without SVC=");
            return 12;
        case OFTP_SVC_STOPPED:
            snprintf(final, (size_t)cap,
                     "OFTPD stopped before carrying out the request");
            return 12;
        case OFTP_SVC_BADREQ:
            snprintf(final, (size_t)cap, "request refused by the transport");
            return 8;
        default:
            snprintf(final, (size_t)cap, "transport answered %d", rc);
            return 12;
        }
    }
}

int
oftp_cli_request(const char *req, OFTP_CLILINE line, void *ctx, char *final,
                 int cap, int *code)
{
    final[0] = '\0';
    if (code != NULL)
        *code = 0;
    return svc_request(req, line, ctx, final, cap, code);
}
