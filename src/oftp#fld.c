/* oftp#fld.c - field primitives of the command codec */
#include "oftpfld.h"
#include "oftpwire.h"
#include "oftpxlt.h"

void
oftp_put_x(unsigned char *b, const char *s, unsigned w)
{
    unsigned i;

    for (i = 0; i < w && s[i] != '\0'; i++)
        b[i] = OFTP_L2W(s[i]);
    for (; i < w; i++)
        b[i] = OFTP_L2W(' ');
}

int
oftp_put_9(unsigned char *b, unsigned long v, unsigned w)
{
    unsigned i = w;

    while (i > 0) {
        b[--i] = OFTP_L2W('0' + (int)(v % 10));
        v /= 10;
    }
    return v == 0 ? 0 : OFTP_EFIELD;
}

void
oftp_put_c(unsigned char *b, char c)
{
    b[0] = OFTP_L2W(c);
}

void
oftp_put_u2(unsigned char *b, unsigned v)
{
    b[0] = (unsigned char)((v >> 8) & 0xFF);
    b[1] = (unsigned char)(v & 0xFF);
}

void
oftp_get_x(const unsigned char *b, unsigned w, char *out)
{
    unsigned i;

    for (i = 0; i < w; i++)
        out[i] = OFTP_W2L(b[i]);
    while (i > 0 && out[i - 1] == ' ')
        i--;
    out[i] = '\0';
}

int
oftp_get_9(const unsigned char *b, unsigned w, unsigned long *v)
{
    unsigned long n = 0;
    unsigned i;

    for (i = 0; i < w; i++) {
        char c = OFTP_W2L(b[i]);
        unsigned d;

        if (c < '0' || c > '9')
            return OFTP_EFIELD;
        d = (unsigned)(c - '0');
        if (n > (0xFFFFFFFFUL - d) / 10)
            return OFTP_ERANGE;
        n = n * 10 + d;
    }
    *v = n;
    return 0;
}

char
oftp_get_c(const unsigned char *b)
{
    return OFTP_W2L(b[0]);
}

unsigned
oftp_get_u2(const unsigned char *b)
{
    return ((unsigned)b[0] << 8) | b[1];
}
