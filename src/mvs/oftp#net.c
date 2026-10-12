/* oftp#net.c - TCP for the monitor and the utility (MVS only) */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <mvs/socket.h>

#include "oftpfld.h"
#include "oftpnet.h"
#include "oftpxlt.h"

#define WIRE_LF     0x0A

int
oftp_net_listen(int port, int *err)
{
    struct sockaddr_in a;
    int s, nb = 1;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        *err = errno;
        return -1;
    }
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, &a, sizeof(a)) < 0 || listen(s, 5) < 0) {
        *err = errno;
        closesocket(s);
        return -1;
    }
    ioctlsocket(s, FIONBIO, &nb);
    *err = 0;
    return s;
}

int
oftp_net_accept(int ls, char *ip, int iplen)
{
    struct sockaddr_in a;
    int len = sizeof(a), s;
    unsigned long h;

    s = accept(ls, &a, &len);
    if (s < 0)
        return -1;
    h = ntohl(a.sin_addr.s_addr);
    snprintf(ip, (size_t)iplen, "%lu.%lu.%lu.%lu", (h >> 24) & 0xFF,
             (h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF);
    return s;
}

int
oftp_net_connect(const char *host, int port, int secs)
{
    struct sockaddr_in a;
    struct hostent *he;
    time_t until = time(NULL) + secs;

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    if (inet_aton(host, &a.sin_addr) == 0) {
        if ((he = gethostbyname(host)) == NULL)
            return -1;
        memcpy(&a.sin_addr, he->h_addr_list[0], sizeof(a.sin_addr));
    }
    /* a blocking connect() gives up after about a second: try again
    ** until the time is up
    */
    do {
        int s = socket(AF_INET, SOCK_STREAM, 0);

        if (s < 0)
            return -1;
        if (connect(s, &a, sizeof(a)) == 0)
            return s;
        closesocket(s);
    } while (time(NULL) < until);
    return -1;
}

void
oftp_net_close(int s)
{
    if (s >= 0)
        closesocket(s);
}

/* Waits up to secs for data on s, polling *stop each second.
** 0 ready, OFTP_XTIMEOUT, OFTP_XCLOSED (stopped).
*/
static int
wait_read(int s, int secs, volatile int *stop)
{
    time_t until = time(NULL) + secs;

    for (;;) {
        fd_set r;
        timeval tv;
        int rc;

        if (stop != NULL && *stop)
            return OFTP_XCLOSED;
        if (s < 1)                      /* select() cannot watch socket 0 */
            return 0;
        FD_ZERO(&r);
        FD_SET(s, &r);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        rc = select(s + 1, &r, NULL, NULL, &tv);
        if (rc > 0)
            return 0;
        if (rc < 0)
            return OFTP_XCLOSED;
        if (time(NULL) >= until)
            return OFTP_XTIMEOUT;
    }
}

static int
x_send(void *ctx, const unsigned char *b, unsigned n)
{
    OFTP_NETX *nx = ctx;

    while (n > 0) {
        int w = send(nx->sock, b, (int)n, 0);

        if (w <= 0)
            return -1;
        b += w;
        n -= (unsigned)w;
    }
    return 0;
}

static int
x_recv(void *ctx, unsigned char *b, unsigned n, int secs)
{
    OFTP_NETX *nx = ctx;

    while (n > 0) {
        int rc = wait_read(nx->sock, secs, nx->stop);

        if (rc != 0)
            return rc;
        rc = recv(nx->sock, b, (int)n, 0);
        if (rc == 0)
            return OFTP_XCLOSED;
        if (rc < 0)
            return errno == EWOULDBLOCK ? 0 : OFTP_XCLOSED;
        b += rc;
        n -= (unsigned)rc;
    }
    return 0;
}

void
oftp_net_xport(OFTP_NETX *nx, OFTP_XPORT *x)
{
    int nb = 1;

    ioctlsocket(nx->sock, FIONBIO, &nb);
    x->ctx = nx;
    x->send = x_send;
    x->recv = x_recv;
}

int
oftp_net_puts(int s, const char *text)
{
    unsigned char buf[600];
    unsigned n = 0;

    while (*text && n < sizeof(buf) - 2)
        buf[n++] = OFTP_L2W(*text++);
    buf[n++] = OFTP_WIRE_CR;
    buf[n++] = WIRE_LF;
    return send(s, buf, (int)n, 0) == (int)n ? 0 : -1;
}

int
oftp_net_gets(int s, char *buf, int cap, int secs)
{
    int n = 0;

    for (;;) {
        unsigned char c;
        int rc;

        if (wait_read(s, secs, NULL) != 0)
            return -1;
        rc = recv(s, &c, 1, 0);
        if (rc == 0)
            return n > 0 ? n : -1;
        if (rc < 0) {
            if (errno == EWOULDBLOCK)
                continue;
            return -1;
        }
        if (c == WIRE_LF)
            break;
        if (c != OFTP_WIRE_CR && n < cap - 1)
            buf[n++] = OFTP_W2L(c);
    }
    buf[n] = '\0';
    return n;
}
