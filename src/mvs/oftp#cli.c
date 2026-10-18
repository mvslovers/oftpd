/* oftp#cli.c - the client side of OFTPD's requests, both transports
** (MVS only; oftpcli.h)
*/
#include <stdio.h>
#include <string.h>

#include <mvs/racf.h>

#include "oftpcli.h"
#include "oftpnet.h"
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
            int cap)
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
            rc = oftp_cli_piece(buf, len, line, ctx, final, cap);
            if (rc >= 0)
                return rc;
            snprintf(final, (size_t)cap, "incomplete answer from OFTPD");
            return 12;
        case OFTP_SVC_MORE:
            if (oftp_cli_piece(buf, len, line, ctx, final, cap) != -1) {
                snprintf(final, (size_t)cap, "incomplete answer from OFTPD");
                return 12;
            }
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
oftp_cli_request(const char *host, int port, const char *req,
                 OFTP_CLILINE line, void *ctx, char *final, int cap)
{
    char buf[256];
    int s, rc;

    final[0] = '\0';
    if (host == NULL || host[0] == '\0')
        return svc_request(req, line, ctx, final, cap);
    if ((s = oftp_net_connect(host, port, 3)) < 0) {
        snprintf(final, (size_t)cap, "OFTPD not reached at %s port %d",
                 host, port);
        return 12;
    }
    if (oftp_net_gets(s, buf, sizeof(buf), 10) < 0
        || strncmp(buf, "220", 3) != 0) {
        snprintf(final, (size_t)cap, "no greeting from OFTPD");
        oftp_net_close(s);
        return 12;
    }
    oftp_net_puts(s, req);
    for (;;) {
        if (oftp_net_gets(s, buf, sizeof(buf), 60) < 0) {
            snprintf(final, (size_t)cap, "no answer from OFTPD");
            rc = 12;
            break;
        }
        if (buf[0] == '1') {
            if (line != NULL)
                line(ctx, strlen(buf) > 4 ? buf + 4 : "");
            continue;
        }
        snprintf(final, (size_t)cap, "%s", strlen(buf) > 4 ? buf + 4 : buf);
        rc = oftp_cli_rc(buf);
        break;
    }
    oftp_net_close(s);
    return rc;
}

void
oftp_cli_whoami(char *user)
{
    ACEE *acee = racf_get_acee();
    unsigned n;

    user[0] = '\0';
    if (acee == NULL)
        return;
    n = (unsigned char)acee->aceeuser[0];
    if (n > 8)
        n = 8;
    memcpy(user, acee->aceeuser + 1, n);
    user[n] = '\0';
    while (n > 0 && user[n - 1] == ' ')
        user[--n] = '\0';
}
