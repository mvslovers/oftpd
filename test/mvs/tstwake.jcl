//TSTWAKE  JOB (OFTP),'SVC WAKE ON NOT OWN',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTWAKE -- a caller of the OFTPSVC routine that wakes on a slot
//* another caller has since claimed (#124). It installs OFTPSVC under
//* an SVC number of its own and plays OFTPD itself, so NO OFTPD may be
//* running under that number. THE STEPLIB MUST BE APF-AUTHORIZED
//* (the test switches to key 0) and must hold TSTWAKE and OFTPSVC:
//* replace APF.TEST.LINKLIB below with a library that is in IEAAPF00 on
//* its own volume and used for tests only. TSTWAKE is AC(1) and does
//* not belong in a production APF library. Copy both members with
//* replace (IEBCOPY SELECT MEMBER=((TSTWAKE,,R),(OFTPSVC,,R))): without
//* R an older copy stays and the test runs the wrong code.
//* PARM='RUN' looks for a free SVC from 255 down; PARM='RUN 241' takes that one.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTWAKE,PARM='RUN'
//STEPLIB  DD  DISP=SHR,DSN=APF.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
