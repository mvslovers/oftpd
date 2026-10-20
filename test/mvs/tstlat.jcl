//TSTLAT   JOB (OFTP),'REQUEST TIME',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTLAT against a RUNNING OFTPD: how long the requests of the dialog
//* take over the SVC (internals/xmem.md section 17).
//*
//* Before submitting:
//*   - OFTPD started with SVC=nnn (OFTP039I on the console) and the
//*     partner below in OFTPPTN.
//*   - Edit the test library, the partner and the data set the job may
//*     read; the last two words of PARM are the largest number of held
//*     sends queued (and deleted again) and a label for the output.
//*   - The STEPLIB must NOT be authorized, as for TSTSVC.
//*
//* For a measurement on a loaded system submit tstlod.jcl first and let
//* it run while this job does.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTLAT,PARM='RUN,PARTNER1,IBMUSER.OFTPD.SAMPLE,200,IDLE'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
