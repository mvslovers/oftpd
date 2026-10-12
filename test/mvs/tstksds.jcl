//TSTKSDS  JOB (OFTP),'KSDS SMOKE TEST',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTKSDS against a throwaway KSDS: define, run, delete.
//* Edit the data set names (IBMUSER, the test library) and the volume
//* (one with VSAM space in your user catalog), then submit.
//* The test library is the one `mbt test --mvs` deployed.
//* ------------------------------------------------------------------
//DEFINE   EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.OFTP370.TSTKSDS CLUSTER PURGE
  SET MAXCC = 0
  DEFINE CLUSTER (NAME(IBMUSER.OFTP370.TSTKSDS) -
                  INDEXED KEYS(8 0) RECORDSIZE(80 80) -
                  TRACKS(5 5) SHAREOPTIONS(1 3) UNIQUE -
                  VOLUMES(PUB000)) -
         DATA   (NAME(IBMUSER.OFTP370.TSTKSDS.DATA)) -
         INDEX  (NAME(IBMUSER.OFTP370.TSTKSDS.INDEX))
/*
//RUN      EXEC PGM=TSTKSDS,PARM='RUN',COND=(0,NE,DEFINE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTP370.V0R1M0D.TESTLIB
//KSDS     DD  DISP=OLD,DSN=IBMUSER.OFTP370.TSTKSDS
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//LISTC    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT ENTRIES(IBMUSER.OFTP370.TSTKSDS) ALL
  DELETE IBMUSER.OFTP370.TSTKSDS CLUSTER PURGE
/*
