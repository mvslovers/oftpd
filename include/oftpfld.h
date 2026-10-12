#ifndef OFTPFLD_H
#define OFTPFLD_H

/* oftpfld.h - field primitives of the command codec (src/oftp#fld.c).
** Private to the codec. Positions and widths are the caller's.
*/

/* X(w): s translated, padded with blanks; longer s is cut at w */
extern void oftp_put_x(unsigned char *b, const char *s, unsigned w)
                                                        asm("OFTPPUTX");
/* 9(w): v as w decimal digits with leading zeros. OFTP_EFIELD if v
** needs more than w digits.
*/
extern int  oftp_put_9(unsigned char *b, unsigned long v, unsigned w)
                                                        asm("OFTPPUT9");
/* One character */
extern void oftp_put_c(unsigned char *b, char c)        asm("OFTPPUTC");
/* U(2): big endian */
extern void oftp_put_u2(unsigned char *b, unsigned v)   asm("OFTPPUTU");

/* X(w) into out[w+1], translated, trailing blanks removed */
extern void oftp_get_x(const unsigned char *b, unsigned w, char *out)
                                                        asm("OFTPGETX");
/* 9(w): OFTP_EFIELD on a non-digit, OFTP_ERANGE beyond 32 bits */
extern int  oftp_get_9(const unsigned char *b, unsigned w,
                       unsigned long *v)                asm("OFTPGET9");
extern char oftp_get_c(const unsigned char *b)          asm("OFTPGETC");
extern unsigned oftp_get_u2(const unsigned char *b)     asm("OFTPGETU");

/* The wire's carriage return; X'8D' is accepted on input as well */
#define OFTP_WIRE_CR    0x0D
#define OFTP_WIRE_CRP   0x8D
#define OFTP_IS_CR(o)   ((o) == OFTP_WIRE_CR || (o) == OFTP_WIRE_CRP)

#endif /* OFTPFLD_H */
