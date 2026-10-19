#ifndef OFTPNET_H
#define OFTPNET_H

/* oftpnet.h - TCP for the monitor and the utility (src/mvs/oftp#net.c)
**
** The sockets are the Hercules host's (X'75'): IPv4 only, no setsockopt,
** a blocking connect() gives up after about a second, select() returns at
** once when its first argument is below 2 (libc370 sockets reference).
*/

#include "oftpsess.h"

/* A listening socket on port, non-blocking, or -1 with *err set */
extern int  oftp_net_listen(int port, int *err)          asm("OFTPNLSN");

/* The next pending connection, or -1 when none is waiting. ip gets the
** caller's address as a dotted string.
*/
extern int  oftp_net_accept(int ls, char *ip, int iplen) asm("OFTPNACC");

/* Connects to host:port, trying for up to secs seconds; -1 on failure */
extern int  oftp_net_connect(const char *host, int port, int secs)
                                                         asm("OFTPNCON");

extern void oftp_net_close(int s)                        asm("OFTPNCLS");

/* The session engine's transport over socket s. stop, when not NULL, is
** polled while waiting: non-zero ends the wait as a lost connection.
*/
typedef struct oftp_netx {
    int         sock;
    volatile int *stop;
} OFTP_NETX;

extern void oftp_net_xport(OFTP_NETX *nx, OFTP_XPORT *x) asm("OFTPNXPT");

#endif /* OFTPNET_H */
