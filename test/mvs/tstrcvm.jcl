//TSTRCVM1 JOB (OFTP),'RCV RULES 1',CLASS=A,MSGCLASS=H
//* ------------------------------------------------------------------
//* TSTRCVM: receive rules (#44) against two RUNNING OFTPDs. Set up with
//* tstrcvmd.jcl (job TSTRCVMS once; then start TSTRCVMA and TSTRCVMB,
//* and look for OFTP024I on B: the rules were read). The jobs, in
//* order:
//*
//*   TSTRCVM1  rule set 1: pattern, date, replace, refused replace,
//*             no rule, invalid name, REPLACE=Y onto a PDS and onto a
//*             VSAM cluster (refused, SFNA 12); then what B made, listed
//*   TSTRCVW2  writes rule set 2 into the member
//*   -----     F TSTRCVMB,REFRESH   (OFTP024I: 5 rules)
//*   TSTRCVM2  the second set decides
//*   TSTRCVW3  writes a member with a syntax error
//*   -----     F TSTRCVMB,REFRESH   (OFTP010W, OFTP025E: not used)
//*   TSTRCVM3  the second set is still in force
//*
//* The userid that runs them must be in ADMIN= of OFTPD A. The test
//* library must not be APF-authorized (the point of the transport).
//* Change the library names.
//* ------------------------------------------------------------------
//* what an earlier run left (the day's REPLACE=Y date name is replaced)
//OLD      EXEC PGM=IEFBR14
//D1       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.TSTA.ONE,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D2       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.REP.ONE,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D3       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.NOR.ONE,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D4       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.NEW.TWO,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D5       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.NEW.THREE,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D6       DD  DISP=(MOD,DELETE),DSN=IBMUSER.OFTPBIN.TSTRCV.NONE.ONE,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D7       DD  DISP=(MOD,DELETE),DSN=IBMUSER.OFTPBIN.TSTRCV.REP.TWO,
//             UNIT=SYSDA,SPACE=(TRK,1)
//D8       DD  DISP=(MOD,DELETE),DSN=IBMUSER.RCVTEST.REP.BUSY,
//             UNIT=SYSDA,SPACE=(TRK,1)
//* the data set the REPLACE=Y rule will not be able to replace: it is
//* a copy of the first file, and the next step holds it
//SEED     EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  DISP=SHR,DSN=IBMUSER.TSTRCVM.D1
//SYSUT2   DD  DSN=IBMUSER.RCVTEST.REP.BUSY,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(TRK,1),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=3120)
//RUN      EXEC PGM=TSTRCVM,PARM='RUN 1'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//BUSY     DD  DISP=OLD,DSN=IBMUSER.RCVTEST.REP.BUSY
//* RC 0: the data set that could not be replaced still holds the
//* first file (RC 8 would mean it was changed)
//CMP      EXEC PGM=IEBCOMPR
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  DISP=SHR,DSN=IBMUSER.TSTRCVM.D1
//SYSUT2   DD  DISP=SHR,DSN=IBMUSER.RCVTEST.REP.BUSY
//SYSIN    DD  *
         COMPARE TYPORG=PS
/*
//* the PDS a REPLACE=Y rule would not rename is as it was (RC 0)
//CMPPDS   EXEC PGM=IEBCOMPR
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  DISP=SHR,DSN=IBMUSER.RCVTEST.PDS.ONE
//SYSUT2   DD  DISP=SHR,DSN=IBMUSER.RCVTEST.PDS.ONE.COPY
//SYSIN    DD  *
         COMPARE TYPORG=PO
/*
//* the data sets B made, where they are, and that no backup or
//* temporary name is left behind
//* the last of the three is NOT FOUND (RC 4 of the step): the temporary
//* names, and the backup of a REPLACE=Y, are gone again
//LIST     EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT LEVEL(IBMUSER.RCVTEST) VOLUME
  LISTCAT LEVEL(IBMUSER.OFTPBIN) VOLUME
  LISTCAT LEVEL(IBMUSER.OFTPBTMP) VOLUME
/*
//*
//TSTRCVW2 JOB (OFTP),'RCV RULES W2',CLASS=A,MSGCLASS=H
//W        EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  *
* rule set 2: PAT has another pattern, the others are gone
RULE=PAT    FROM=TSTA VFN=TSTRCV.PAT.*  DSN=IBMUSER.RCVTEST.NEW.%Q3
RULE=XYZ    VFN=TSTRCV.XYZ.*  DSN=IBMUSER.RCVTEST.XYZ.%Q3
/*
//SYSUT2   DD  DISP=SHR,DSN=IBMUSER.TSTRCVM.RULES
//*
//TSTRCVM2 JOB (OFTP),'RCV RULES 2',CLASS=A,MSGCLASS=H
//RUN      EXEC PGM=TSTRCVM,PARM='RUN 2'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//*
//TSTRCVW3 JOB (OFTP),'RCV RULES W3',CLASS=A,MSGCLASS=H
//W        EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  *
* rule set 3: a rule with an invalid name, a rule for no partner
RULE=PAT    FROM=TSTA VFN=TSTRCV.PAT.*  DSN=IBMUSER.RCVTEST.%Q3.%NOSUCH
BOGUS=1
RULE=XYZ    FROM=NOBODY  VFN=TSTRCV.XYZ.*  DSN=IBMUSER.RCVTEST.XYZ
/*
//SYSUT2   DD  DISP=SHR,DSN=IBMUSER.TSTRCVM.RULES
//*
//TSTRCVM3 JOB (OFTP),'RCV RULES 3',CLASS=A,MSGCLASS=H
//RUN      EXEC PGM=TSTRCVM,PARM='RUN 3'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
