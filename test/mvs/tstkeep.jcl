//TSTKEEP  JOB (OFTP),'KEEPDAYS TEST',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* The daily KEEPDAYS run, seen on MVS. Two submissions around a start
//* of OFTPD (KEEPDAYS=7, the default; the seeds' ages are chosen for it):
//*
//*   1. P OFTPD                       the spool is OFTPD's: stop it
//*   2. submit with PARM='SEED'       old and young transfers go in,
//*                                    their last-change times written
//*   3. S OFTPD                       the first tick is the daily run;
//*                                    OFTP235I ... DELETED BY KEEPDAYS
//*                                    must be on the log
//*   4. P OFTPD, then PARM='CHECK'    the old finished ones are gone, the
//*                                    young and the unfinished are not
//*
//* Edit the spool name (the S= of the OFTPD PROC) and the test library
//* (the one `mbt test --mvs` deployed). Do not use PURGE to make the
//* deletion happen: it logs the name of whoever asked, never BY KEEPDAYS.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTKEEP,PARM='SEED'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//OFTPSPL  DD  DISP=OLD,DSN=OFTP.SPOOL
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
