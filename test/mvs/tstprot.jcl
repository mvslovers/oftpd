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
//*
//* IT IS NOT THE TESTLIB. The library `mbt test --mvs` deploys to is
//* not in IEAAPF00, by construction, so TSTPROT run from there fails
//* its own first check every single time: a correct guard and no
//* measurement. Copy the module into an authorized library for this
//* one job. The opposite holds for TSTSVC, which must run from the
//* TESTLIB precisely because it is unauthorized -- the two jobs want
//* opposite libraries and each one fails loudly in the other's.
//* ------------------------------------------------------------------
//RUN      EXEC PGM=TSTPROT,PARM='RUN'
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
