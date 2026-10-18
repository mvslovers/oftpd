//TSTSVC   JOB (OFTP),'XMEM TRANSPORT',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTSVC against a RUNNING OFTPD.
//*
//* Before submitting:
//*   - OFTPD must be started and must have SVC=nnn in its OFTPPRM
//*     member. Look for OFTP030I and then OFTP039I on the console:
//*     the first says the transport is installed, the SECOND says it
//*     is active. Only OFTP039I means this job can do anything.
//*   - Edit the test library name below (the one `mbt test --mvs`
//*     deployed).
//*
//* The STEPLIB is deliberately NOT APF-authorized and this job needs
//* no authority of any kind. That is the property under test: an
//* ordinary program reaches OFTPD through the SVC. Do not "fix" a
//* failure by authorizing it -- the job now checks __isauth() itself
//* and FAILS when it is authorized, because such a run would pass
//* every other assertion while demonstrating nothing. The library
//* mbt test --mvs creates is not in IEAAPF00, so the default is
//* already right; this only stops someone moving the job.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTSVC,PARM='RUN'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
