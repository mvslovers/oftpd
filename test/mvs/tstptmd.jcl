//TSTPTMA  JOB (OFTP),'OFTPD A',CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* The two OFTPDs TSTPTM needs, on one system, each as a batch job
//* that runs until P TSTPTMA / P TSTPTMB (the second job is below).
//*
//* A is the one the commands go to: SVC=, ADMIN= the user who runs
//* TSTPTM, and no partner at all in OFTPPTN. B only receives; it has
//* no SVC= (two OFTPDs cannot share one subsystem name), so it takes
//* no commands, and the file is looked for in its INPREFIX data sets.
//* The spools (INDEXED KEYS(8 0) RECORDSIZE(512 512), as OFTPSPL
//* defines them) must exist: IBMUSER.OFTPA.SPOOL, IBMUSER.OFTPB.SPOOL.
//* The load library has to be APF-authorized (OFTP004I AUTHORIZED BY
//* LIBRARY on the console); change SYS2.LINKLIB to where yours is.
//*
//* Restart for TSTPTM phase B: P TSTPTMA, wait for OFTP003I, submit
//* this job again. The spool and with it the partner stay.
//* ------------------------------------------------------------------
//OFTPD    EXEC PGM=OFTPD,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//OFTPSPL  DD  DISP=OLD,DSN=IBMUSER.OFTPA.SPOOL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//OFTPPRM  DD  *
ODETTEID=O0000000000TSTA
PORT=3305 SVC=237 ADMIN=IBMUSER
INPREFIX=IBMUSER.OFTPAIN TMPPREFIX=IBMUSER.OFTPATMP
SDEB=4096 CREDIT=99 TIMEOUT=60 RETRY=60
/*
//OFTPPTN  DD  *
* no partner here: TSTPTM adds its own with a command
/*
//TSTPTMB  JOB (OFTP),'OFTPD B',CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1)
//OFTPD    EXEC PGM=OFTPD,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//OFTPSPL  DD  DISP=OLD,DSN=IBMUSER.OFTPB.SPOOL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//OFTPPRM  DD  *
ODETTEID=O0000000000TSTB
PORT=3315
INPREFIX=IBMUSER.OFTPBIN TMPPREFIX=IBMUSER.OFTPBTMP
SDEB=4096 CREDIT=99 TIMEOUT=60 RETRY=60
/*
//OFTPPTN  DD  *
PARTNER=TSTA ODETTEID=O0000000000TSTA
  PASSWORD=TSTA OURPASSWORD=TSTB
/*
