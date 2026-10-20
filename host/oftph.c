/* oftph - the session engine on the workstation, over real TCP
**
** For interop tests against other OFTP implementations before anything
** runs on MVS. Not part of the product. Build: host/build.sh
**
**   oftph call HOST PORT [options] [FILE ...]   call a partner, send files
**   oftph listen PORT [options]                  answer one call
**
** options:
**   -i ID      our Odette ID                 -I ID    the partner's Odette ID
**   -p PW      our password (sent)           -P PW    the partner's password
**   -l LEVEL   highest level (5, 4, 2)       -b SDEB  buffer   -c CRED credit
**   -z         offer buffer compression      -d DIR   where received files go
**   -f FMT     format of the files sent: T (default), U, F, V
**   -r LRECL   record length for F and V
**   -X         drop the line when the first EERP or NERP arrives, before
**              the RTR: a session cut while the other side waits for it
**   -D ID      address the files to ID (SFIDDEST), not to the partner:
**              for a partner reached through the one called
**
** A file is sent under its base name, upper-cased and cut to 26
** characters. Received files are written to DIR under their virtual file
** name: T and U as the octets received, F and V one record per line.
*/
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "oftpsess.h"

static int
x_send(void *ctx, const unsigned char *b, unsigned n)
{
    int fd = *(int *)ctx;

    while (n > 0) {
        ssize_t w = send(fd, b, n, 0);

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
    int fd = *(int *)ctx;

    while (n > 0) {
        struct pollfd p;
        ssize_t r;

        p.fd = fd;
        p.events = POLLIN;
        if (poll(&p, 1, secs * 1000) == 0)
            return OFTP_XTIMEOUT;
        r = recv(fd, b, n, 0);
        if (r == 0)
            return OFTP_XCLOSED;
        if (r < 0)
            return -1;
        b += r;
        n -= (unsigned)r;
    }
    return 0;
}

/* ---- files on the workstation ------------------------------------ */

typedef struct hout {
    const char *path;
    FILE       *fp;
    int         done;
    char        dsn[OFTP_DSNLEN + 1];
} HOUT;

typedef struct hfiles {
    HOUT       *out;
    int         nout;
    char        fmt;
    unsigned    lrecl;
    const char *dir;
    const char *dest;           /* -D: the final recipient */
    FILE       *in;
    char        inname[OFTP_DSNLEN + 1];
    char        infmt;
    char        erp_dsn[OFTP_DSNLEN + 1];   /* one EERP owed at a time */
    char        erp_date[OFTP_DATLEN + 1];
    char        erp_time[OFTP_TIMLEN + 1];
    char        erp_orig[OFTP_IDLEN + 1];
    char        erp_user[OFTP_USRLEN + 1];
    int         erp_pending;
    int         cut;            /* -X: drop the line at the first EERP/NERP */
} HFILES;

static void
stamp(char *date, char *tim)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    static unsigned counter;

    strftime(date, OFTP_DATLEN + 1, "%Y%m%d", tm);
    snprintf(tim, OFTP_TIMLEN + 1, "%02u%02u%02u%04u",
             (unsigned)tm->tm_hour % 100, (unsigned)tm->tm_min % 100,
             (unsigned)tm->tm_sec % 100, ++counter % 10000);
}

static int
h_next_out(void *ctx, const OFTP_PEER *peer, OFTP_SFIDCMD *f, OFTP_FH **fh)
{
    HFILES *h = ctx;
    int i;

    for (i = 0; i < h->nout; i++) {
        HOUT *o = &h->out[i];
        const char *base;
        long size;
        unsigned k;

        if (o->done)
            continue;
        o->done = 1;
        if ((o->fp = fopen(o->path, "rb")) == NULL) {
            fprintf(stderr, "oftph: %s: %s\n", o->path, strerror(errno));
            continue;
        }
        base = strrchr(o->path, '/');
        base = base ? base + 1 : o->path;
        for (k = 0; k < OFTP_DSNLEN && base[k]; k++)
            o->dsn[k] = (char)toupper((unsigned char)base[k]);
        o->dsn[k] = '\0';
        fseek(o->fp, 0, SEEK_END);
        size = ftell(o->fp);
        rewind(o->fp);
        strcpy(f->dsn, o->dsn);
        stamp(f->date, f->time);
        strcpy(f->dest, h->dest ? h->dest : peer->code);
        strcpy(f->orig, peer->ourcode);
        f->fmt = h->fmt;
        f->lrecl = (h->fmt == 'F' || h->fmt == 'V') ? h->lrecl : 0;
        f->fsiz = (unsigned long)(size + 1023) / 1024;
        *fh = o;
        return 1;
    }
    return 0;
}

static int
h_read(void *ctx, OFTP_FH *fh, unsigned char *b, unsigned cap, unsigned *n,
       int *last)
{
    HFILES *h = ctx;
    HOUT *o = fh;
    size_t r;

    if (h->fmt == 'F' || h->fmt == 'V') {
        /* one line is one record; F is padded to LRECL with blanks */
        char line[32768];
        size_t len;

        if (fgets(line, sizeof(line), o->fp) == NULL)
            return 0;
        len = strcspn(line, "\r\n");
        if (len > cap)
            return -1;
        if (h->fmt == 'F') {
            memset(b, ' ', cap);
            memcpy(b, line, len);
            *n = cap;
        } else {
            memcpy(b, line, len);
            *n = (unsigned)len;
        }
        return 1;
    }
    r = fread(b, 1, cap, o->fp);
    *n = (unsigned)r;
    *last = r < cap;
    if (*last)
        return 1;
    /* look ahead so the last piece carries the end of record */
    {
        int c = fgetc(o->fp);

        if (c == EOF)
            *last = 1;
        else
            ungetc(c, o->fp);
    }
    return 1;
}

static void
h_out_done(void *ctx, OFTP_FH *fh, int reas, char retry)
{
    HOUT *o = fh;

    (void)ctx;
    fclose(o->fp);
    if (reas == 0)
        printf("sent      %s\n", o->dsn);
    else
        printf("refused   %s reason %02d retry %c\n", o->dsn, reas, retry);
}

static int
h_open_in(void *ctx, const OFTP_PEER *peer, const OFTP_SFIDCMD *f,
          OFTP_FH **fh, char *retry)
{
    HFILES *h = ctx;
    char path[1024];
    unsigned k;

    (void)peer;
    if (h->erp_pending) {           /* keep it simple: one at a time */
        *retry = 'Y';
        return 99;
    }
    for (k = 0; f->dsn[k]; k++)
        if (f->dsn[k] == '/')
            return 1;
    snprintf(path, sizeof(path), "%s/%s", h->dir, f->dsn);
    if ((h->in = fopen(path, "wb")) == NULL) {
        *retry = 'Y';
        return 12;
    }
    strcpy(h->inname, f->dsn);
    h->infmt = f->fmt;
    strcpy(h->erp_dsn, f->dsn);
    strcpy(h->erp_date, f->date);
    strcpy(h->erp_time, f->time);
    strcpy(h->erp_orig, f->orig);
    strcpy(h->erp_user, f->user);
    *fh = h;
    return 0;
}

static int
h_write(void *ctx, OFTP_FH *fh, const unsigned char *b, unsigned n, int eor)
{
    HFILES *h = ctx;

    (void)fh;
    if (fwrite(b, 1, n, h->in) != n)
        return 12;
    if (eor && fputc('\n', h->in) == EOF)
        return 12;
    return 0;
}

static int
h_close_in(void *ctx, OFTP_FH *fh, int ok)
{
    HFILES *h = ctx;
    int rc;

    (void)fh;
    rc = fclose(h->in);
    if (ok && rc == 0) {
        h->erp_pending = 1;
        printf("received  %s (%c)\n", h->inname, h->infmt);
        return 0;
    }
    return ok ? 12 : 0;
}

static int
h_next_erp(void *ctx, const OFTP_PEER *peer, char *code, OFTP_ERPCMD *e,
           OFTP_FH **hh)
{
    HFILES *h = ctx;

    if (!h->erp_pending)
        return 0;
    memset(e, 0, sizeof(*e));
    *code = OFTP_EERP;
    strcpy(e->dsn, h->erp_dsn);
    strcpy(e->date, h->erp_date);
    strcpy(e->time, h->erp_time);
    strcpy(e->user, h->erp_user);
    strcpy(e->dest, h->erp_orig);
    strcpy(e->orig, peer->ourcode);
    *hh = h;
    return 1;
}

static void
h_erp_sent(void *ctx, OFTP_FH *hh)
{
    HFILES *h = ctx;

    (void)hh;
    printf("eerp sent %s\n", h->erp_dsn);
    h->erp_pending = 0;
}

static void
h_erp_recv(void *ctx, const OFTP_PEER *peer, char code, const OFTP_ERPCMD *e)
{
    HFILES *h = ctx;

    (void)peer;
    printf("%s  %s %s %s from %s\n", code == OFTP_EERP ? "eerp got" : "nerp got",
           e->dsn, e->date, e->time, e->orig);
    if (h->cut) {
        /* the engine would answer RTR next: the line goes away instead */
        printf("cut: line dropped, no RTR sent\n");
        fflush(stdout);
        _exit(3);
    }
}

/* ---- partners ---------------------------------------------------- */

static OFTP_PEER config;

static int
lookup(void *ctx, const char *code, OFTP_PEER *peer)
{
    (void)ctx;
    if (config.code[0] && strcmp(code, config.code) != 0)
        return -1;
    *peer = config;
    strcpy(peer->code, code);
    return 0;
}

static void
logline(void *ctx, int warn, const char *line)
{
    (void)ctx;
    (void)warn;
    fprintf(stderr, "oftp: %s\n", line);
}

static void
usage(void)
{
    fprintf(stderr,
            "usage: oftph call HOST PORT [options] [FILE ...]\n"
            "       oftph listen PORT [options]\n"
            "options: -i ourID -I partnerID -p ourPW -P partnerPW -l level\n"
            "         -b sdeb -c credit -z -d dir -f T|U|F|V -r lrecl -D destID -X\n");
    exit(2);
}

static void
copy(char *dst, const char *src, size_t cap)
{
    snprintf(dst, cap, "%s", src);
    for (; *dst; dst++)
        *dst = (char)toupper((unsigned char)*dst);
}

int
main(int argc, char **argv)
{
    static OFTP_SESS s;
    HFILES h;
    int fd, call, argi, port, rc, opt;
    const char *host = NULL;

    if (argc < 3)
        usage();
    call = strcmp(argv[1], "call") == 0;
    if (!call && strcmp(argv[1], "listen") != 0)
        usage();
    if (call) {
        if (argc < 4)
            usage();
        host = argv[2];
        port = atoi(argv[3]);
        argi = 4;
    } else {
        port = atoi(argv[2]);
        argi = 3;
    }

    memset(&config, 0, sizeof(config));
    strcpy(config.name, "PARTNER");
    strcpy(config.ourcode, "O0013000000OFTPH");
    config.level = 5;
    config.sdeb = 4096;
    config.cred = 99;
    config.mode = 'B';
    memset(&h, 0, sizeof(h));
    h.fmt = 'T';
    h.dir = ".";

    optind = argi;
    while ((opt = getopt(argc, argv, "i:I:p:P:l:b:c:zd:f:r:D:X")) != -1) {
        switch (opt) {
        case 'i': copy(config.ourcode, optarg, sizeof(config.ourcode)); break;
        case 'I': copy(config.code, optarg, sizeof(config.code)); break;
        case 'p': copy(config.pswd_out, optarg, sizeof(config.pswd_out)); break;
        case 'P': copy(config.pswd_in, optarg, sizeof(config.pswd_in)); break;
        case 'l': config.level = atoi(optarg); break;
        case 'b': config.sdeb = (unsigned)atoi(optarg); break;
        case 'c': config.cred = (unsigned)atoi(optarg); break;
        case 'z': config.cmpr = 1; break;
        case 'd': h.dir = optarg; break;
        case 'f': h.fmt = (char)toupper((unsigned char)optarg[0]); break;
        case 'r': h.lrecl = (unsigned)atoi(optarg); break;
        case 'D': h.dest = optarg; break;
        case 'X': h.cut = 1; break;
        default: usage();
        }
    }
    if ((h.fmt == 'F' || h.fmt == 'V') && h.lrecl == 0)
        usage();
    h.nout = argc - optind;
    h.out = calloc((size_t)(h.nout > 0 ? h.nout : 1), sizeof(HOUT));
    for (rc = 0; rc < h.nout; rc++)
        h.out[rc].path = argv[optind + rc];

    if (call) {
        struct addrinfo hints, *ai;
        char ps[16];

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        snprintf(ps, sizeof(ps), "%d", port);
        if (getaddrinfo(host, ps, &hints, &ai) != 0) {
            fprintf(stderr, "oftph: cannot resolve %s\n", host);
            return 2;
        }
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) != 0) {
            fprintf(stderr, "oftph: connect: %s\n", strerror(errno));
            return 2;
        }
        freeaddrinfo(ai);
    } else {
        struct sockaddr_in a;
        int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;

        setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(ls, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(ls, 1) != 0) {
            fprintf(stderr, "oftph: listen: %s\n", strerror(errno));
            return 2;
        }
        fprintf(stderr, "oftph: waiting on port %d\n", port);
        fd = accept(ls, NULL, NULL);
        close(ls);
    }

    s.x.ctx = &fd;
    s.x.send = x_send;
    s.x.recv = x_recv;
    s.f.ctx = &h;
    s.f.next_out = h_next_out;
    s.f.read = h_read;
    s.f.out_done = h_out_done;
    s.f.open_in = h_open_in;
    s.f.write = h_write;
    s.f.close_in = h_close_in;
    s.f.next_erp = h_next_erp;
    s.f.erp_sent = h_erp_sent;
    s.f.erp_recv = h_erp_recv;
    s.p.lookup = lookup;
    s.log = logline;
    s.timeout = 60;
    s.initiator = call;
    s.peer = config;
    strcpy(s.localcode, config.ourcode);

    rc = oftp_sess_run(&s);
    close(fd);
    printf("session end: ESID %02d (%s), %u files out, %u in, %u/%u EERP out/in\n",
           s.esid, s.esid_local ? "ours" : "partner's", s.files_out,
           s.files_in, s.erps_out, s.erps_in);
    return rc == 0 ? 0 : 1;
}
