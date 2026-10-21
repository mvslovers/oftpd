//TSTMEM   JOB (OFTP),'PARTNER MEMBER',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTMEM against a RUNNING OFTPD (SVC= in OFTPPRM, OFTP039I on the
//* console). Give PARM the data set and member that the OFTPPTN DD of
//* that OFTPD names, for example PARM='RUN SYS1.PARMLIB(OFTPPTN0)'.
//* Change the test library to the one `mbt test --mvs` deployed; like
//* TSTSVC it needs no authority.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTMEM,PARM='RUN SYS1.PARMLIB(OFTPPTN0)'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
