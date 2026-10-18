#ifndef OFTPSEC_H
#define OFTPSEC_H

/* oftpsec.h - the caller's own authority (src/mvs/oftp#sec.c, MVS)
**
** OFTPD reads a data set it sends with its own authority, which is more
** than the user who queued it may have. So a send over the cross-memory
** transport -- the one place the caller's userid is real -- is checked at
** the time it is queued: may that userid read the data set? An ACEE is
** built for it with RACINIT PASSCHK=NO (OFTPD is authorized), the
** question is a RACHECK CLASS=DATASET ATTR=READ, and the ACEE is deleted
** again. A system without a security product (CVTRAC 0) has nothing to
** check and lets every send through.
*/

#include <stddef.h>

/* 0: may read dsn (a member's library counts) -- or, with alter set,
** may delete it (ALTER, for DISP=DELETE). 1: may not, and why says so
** for the caller and the console; also for a caller without a userid
** on a system that has a security product.
*/
extern int oftp_sec_may_read(const char *user, const char *dsn, int alter,
                             char *why, size_t cap)     asm("OFTPSECR");

#endif /* OFTPSEC_H */
