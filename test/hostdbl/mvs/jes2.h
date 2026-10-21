#ifndef HOSTDBL_JES2_H
#define HOSTDBL_JES2_H
/* Host double of <mvs/jes2.h> for test/tstjob.c: the internal reader
** calls src/mvs/oftp#job.c makes, with the signatures of the real header.
*/
typedef struct vsfile VSFILE;

extern int  jesiropn(VSFILE **vsfile);
extern int  jesirput(VSFILE *vsfile, const char card[80]);
extern int  jesircls(VSFILE *vsfile);
extern int  jesircl2(VSFILE *vsfile, unsigned char jobid[8]);

#endif
