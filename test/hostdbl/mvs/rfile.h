#ifndef HOSTDBL_RFILE_H
#define HOSTDBL_RFILE_H
/* Host double of <mvs/rfile.h> for test/tstjob.c: the fields and calls
** src/mvs/oftp#job.c uses, with the signatures of the real header.
*/
#include <stddef.h>

typedef struct rfile RFILE;

struct rfile {
    int     recfm;
#define RFILE_RECFM_F   0
#define RFILE_RECFM_V   1
#define RFILE_RECFM_U   2
    int     lrecl;
    void   *hfile;                      /* the double's read position */
};

extern int  ropen(const char *fnm, int write, RFILE **fp);
extern int  rclose(RFILE *fp);
extern int  rread(RFILE *fp, void *ptr, size_t *read);

#endif
