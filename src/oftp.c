/* OFTP - TSO command and batch utility, a client of the monitor
**
**   OFTP SEND PARTNER(name) DSN(data.set.name) [VFN(name)] [FORMAT(T)]
**             [XLATE] [USER(id)]
**   OFTP LIST [PARTNER(name)]
**
** Common operands: HOST(address) PORT(n), default 127.0.0.1 and 3306 --
** the control port of OFTPD. KEYWORD=VALUE is accepted as well as
** KEYWORD(VALUE). Return code 0 done, 4 refused, 8 OFTPD not reached.
**
** During development the control port has no security (CLAUDE.md).
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oftp.h"
#include "oftpnet.h"

static void
upcase(char *s)
{
    for (; *s; s++)
        if ((*s >= 'a' && *s <= 'i') || (*s >= 'j' && *s <= 'r')
            || (*s >= 's' && *s <= 'z'))
            *s = (char)(*s - 'a' + 'A');
}

/* KEY(value) or KEY=value into key and value; quotes removed */
static int
operand(const char *arg, char *key, size_t kcap, char *val, size_t vcap)
{
    const char *p = strpbrk(arg, "(=");
    size_t kl, vl;

    if (p == NULL) {
        snprintf(key, kcap, "%s", arg);
        val[0] = '\0';
        return 0;
    }
    kl = (size_t)(p - arg);
    if (kl >= kcap)
        return -1;
    memcpy(key, arg, kl);
    key[kl] = '\0';
    p++;
    if (*p == '\'')
        p++;
    vl = strlen(p);
    if (arg[kl] == '(' && vl > 0 && p[vl - 1] == ')')
        vl--;
    if (vl > 0 && p[vl - 1] == '\'')
        vl--;
    if (vl >= vcap)
        return -1;
    memcpy(val, p, vl);
    val[vl] = '\0';
    return 0;
}

int
main(int argc, char **argv)
{
    char req[256], host[64] = "127.0.0.1", line[256];
    int port = 3306, i, s, rc = 0;
    size_t n;

    if (argc < 2) {
        printf("OFTP001E USAGE: OFTP SEND PARTNER(..) DSN(..) | OFTP LIST\n");
        return 8;
    }
    snprintf(req, sizeof(req), "%s", argv[1]);
    upcase(req);
    if (strcmp(req, "SEND") != 0 && strcmp(req, "LIST") != 0) {
        printf("OFTP002E UNKNOWN REQUEST %s\n", argv[1]);
        return 8;
    }
    n = strlen(req);
    for (i = 2; i < argc; i++) {
        char key[16], val[80];

        if (operand(argv[i], key, sizeof(key), val, sizeof(val)) != 0) {
            printf("OFTP003E OPERAND TOO LONG: %s\n", argv[i]);
            return 8;
        }
        upcase(key);
        if (strcmp(key, "HOST") == 0) {
            snprintf(host, sizeof(host), "%s", val);
            continue;
        }
        if (strcmp(key, "PORT") == 0) {
            port = atoi(val);
            continue;
        }
        if (strcmp(key, "FORMAT") == 0)
            strcpy(key, "FMT");
        upcase(val);
        if (strcmp(key, "XLATE") == 0 && val[0] == '\0')
            strcpy(val, "Y");
        n += (size_t)snprintf(req + n, sizeof(req) - n, " %s=%s", key, val);
        if (n >= sizeof(req)) {
            printf("OFTP003E REQUEST TOO LONG\n");
            return 8;
        }
    }

    if ((s = oftp_net_connect(host, port, 3)) < 0) {
        printf("OFTP010E OFTPD NOT REACHED AT %s PORT %d\n", host, port);
        return 8;
    }
    if (oftp_net_gets(s, line, sizeof(line), 10) < 0
        || strncmp(line, "220", 3) != 0) {
        printf("OFTP011E NO GREETING FROM OFTPD\n");
        oftp_net_close(s);
        return 8;
    }
    oftp_net_puts(s, req);
    for (;;) {
        if (oftp_net_gets(s, line, sizeof(line), 60) < 0) {
            printf("OFTP012E NO ANSWER FROM OFTPD\n");
            rc = 8;
            break;
        }
        printf("%s\n", strlen(line) > 4 ? line + 4 : line);
        if (line[0] != '1') {
            rc = line[0] == '2' ? 0 : 4;
            break;
        }
    }
    oftp_net_close(s);
    return rc;
}
