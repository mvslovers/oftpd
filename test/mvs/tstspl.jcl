//TSTSPL   JOB (OFTP),'SPOOL TEST',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTSPL against a throwaway spool cluster: define, run, delete.
//* Edit the data set names (IBMUSER, the test library) and the volume
//* (one with VSAM space in your user catalog), then submit.
//* The test library is the one `mbt test --mvs` deployed.
//* ------------------------------------------------------------------
//DEFINE   EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.OFTP370.TSTSPL CLUSTER PURGE
  SET MAXCC = 0
  DEFINE CLUSTER (NAME(IBMUSER.OFTP370.TSTSPL) -
                  INDEXED KEYS(8 0) RECORDSIZE(512 512) -
                  TRACKS(5 5) SHAREOPTIONS(1 3) UNIQUE -
                  VOLUMES(PUB000)) -
         DATA   (NAME(IBMUSER.OFTP370.TSTSPL.DATA)) -
         INDEX  (NAME(IBMUSER.OFTP370.TSTSPL.INDEX))
/*
//RUN      EXEC PGM=TSTSPL,PARM='RUN',COND=(0,NE,DEFINE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTP370.V0R1M0D.TESTLIB
//OFTPSPL  DD  DISP=OLD,DSN=IBMUSER.OFTP370.TSTSPL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//LISTC    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT ENTRIES(IBMUSER.OFTP370.TSTSPL) ALL
  DELETE IBMUSER.OFTP370.TSTSPL CLUSTER PURGE
/*
