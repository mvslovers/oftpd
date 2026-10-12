/* oftp#cli.c - the client side of OFTPD's control port (MVS only) */
#include <stdio.h>
#include <string.h>

#include <mvs/racf.h>

#include "oftpcli.h"
#include "oftpnet.h"

/* The return code for OFTPD's final answer:
**   200 done                     0     210 done, with a warning     4
**   4xx refused for now          4     504 no such transfer         4
**   501 unknown partner         12     5xx anything else wrong      8
*/
static int
reply_rc(const char *line)
{
    if (strncmp(line, "200", 3) == 0)
        return 0;
    if (line[0] == '2' || line[0] == '4' || strncmp(line, "504", 3) == 0)
        return 4;
    if (strncmp(line, "501", 3) == 0)
        return 12;
    return 8;
}

int
oftp_cli_request(const char *host, int port, const char *req,
                 OFTP_CLILINE line, void *ctx, char *final, int cap)
{
    char buf[256];
    int s, rc;

    final[0] = '\0';
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
        rc = reply_rc(buf);
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
