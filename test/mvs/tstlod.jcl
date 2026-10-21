//TSTLOD   JOB (OFTP),'BACKGROUND LOAD',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* Background load for TSTLAT: two jobs that spin the processor and
//* one that writes and reads a data set, each for the given seconds.
//* Submit this job copied as often as the load needs, then TSTLAT.
//* ------------------------------------------------------------------
//CPU      EXEC PGM=TSTLOD,PARM='CPU,600'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
