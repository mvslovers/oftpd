//TSTSVCR  JOB (OFTP),'XMEM FREED BUF',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTSVCR against a RUNNING OFTPD that was built with
//* -DOFTP_DEBUG_SLOW (a debug build, never shipped: put cflags =
//* ["-DOFTP_DEBUG_SLOW"] under [build] in mbt.toml for the deploy, and take
//* it out again). A normal OFTPD
//* answers at once, and the test then FAILS: that is how it says it is
//* not a SLOW build.
//*
//* A subtask of this job frees the reply buffer while OFTPD holds the
//* request, and the answer is then copied out into storage that is no
//* longer there (internals/xmem.md section 6).
//*
//* Edit the test library name below (the one `mbt test --mvs`
//* deployed). The STEPLIB must NOT be APF-authorized, as for TSTSVC.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTSVCR,PARM='SLOW'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
