//TSTRCVR  JOB (OFTP),'REPLACE RECOVERY',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTRCVR (#44): what oftp_mon_recover() does with a REPLACE=Y that a
//* stop cut between its two renames, and what oftp_dsf_kind() says of a
//* PS, a PDS, a VSAM cluster and a member. Needs no running OFTPD.
//* Edit the data set names (IBMUSER, the test library) and the volume
//* (one with VSAM space in your user catalog), then submit. The test
//* library is the one `mbt test --mvs` deployed. The console shows
//* OFTP239I (1) and OFTP243W (2); the log of the job does not.
//* ------------------------------------------------------------------
//OLD      EXEC PGM=IEFBR14
//D1       DD  DISP=(MOD,DELETE),DSN=IBMUSER.OFTPD.TRCVR.PDS,
//             UNIT=SYSDA,SPACE=(TRK,1)
//DEFINE   EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.OFTPD.TRCVR.SPOOL CLUSTER PURGE
  DELETE IBMUSER.OFTPD.TRCVR.KSDS CLUSTER PURGE
  SET MAXCC = 0
  DEFINE CLUSTER (NAME(IBMUSER.OFTPD.TRCVR.SPOOL) -
                  INDEXED KEYS(8 0) RECORDSIZE(512 512) -
                  TRACKS(5 5) SHAREOPTIONS(1 3) UNIQUE -
                  VOLUMES(WORK01)) -
         DATA   (NAME(IBMUSER.OFTPD.TRCVR.SPOOL.DATA)) -
         INDEX  (NAME(IBMUSER.OFTPD.TRCVR.SPOOL.INDEX))
  DEFINE CLUSTER (NAME(IBMUSER.OFTPD.TRCVR.KSDS) -
                  INDEXED KEYS(8 0) RECORDSIZE(80 80) -
                  TRACKS(1 1) SHAREOPTIONS(1 3) UNIQUE -
                  VOLUMES(WORK01)) -
         DATA   (NAME(IBMUSER.OFTPD.TRCVR.KSDS.DATA)) -
         INDEX  (NAME(IBMUSER.OFTPD.TRCVR.KSDS.INDEX))
/*
//PDS      EXEC PGM=IEFBR14,COND=(0,NE,DEFINE)
//D1       DD  DSN=IBMUSER.OFTPD.TRCVR.PDS,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120,DSORG=PO)
//RUN      EXEC PGM=TSTRCVR,PARM='RUN',COND=(0,NE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//OFTPSPL  DD  DISP=OLD,DSN=IBMUSER.OFTPD.TRCVR.SPOOL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//CLEAN    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.OFTPD.TRCVR.SPOOL CLUSTER PURGE
  DELETE IBMUSER.OFTPD.TRCVR.KSDS CLUSTER PURGE
  DELETE IBMUSER.OFTPD.TRCVR.PDS
/*
