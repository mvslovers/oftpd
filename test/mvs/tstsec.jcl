//TSTSEC   JOB (OFTP),'NO ACEE',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTSEC -- oftpd#54: a send from a caller without an ACEE, and on a
//* system without a security product.
//*
//* THE JOB MUST RUN AUTHORIZED: the test clears ASXBSENV of its
//* own address space for the length of one SVC. TSTSVC is the opposite
//* and must not be authorized; copy TSTSEC into an authorized library
//* (SYS2.LINKLIB on a TK5 container) and name that below.
//*
//* Needs a running OFTPD with SVC=, a partner (PARM, second item), and
//* ADMIN= in OFTPPRM naming the userid this job runs under, so that the
//* held test sends can be deleted again. The data set (third item) is a
//* sequential data set that userid may read; step 1 makes one (edit the
//* high-level qualifier here and in PARM).
//* ------------------------------------------------------------------
//MAKE     EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  *
OFTP SEC TEST RECORD
/*
//SYSUT2   DD  DSN=HERC01.OFTPSEC.DATA,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//RUN      EXEC PGM=TSTSEC,
//             PARM='RUN,PARTNER1,HERC01.OFTPSEC.DATA'
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
