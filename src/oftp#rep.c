/* oftp#rep.c - an answer of OFTPD, as the client reads it (portable)
**
** The same lines arrive by either transport: over TCP one at a time,
** over the SVC as a buffer of lines each ending '\n', and a long answer
** in several such buffers (internals/xmem.md section 4). What a line
** means is decided here, once, for both.
*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "oftpcli.h"

/* The return code for OFTPD's final answer:
**   200 done                     0     210 done, with a warning     4
**   4xx refused for now          4     504 no such transfer         4
**   501 unknown partner         12     5xx anything else wrong      8
*/
int
oftp_cli_rc(const char *line)
{
    if (strncmp(line, "200", 3) == 0)
        return 0;
    if (line[0] == '2' || line[0] == '4' || strncmp(line, "504", 3) == 0)
        return 4;
    if (strncmp(line, "501", 3) == 0)
        return 12;
    return 8;
}

/* The three digits that open a line; 0 when it does not start with them */
static int
reply_code(const char *line)
{
    if (!isdigit((unsigned char)line[0]) || !isdigit((unsigned char)line[1])
        || !isdigit((unsigned char)line[2]))
        return 0;
    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

int
oftp_cli_piece(const char *buf, unsigned len, OFTP_CLILINE line, void *ctx,
               char *final, int cap, int *code)
{
    char one[256];
    unsigned i = 0;

    while (i < len) {
        unsigned e = i, n;

        while (e < len && buf[e] != '\n')
            e++;
        if (e == len)
            return -2;                  /* a line without its end     */
        n = e - i;
        if (n >= sizeof(one))
            n = sizeof(one) - 1;
        memcpy(one, buf + i, n);
        one[n] = '\0';
        i = e + 1;
        if (one[0] == '1') {
            if (line != NULL)
                line(ctx, n > 4 ? one + 4 : "");
            continue;
        }
        /* the final line ends the answer; nothing may follow it */
        snprintf(final, (size_t)cap, "%s", n > 4 ? one + 4 : one);
        if (code != NULL)
            *code = reply_code(one);
        return i == len ? oftp_cli_rc(one) : -2;
    }
    return -1;
}
