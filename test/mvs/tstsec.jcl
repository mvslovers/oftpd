//TSTSEC   JOB (OFTP),'NO ACEE',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTSEC -- oftpd#54: a send from a caller without an ACEE, and on a
//* system without a security product.
//*
//* THE JOB MUST RUN AUTHORIZED: the test clears ASXBSENV of its
//* own address space for the length of one SVC. TSTSVC is the opposite
//* and must not be authorized.
//*
//* THROWAWAY CONTAINER ONLY. With PARM item NOSEC (or HANG) the program
//* clears CVTRAC, the security product's anchor, of the WHOLE SYSTEM for
//* the length of one SVC; whoever can start it from an authorized
//* library can switch security off. So: put TSTSEC in a library of its
//* own (the one `mbt test --mvs` deploys to) and let the program ask for
//* authorization through SVC 244 as OFTPD does (TK5 with RAKF: the way
//* it was measured). An APF-authorized library was not tried: the
//* module has writable data, which mbt warns would be key-0 storage
//* when fetched that way. Never put it in LNKLST, SYS2.LINKLIB or any
//* library of a stand. An ESTAE puts CVTRAC back after an abend or a
//* CANCEL; the call has no timeout, so a hang nobody cancels leaves it
//* cleared until the next IPL.
//*
//* Needs a running OFTPD with SVC=, a partner (PARM, second item), and
//* ADMIN= in OFTPPRM naming the userid this job runs under, so that the
//* held test sends can be deleted again. The data set (third item) is a
//* sequential data set that userid may read; step 1 makes one (edit the
//* high-level qualifier here and in PARM).
//*
//* Fourth PARM item: none (a product is there), NOSEC (the calls without
//* an ACEE also run without CVTRAC), HANG (needs an OFTPD built with
//* -DOFTP_DEBUG_SLOW: one call with CVTRAC cleared that OFTPD holds for a
//* minute; CANCEL the job, then run TSTSEC with PARM='CHECK', which fails
//* if CVTRAC is zero).
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
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
