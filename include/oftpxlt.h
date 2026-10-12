#ifndef OFTPXLT_H
#define OFTPXLT_H

/* CP037 <-> ISO 8859-1 (src/oftp#xlt.c) */
extern const unsigned char oftp_e2a[256] asm("OFTPE2A");
extern const unsigned char oftp_a2e[256] asm("OFTPA2E");

/* Local character <-> wire (ASCII) octet.
**
** Every OFTP command field is ISO 646 on the wire. The program's own
** characters are EBCDIC on MVS and ASCII on the workstation, where the
** host tests run, so there the mapping is the identity.
*/
#ifdef __MVS__
#define OFTP_L2W(c) (oftp_e2a[(unsigned char)(c)])
#define OFTP_W2L(c) ((char)oftp_a2e[(unsigned char)(c)])
#else
#define OFTP_L2W(c) ((unsigned char)(c))
#define OFTP_W2L(c) ((char)(unsigned char)(c))
#endif

#endif /* OFTPXLT_H */
