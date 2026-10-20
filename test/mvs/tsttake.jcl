//TSTTAKE  JOB (OFTP),'CSA SLOT WIPE',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTTAKE -- what oftp_svc_take() leaves in a request slot of the
//* CSA anchor. Needs no OFTPD. THE STEPLIB MUST BE APF-AUTHORIZED
//* (oftp_svc_take() switches to key 0), like TSTPROT's: replace
//* APF.TEST.LINKLIB below with a library that is in IEAAPF00 on its own
//* volume and used for tests only, and copy the module there. The
//* module is AC(1): it does not belong in a production APF library.
//* Copy it with replace (IEBCOPY SELECT MEMBER=((TSTTAKE,,R))): without
//* R an older copy stays and the test runs the wrong code.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTTAKE,PARM='RUN'
//STEPLIB  DD  DISP=SHR,DSN=APF.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
