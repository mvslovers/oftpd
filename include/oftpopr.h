#ifndef OFTPOPR_H
#define OFTPOPR_H

/* oftpopr.h - the operator command line of F OFTPD,... (src/oftp#opr.c) */

#include <stddef.h>

/* Splits cmd into the verb and its first two operands; blanks and commas
** separate. A field that does not fit is cut, and what is left of it
** starts the next one, so too much input never disappears.
*/
extern void oftp_opr_split(const char *cmd, char *verb, size_t vsz,
                           char *arg, size_t asz, char *arg2, size_t a2sz)
                                                        asm("OFTPOPSP");

/* 1 if verb takes one operand at most and arg2, the second, is there:
** PURGE 30,x or HOLD P1,P2 must be refused, not run on the first.
*/
extern int  oftp_opr_extra(const char *verb, const char *arg2)
                                                        asm("OFTPOPXT");

#endif
