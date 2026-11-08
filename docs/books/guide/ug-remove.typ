#import "../bookmaster/bookmaster.typ": *

= Removing OFTPD <ug-remove>

#idx("removal")
#idx("uninstall")
This chapter takes OFTPD off an MVS 3.8j system and frees its FMID, so that it
can be installed again. The main way lets SMP do the work: a dummy function,
#cmd("TOFTDEL"), deletes every release of OFTPD, and the load modules leave both
libraries with it. A way that uses UCLIN alone stays as the fallback
(@ug-remove-uclin). If you want to move to a newer release, you are in the
wrong chapter: a release deletes its predecessor when it is installed, and the
last step below would scratch the libraries that the new release installs into.

== What OFTPD Put on the System <ug-remove-what>

#tab(caption: [What release 1.0.0 installed])[
  #table(columns: (1.6in, 1fr),
    [Item], [Name],
    [FMID], [#cmd("TOFT100")],
    [Load modules], [#cmd("OFTPD"), #cmd("OFTP"), #cmd("OFTPI"), #cmd("OFTPSVC")],
    [Target library], [#cmd("OFTPD.LINKLIB")],
    [Distribution library], [#cmd("OFTPD.AOFTPLOD")],
    [Sample library], [#cmd("OFTPD.SAMPLIB")],
    [Panel library], [#cmd("OFTPD.ISPPLIB")],
  )
]

The prerelease 0.1.0-dev put #cmd("TOFT010") on the system, with the three load
modules #cmd("OFTPD"), #cmd("OFTP") and #cmd("OFTPI"), into the same libraries.
The staging library #cmd("OFTPD.OFTPLOAD") is not on the list, because the
accept job scratches it. Until the accept job has run it is there, with the four
load modules in it, and step 5 scratches it too.

*The install job ends after the APPLY, and the accept job follows it.* That
gives the system two states, and this chapter works in both. In the first,
applied and not accepted, the install job has run and the accept job has not\:
#cmd("TOFT100") is #cmd("REC APP") in the target zone (CDS) and not in the
distribution zone (ACDS), and #cmd("OFTPD.AOFTPLOD") is empty. In the second,
accepted, the accept job has run\: #cmd("TOFT100") is #cmd("REC APP ACC") in the
CDS and #cmd("REC APP ACC RGN") in the ACDS, and #cmd("OFTPD.AOFTPLOD") holds the
four modules. A #cmd("RESTORE") is possible in the first state and not in the
second (@ug-remove-why).

*An earlier release leaves its FMID behind.* Release 1.0.0 deletes
#cmd("TOFT010") (its #cmd("++VER") reads #cmd("DELETE(TOFT010)")), and SMP
leaves the id of the deleted release in the inventory as a mark, in both zones:

#screen(```
TOFT010   TYPE            = FUNCTION
          DELBY           = TOFT100
```)

The mark is there on a system that went from the prerelease to 1.0.0, and on a
system that installed 1.0.0 new and never had the prerelease. Deleting the id of
the current release clears only that one. Both ways below therefore name
every id that OFTPD has used (#cmd("TOFT010") and #cmd("TOFT100")), and you need
not know which releases the system has seen. The jobs are written for releases
up to 1.0.0. A later release adds its own id to every list in this chapter.

== Step 1: Stop the Monitor <ug-remove-stop>

#screen(```
P OFTPD
```)

Wait until the task has ended, and make sure that no batch job or TSO session is
still using #cmd("OFTPD.LINKLIB"). The monitor gives its SVC entry back when it
ends. If the log says #cmd("OFTP036W") or #cmd("OFTP037W"), it has left the
routine and the anchor in common storage, and that storage stays allocated until
the next IPL (@ops-recover). That does not change anything below.

== Step 2: Delete the Release with the Dummy Function <ug-remove-dummy>

#idx("TOFTDEL")
#idx("dummy function")
SMP deletes a function when a later function names it in a #cmd("DELETE")
operand, and it removes the load modules of the deleted function from the
libraries on the way. The job below receives, applies and accepts such a
function. #cmd("TOFTDEL") has no elements. It deletes all OFTPD ids up to 1.0.0
in one #cmd("DELETE") statement.

*#cmd("TOFTDEL") is not a release FMID.* No release of OFTPD ships it, the job
brings it in by itself, and step 3 frees it again in the same flow. Do not use
it for anything else, and do not keep it on the system.

Two things in the job matter:

- The #cmd("APPLY") and the #cmd("ACCEPT") step each carry a
  #cmd("HMASMP.LINKLIB") and a #cmd("HMASMP.AOFTPLOD") statement. SMP deletes
  the modules from the libraries that these DD names point to. Without
  #cmd("HMASMP.AOFTPLOD"), the #cmd("ACCEPT") ends with return code 12 and the
  message #cmd("HMA2833 ** AOFTPLOD DDCARD MISSING"). Without
  #cmd("HMASMP.LINKLIB"), the #cmd("APPLY") still ends with return code 04 and
  prints the same #cmd("HMA2240") lines, but it deletes from the library that
  the procedure names itself (#cmd("SYS1.LINKLIB") on the systems measured), and
  #cmd("OFTPD.LINKLIB") keeps its members. The member list of the last step is
  the only proof. The statements come after the #cmd("EXEC") statement and
  before #cmd("SMPCNTL"), as in the install job.
- The later steps run only on #cmd("COND=(4,LT,...)"). The #cmd("RECEIVE") ends
  with return code 04, because a function without elements raises
  #cmd("HMA3971"). With the usual #cmd("COND=(0,NE,...)") the job would stop
  right after the receive.

Submit this job:

#screen(```
//OFTPDDEL JOB (SYS),'OFTPD DELETE',
//             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//RECV    EXEC SMPREC
//SMPPTFIN DD  *
++FUNCTION(TOFTDEL) .
++VER(Z038) DELETE(TOFT010,TOFT100) .
/*
//SMPCNTL  DD  *
 RESETRC .
 RECEIVE SELECT(TOFTDEL) .
/*
//APPLY   EXEC SMPAPP,COND=(4,LT,RECV.HMASMP)
//HMASMP.LINKLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//HMASMP.AOFTPLOD DD  DISP=SHR,DSN=OFTPD.AOFTPLOD
//SMPCNTL  DD  *
 APPLY S(TOFTDEL) DIS(WRITE) .
/*
//ACCEPT  EXEC SMPAPP,COND=(4,LT,APPLY.HMASMP)
//HMASMP.LINKLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//HMASMP.AOFTPLOD DD  DISP=SHR,DSN=OFTPD.AOFTPLOD
//SMPCNTL  DD  *
 ACCEPT S(TOFTDEL) DIS(WRITE) .
/*
//MEMBERS EXEC PGM=IKJEFT01
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
 LISTDS 'OFTPD.LINKLIB' MEMBERS
 LISTDS 'OFTPD.AOFTPLOD' MEMBERS
/*
//
```)

Read the three SMP steps one by one. The receive ends with return code 04:

#screen(raw(read("../ex/ug-remove/dummy-recv.txt")))

The apply deletes the four load modules from the target library. It also ends
with return code 04, because SMP does not find #cmd("TOFT100") in the restore
data set (#cmd("HMA2461")). On a system that has only the prerelease, the id in
that message is #cmd("TOFT010"), and the apply deletes three load modules:

#screen(raw(read("../ex/ug-remove/dummy-apply.txt")))

The accept deletes the four copies from the distribution library and ends with
return code 00:

#screen(raw(read("../ex/ug-remove/dummy-accept.txt")))

*The result is in the last step, not in the return codes.* Both libraries must
list no member. Everything else is the proof that the job ran, not that the work
happened:

#screen(raw(read("../ex/ug-remove/members-empty.txt")))

If a step ends higher than 04, the following #cmd("APPLY") or #cmd("ACCEPT")
step does not run\; the last step does, and shows what is still there. Read the
output of the failed step\; do not submit the whole job again before you know
what it did. An #cmd("ACCEPT") that ended with return code 12 on
#cmd("HMA2833") has still rewritten the ACDS, but the distribution library
still holds its modules. Submitting the #cmd("ACCEPT") step alone, with both
#cmd("HMASMP") statements, ends with return code 00 and #cmd("HMA2270"), but
deletes only the modules after the first one. On the 1.0.0-dev package it
deletes #cmd("OFTPD"), #cmd("OFTPI") and #cmd("OFTPSVC")\; #cmd("OFTP"), the
first module in the order of the #cmd("HMA2240") messages, stays in
#cmd("OFTPD.AOFTPLOD"), where SMP no longer knows it. On the prerelease the
first module stays as well. That return code 00 does not mean the library is
empty: list it as shown above. Scratch the library with step 5 if OFTPD goes
for good.

*An applied, not accepted system gives a different output*, and the job removes
OFTPD there as well. Measured on a TK5 container after the install job of the
1.0.0 package as a new install, the steps of the job above ended like this\:

- #cmd("RECV") (the #cmd("RECEIVE") of #cmd("TOFTDEL"))\: return code 04, as
  above.
- #cmd("APPLY") (the #cmd("APPLY") of #cmd("TOFTDEL"))\: return code 00. The
  message #cmd("HMA2461") of the accepted case does not appear\; the step prints
  the four #cmd("HMA2240") lines for #cmd("LINKLIB").
- #cmd("ACCEPT") (the #cmd("ACCEPT") of #cmd("TOFTDEL"))\: return code 00,
  without any #cmd("HMA2240") line, because #cmd("OFTPD.AOFTPLOD") holds nothing
  to delete.

The job ends with condition code 0004, from #cmd("RECV"). Both libraries list no
member, and #cmd("OFTPD.OFTPLOAD")
is still there (step 5 removes it). Steps 3 and 4 gave the same as in this
chapter. This state after an upgrade from the prerelease, where the distribution
library holds the three modules of 0.1.0, was not run through this job.

*What has been measured.* The job was run on test systems (MVS/CE) in two
states: with the prerelease 0.1.0-dev alone, and after the upgrade from the
prerelease to 1.0.0-dev, the build of the tree before the release. The recovery
after a failed #cmd("ACCEPT") was measured on the prerelease and on a new
install of the 1.0.0-dev package. On TK5 containers it was run again with the
1.0.0 package that mbt 3.0.1 builds, as a new install, in the applied-only
state above and after the accept job. A system with more than one product in the
same zones, a system with other service on it, and a dummy function with a
#cmd("++MAC") element instead of none have not been measured. Whatever the
system, the #cmd("LIST") in step 4 is the proof.

== Step 3: Free the Ids <ug-remove-free>

#idx("UCLIN")
#idx("FMID", "removal")
The dummy function has not made the ids free. It has left #cmd("TOFTDEL") itself
in both zones, with the status #cmd("REC APP ACC") (the ACDS adds #cmd("RGN")),
and a mark
#cmd("DELBY = TOFTDEL") on every id it deleted. This job removes the three
entries from the CDS (what is applied) and the ACDS (what is accepted). It
changes the two SMP data bases and no library:

#screen(```
//OFTPUCL  JOB (SYS),'OFTPD UNINSTALL',
//             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//UCLCDS  EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN CDS .
  DEL SYSMOD(TOFTDEL) .
  DEL SYSMOD(TOFT100) .
  DEL SYSMOD(TOFT010) .
 ENDUCL .
/*
//UCLACDS EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN ACDS .
  DEL SYSMOD(TOFTDEL) .
  DEL SYSMOD(TOFT100) .
  DEL SYSMOD(TOFT010) .
 ENDUCL .
/*
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS  SYSMOD(TOFTDEL) .
 LIST ACDS SYSMOD(TOFTDEL) .
 LIST CDS  SYSMOD(TOFT100) .
 LIST ACDS SYSMOD(TOFT100) .
 LIST CDS  SYSMOD(TOFT010) .
 LIST ACDS SYSMOD(TOFT010) .
/*
//
```)

Every #cmd("DEL") finds its entry, because the dummy function left a mark for
every id it named. That includes an id that the system never had: on a system
that had only the prerelease, #cmd("TOFT100") was marked as well, and the
#cmd("DEL") found it. Both #cmd("UCLIN") steps end with return code 00, and each
#cmd("DEL") reports #cmd("HMA2550 UPDATE COMPLETE"). This is the output of the
step #cmd("UCLCDS"):

#screen(raw(read("../ex/ug-remove/uclcds-free.txt")))

The step #cmd("UCLACDS") gives the same. Any other message is not expected.

== Step 4: Read the LIST <ug-remove-list>

The last step of the job is the result. For every id, in both zones, it must
answer that the entry was not found. These are the first two of the six
statements, as the step printed them:

#screen(raw(read("../ex/ug-remove/list-gone.txt")))

and the step must end with the highest return code 04. Return code 04 together
with this answer means that the FMID is free, and #cmd("TOFTDEL") with it. Both
zones count: an id gone from one and still in the other is not free.

*Read the stanzas, not the return code of the step.* A mark that is still there
answers with return code 00, so it does not raise the return code of the step
above the 04 that you expect. The job has six #cmd("LIST") statements, and all
six must show the id as not found. A stanza that names #cmd("TYPE = FUNCTION")
is an entry that is still there, whether it shows a status such as
#cmd("REC APP ACC") or only a #cmd("DELBY") of its successor. This is what the
statements #cmd("LIST CDS SYSMOD(TOFT100)") and
#cmd("LIST CDS SYSMOD(TOFTDEL)") print after step 2, before step 3 has run:

#screen(raw(read("../ex/ug-remove/list-present.txt")))

A mark that was there before stays as it was: on a system that has 1.0.0,
#cmd("TOFT010") still reads #cmd("DELBY = TOFT100") after the dummy function,
not #cmd("DELBY = TOFTDEL"). The #cmd("DEL") of step 3 clears it all the same.

== Step 5: Scratch the Libraries <ug-remove-scratch>

After step 2 the two libraries are empty, but they still exist. The allocation
job of the installation creates both with #cmd("DISP=(NEW,CATLG,DELETE)"). The
job of 1.0.0 looks in the catalog first, and its header says that it ends with
return code 8 and allocates nothing when a library is cataloged (that was not
run here). An allocation job without that look, run against libraries that
still existed, ended with return code 00, reported
#cmd("IEF287I") with #cmd("NOT CATLGD") for both, and left uncataloged
duplicates behind. Delete the libraries in a job of your own before you install
again:

#screen(```
  DELETE OFTPD.LINKLIB  NONVSAM SCRATCH PURGE
  DELETE OFTPD.AOFTPLOD NONVSAM SCRATCH PURGE
```)

Do this only when you remove OFTPD for good. After an upgrade, these are the
data sets that the next release installs into; leave them.
#cmd("OFTPD.SAMPLIB") and #cmd("OFTPD.ISPPLIB") may stay. The install job
scratches and receives them again the next time.

If the accept job has not run, the staging library #cmd("OFTPD.OFTPLOAD") is
still there, and neither the dummy function nor #cmd("UCLIN") removes it\: SMP
does not own it. Scratch it too when OFTPD goes for good\:

#screen(```
  DELETE OFTPD.OFTPLOAD NONVSAM SCRATCH PURGE
```)

Measured on a TK5 container after the dummy function on an applied, not
accepted system\: IDCAMS ends with return code 0 and the library is gone. After
the accept job the same #cmd("DELETE") ends with return code 8, because the
accept job has scratched the library already, so you do not need it then. The
install job scratches the library itself when it runs again (step
#cmd("DELOLD")).

== The Fallback: UCLIN Alone <ug-remove-uclin>

#idx("UCLIN")
Use this way when the dummy function cannot be used. After step 1, submit this
job instead of steps 2 and 3. It changes the two SMP data bases and no library:

#screen(```
//OFTPUCL  JOB (SYS),'OFTPD UNINSTALL',
//             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//UCLCDS  EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN CDS .
  DEL SYSMOD(TOFT100) MOD(OFTP) .
  DEL SYSMOD(TOFT100) MOD(OFTPD) .
  DEL SYSMOD(TOFT100) MOD(OFTPI) .
  DEL SYSMOD(TOFT100) MOD(OFTPSVC) .
  DEL MOD(OFTP) .
  DEL MOD(OFTPD) .
  DEL MOD(OFTPI) .
  DEL MOD(OFTPSVC) .
  DEL LMOD(OFTP) .
  DEL LMOD(OFTPD) .
  DEL LMOD(OFTPI) .
  DEL LMOD(OFTPSVC) .
  DEL SYSMOD(TOFT100) .
  DEL SYSMOD(TOFT010) .
 ENDUCL .
/*
//UCLACDS EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN ACDS .
  DEL SYSMOD(TOFT100) MOD(OFTP) .
  DEL SYSMOD(TOFT100) MOD(OFTPD) .
  DEL SYSMOD(TOFT100) MOD(OFTPI) .
  DEL SYSMOD(TOFT100) MOD(OFTPSVC) .
  DEL MOD(OFTP) .
  DEL MOD(OFTPD) .
  DEL MOD(OFTPI) .
  DEL MOD(OFTPSVC) .
  DEL SYSMOD(TOFT100) .
  DEL SYSMOD(TOFT010) .
 ENDUCL .
/*
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS  SYSMOD(TOFT100) .
 LIST ACDS SYSMOD(TOFT100) .
 LIST CDS  SYSMOD(TOFT010) .
 LIST ACDS SYSMOD(TOFT010) .
/*
//
```)

Every #cmd("DEL") that finds its entry reports #cmd("HMA2550 UPDATE COMPLETE").
An entry that is not in the zone is reported as #cmd("HMA2532") and
#cmd("HMA2560"), and the step ends with return code 08. That is expected for
what your system never had\; the statements after it are still processed. It is
no sign that a line did what you wanted, and a step that ends with return code
08 can also hide a statement that failed for another reason. Read every message,
and take the #cmd("LIST") of step 4 as the only proof.

On a system that installed 1.0.0 new, every #cmd("DEL") finds its entry, because
#cmd("TOFT100") named #cmd("TOFT010") and left a mark for it. Both steps end
with return code 00. On a system that has only the prerelease, #cmd("TOFT100")
and the module #cmd("OFTPSVC") were never in the zone. This is the output of the
step #cmd("UCLCDS") there:

#screen(raw(read("../ex/ug-remove/uclcds.txt")))

The step #cmd("UCLACDS") gives the same, without the #cmd("LMOD") statements.
Any other message is not expected.

The two zones have a step each for a reason. After a step has ended with return
code 08, SMP does not execute the next #cmd("UCLIN") of the same step: it
answers #cmd("HMA2293 ** CONTROL STATEMENT IGNORED DUE TO PREVIOUS ERROR") and
does nothing. With both blocks in one step, the entries of a system that has
only the prerelease would leave the ACDS untouched.

Then read the #cmd("LIST") as in step 4, here four statements without
#cmd("TOFTDEL"), and, if you remove OFTPD for good, scratch the libraries as in
step 5. #cmd("UCLIN") changes only the inventory: until you scratch them, the
load modules are still in the target library, and the copies that SMP accepted
are in the distribution library.

*What has been measured.* The fallback was run on a system with 1.0.0-dev
installed new and on a system with the prerelease alone. After the first, and
after scratching the libraries, a new install of 1.0.0-dev ran through with
return code 00 in every step. A system that went from the prerelease to 1.0.0
was not run through the fallback\; the mark of #cmd("TOFT010") is there as well,
so the same result is expected, but it has not been seen.

== What Is Not Removed <ug-remove-left>

SMP never owned these, so you have to take care of them:

- the procedure #cmd("OFTPD") in the procedure library, and the members
  #cmd("OFTPPRM0"), #cmd("OFTPPTN0") and #cmd("OFTPRCV0") that you copied into
  the parameter library, with your partners and rules in them;
- the entry for #cmd("OFTPD.LINKLIB") in the APF list (#cmd("IEAAPF00")), if you
  made one, and the two libraries in the ISPF allocations, with the menu entry
  of the dialog if you added it;
- *your spool* (#cmd("OFTP.SPOOL") in the sample job, or the name you chose),
  with the transfers in it. None of the steps above touches it. Delete it only
  when you no longer need what it holds;
- the data sets that OFTPD received files into.

== Why RESTORE and REJECT Do Not Work <ug-remove-why>

This applies once the accept job has run. The install job ends after the
APPLY, and in that state the way back is open, see below. The accept job
accepts the FMID, and that closes both of the usual ways back:

#screen(raw(read("../ex/ug-remove/restore-reject.txt")))

#cmd("RESTORE") refuses a SYSMOD that has been accepted (#cmd("HMA2452"), return
code 12). #cmd("REJECT") then fails for a reason that looks unrelated:
#cmd("HMA2462") says that the SYSMOD is not on the #cmd("SMPPTS") data set,
although the install job received it from there.

The cause, on the system where this was measured, is the option #cmd("PURGE=YES")
in the system entry of the #cmd("SMPPTS"). With it, SMP deletes a SYSMOD from
the PTS as soon as it has accepted it. The entry shows with
#cmd("LIST PTS ."):

#screen(raw(read("../ex/ug-remove/pts-system.txt")))

Measured on TK5 containers with the 1.0.0 package that mbt 3.0.1 builds, as a
new install\: after the accept job, #cmd("RESTORE") gave #cmd("HMA2452"),
#cmd("REJECT") gave #cmd("HMA2462"), both with return code 12, and
#cmd("LIST PTS .") showed no entry for #cmd("TOFT100"). After an upgrade from
the prerelease the #cmd("RESTORE") was refused as well, with #cmd("HMA2452") and
the text #cmd("DELETES SYSMODS"), return code 12.

*Before the accept job, RESTORE works.* On a new install that had only the
install job behind it, this job ended with return code 00, deleted the four load
modules from #cmd("OFTPD.LINKLIB") and took #cmd("TOFT100") out of the target
zone (it was never in the distribution zone)\:

#screen(```
//OFTPDRST JOB (SYS),'OFTPD RESTORE',CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//RESTORE EXEC SMPAPP
//HMASMP.LINKLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//HMASMP.OFTPLOAD DD  DISP=SHR,DSN=OFTPD.OFTPLOAD
//HMASMP.AOFTPLOD DD  DISP=SHR,DSN=OFTPD.AOFTPLOD
//SMPCNTL  DD  *
 RESTORE S(TOFT100) .
/*
//
```)

The #cmd("HMASMP.OFTPLOAD") statement is for the staging library, which exists
only in this state\: after the accept job, a job with that statement ends with a
#cmd("JCL ERROR") because the data set is gone. A #cmd("REJECT") instead ended
with return code 04 (#cmd("HMA4081"), then #cmd("HMA2270"))\: the PTS entry of
#cmd("TOFT100") was gone, and the status in the CDS stayed #cmd("REC APP"), so it
did not take the function out of the target zone. A #cmd("RESTORE") after that
#cmd("REJECT") still worked on a new install. A #cmd("REJECT") after a
#cmd("RESTORE") ended with return code 12 and #cmd("HMA2462"). Use
#cmd("RESTORE"), not #cmd("REJECT"). On a system that has had several installs
of the same FMID, a #cmd("RESTORE") was seen to end with #cmd("HMA2452") and
#cmd("DELETES SYSMODS") as well, so this holds for the new install only.
#cmd("OFTPD.OFTPLOAD") is not touched by a #cmd("RESTORE") and stays until you
scratch it (step 5).

Two things have not been measured: a system with #cmd("PURGE=NO"), and a
#cmd("REJECT") after the #cmd("ACCEPT") there. No #cmd("UCLIN") statement was
found that changes the system entry, so this book does not change it. It removes
the FMID in ways that do not need the PTS.

The accept is a job of its own for a reason. It fills the distribution library,
which is the base a later #cmd("RESTORE") of a PTF returns to\; without it
the #cmd("RESTORE") would delete the module instead of reverting it. The price is
that once the accept job has run the FMID itself cannot be taken back with
#cmd("RESTORE") or #cmd("REJECT"), and that is why the install job stops before
it.
