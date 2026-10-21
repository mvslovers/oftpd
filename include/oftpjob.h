#ifndef OFTPJOB_H
#define OFTPJOB_H

/* oftpjob.h - the job after receipt (src/mvs/oftp#job.c, MVS only;
** internals/receive-rules.md section 4)
*/

#include "oftpsub.h"

#define OFTP_JOB_ID     9               /* "JOB01234", NUL included       */
#define OFTP_JOB_WHY    48              /* the reason, NUL included       */

/* Reads job (a "pds(member)" of a rule's JOB=), puts the ?symbols of x in
** and gives it to the internal reader of the caller. The member is read
** and checked to the end before the reader is opened: a job that is half
** on the reader is run by JES all the same.
**
** 0: submitted, jobid is the job's JES id. Otherwise nothing was
** submitted and why says why (a short phrase for the console, the spool
** record and SHOW): member missing or unreadable, empty, a record too
** long, a line that grows past its columns, an unsafe ?VFILE, or the
** reader failing.
*/
extern int  oftp_job_submit(const char *job, const OFTP_SUBCTX *x,
                            char *jobid, char *why)         asm("OFTPJOBS");

#endif /* OFTPJOB_H */
