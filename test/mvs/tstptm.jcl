//TSTPM1   JOB (OFTP),'PARTNER MAINT A',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTPTM: the partner table maintained by commands, against two
//* RUNNING OFTPDs (test/mvs/tstptmd.jcl starts them; the spools must
//* exist). Four jobs, one after the other:
//*
//*   TSTPM1  adds, changes, enables the partner TSTPTM, sends a file to
//*           it, tries to delete it. The partner is OFTPD B, reached on
//*           127.0.0.1 port 3315 (change them to where B listens). Then
//*           the parmlib partners (P1), the member loses GONEPTN, and
//*           REFRESH takes it away (P2). Submit tstptmd.jcl's first
//*           job (TSTPTMP) once before the first start of A.
//*   TSTPM2  the OFTP utility: PARTNER LIST, SHOW, ENABLE, CHANGE and
//*           DISABLE, in batch; it leaves SESSIONS=2 and the partner
//*           disabled
//*   -----   now stop OFTPD A (P TSTPTMA, until OFTP003I) and start it
//*           again (submit tstptmd.jcl's first job)
//*   TSTPM3  the partner is still there, as changed; delete the
//*           transfer, then the partner
//*   -----   now stop OFTPD A again and start the job TSTPTMN of
//*           tstptmd.jcl in its place: ADMIN= is somebody else
//*   TSTPM5  ADD, CHANGE, DELETE and REFRESH are refused (507)
//*   TSTPM4  the file B received is the file that was sent
//*
//* The userid that runs them must be in ADMIN= of OFTPD A. The test
//* library must not be APF-authorized: that is the point of the
//* transport, and TSTPTM fails when it is. Change the library names.
//* ------------------------------------------------------------------
//DATA     EXEC PGM=IEFBR14
//OLD      DD  DISP=(MOD,DELETE),DSN=IBMUSER.TSTPTM.DATA,
//             UNIT=SYSDA,SPACE=(TRK,1)
//MAKE     EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  *
THE PARTNER WAS ADDED BY A COMMAND, NOT BY PARMLIB
LINE 2
LINE 3
/*
//SYSUT2   DD  DSN=IBMUSER.TSTPTM.DATA,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,1),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//RUN      EXEC PGM=TSTPTM,
//         PARM='RUN A 127.0.0.1 3315 IBMUSER.TSTPTM.DATA'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//PARM1    EXEC PGM=TSTPTM,PARM='RUN P1'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//* the member without GONEPTN; A's OFTPPTN DD reads it afresh at REFRESH
//REWRITE  EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  *
PARTNER=PARMPTN ODETTEID=O0000000000PARM PORT=1 CALL=N
/*
//SYSUT2   DD  DSN=IBMUSER.TSTPTM.PTN,DISP=SHR
//PARM2    EXEC PGM=TSTPTM,PARM='RUN P2'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//*
//TSTPM2   JOB (OFTP),'PARTNER UTILITY',CLASS=A,MSGCLASS=H
//OFTP     EXEC PGM=OFTP
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
PARTNER LIST
PARTNER SHOW TSTPTM
PARTNER ENABLE TSTPTM
PARTNER SHOW TSTPTM
PARTNER CHANGE TSTPTM SESSIONS(2) PASSWORD(SECRET)
PARTNER DISABLE TSTPTM
PARTNER SHOW TSTPTM
/*
//*
//TSTPM3   JOB (OFTP),'PARTNER MAINT B',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTPTM,PARM='RUN B'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//*
//TSTPM5  JOB (OFTP),'PARTNER NOT ADMIN',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTPTM,PARM='RUN C'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//*
//TSTPM4   JOB (OFTP),'PARTNER FILE',CLASS=A,MSGCLASS=H
//* the data set B received is named INPREFIX.<virtual file name>
//LIST     EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT LEVEL(IBMUSER.OFTPBIN) ALL
/*
