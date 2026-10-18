//TSTPROT  JOB (OFTP),'KEY-8 VS SP241',CLASS=A,MSGCLASS=H,
//         MSGLEVEL=(1,1)
//* ------------------------------------------------------------------
//* TSTPROT -- oftpd#15: may a key-8 program read subpool 241?
//*
//* THE STEPLIB MUST BE APF-AUTHORIZED. TPROT is a privileged
//* instruction, so an unauthorized run gets S0C2 and measures
//* nothing. The job checks __isauth() first and FAILS rather than
//* reporting itself skipped, because a job that passes because it
//* was not allowed to look is worse than no job.
//*
//* It writes nothing: no data sets, no system members, no operator
//* commands. TPROT reports in the condition code and never faults,
//* which is why this is a measurement and not an experiment.
//*
//* OFTPD is NOT required. If one is running with SVC= set, the job
//* also probes its live anchor through the SSCT as a confirmation on
//* the object the client really reads; otherwise it says that probe
//* was skipped and carries on.
//*
//* Edit the library name. It must be one that appears in IEAAPF00
//* with the volume it actually sits on -- a library that is present
//* but on a different volume is not authorized, and the job will say
//* so through its first check rather than abending.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTPROT,PARM='RUN'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.OFTPD.V0R1M0D.TESTLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
