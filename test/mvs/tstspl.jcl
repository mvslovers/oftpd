//TSTSPL   JOB (OFTP),'SPOOL TEST',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1),REGION=8M
//* ------------------------------------------------------------------
//* TSTSPL against a throwaway spool cluster: define, run, delete.
//* Edit the data set names (IBMUSER, the test library) and the volume
//* (one with VSAM space in your user catalog), then submit.
//* The test library is the one `mbt test --mvs` deployed.
//* The spool thread's abend recovery (#49) is part of the run only in a
//* build with -DOFTP_DEBUG_ABEND (cflags under [build]); otherwise the
//* test says SKIP for it. The abend test holds 100 KB to calibrate its
//* free-storage measurement, which needs a region well above the
//* default (about 96 KB free): hence REGION=8M on the JOB card.
//* ------------------------------------------------------------------
//DEFINE   EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.OFTPD.TSTSPL CLUSTER PURGE
  SET MAXCC = 0
  DEFINE CLUSTER (NAME(IBMUSER.OFTPD.TSTSPL) -
                  INDEXED KEYS(8 0) RECORDSIZE(512 512) -
                  TRACKS(5 5) SHAREOPTIONS(1 3) UNIQUE -
                  VOLUMES(PUB000)) -
         DATA   (NAME(IBMUSER.OFTPD.TSTSPL.DATA)) -
         INDEX  (NAME(IBMUSER.OFTPD.TSTSPL.INDEX))
/*
//RUN      EXEC PGM=TSTSPL,PARM='RUN',COND=(0,NE,DEFINE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//OFTPSPL  DD  DISP=OLD,DSN=IBMUSER.OFTPD.TSTSPL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//LISTC    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT ENTRIES(IBMUSER.OFTPD.TSTSPL) ALL
  DELETE IBMUSER.OFTPD.TSTSPL CLUSTER PURGE
/*
