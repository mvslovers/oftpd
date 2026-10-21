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
cleanup step of the install job scratched it.

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

*What has been measured.* The job was run on test systems (MVS/CE) in two
states: with the prerelease 0.1.0-dev alone, and after the upgrade from the
prerelease to 1.0.0-dev, the build of the tree before the release. The recovery
after a failed #cmd("ACCEPT") was measured on the prerelease and on a new
install of the 1.0.0-dev package. A system with more than one product in the
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
job of the installation creates both with #cmd("DISP=(NEW,CATLG,DELETE)"). Run
against libraries that still exist, it does not fail: it ends with return code
00, reports #cmd("IEF287I") with #cmd("NOT CATLGD") for both, and leaves
uncataloged duplicates behind. Delete the libraries in a job of your own before
you install again:

#screen(```
  DELETE OFTPD.LINKLIB  NONVSAM SCRATCH PURGE
  DELETE OFTPD.AOFTPLOD NONVSAM SCRATCH PURGE
```)

Do this only when you remove OFTPD for good. After an upgrade, these are the
data sets that the next release installs into; leave them.
#cmd("OFTPD.SAMPLIB") and #cmd("OFTPD.ISPPLIB") may stay. The install job
scratches and receives them again the next time.

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

The install job accepts the FMID in the same run in which it applies it. That
closes both of the usual ways back:

#screen(raw(read("../ex/ug-remove/restore-reject.txt")))

#cmd("RESTORE") refuses a SYSMOD that has been accepted (#cmd("HMA2452"), return
code 12). #cmd("REJECT") then fails for a reason that looks unrelated:
#cmd("HMA2462") says that the SYSMOD is not on the #cmd("SMPPTS") data set,
although the job received it from there.

The cause, on the system where this was measured, is the option #cmd("PURGE=YES")
in the system entry of the #cmd("SMPPTS"). With it, SMP deletes a SYSMOD from
the PTS as soon as it has accepted it. The entry shows with
#cmd("LIST PTS ."):

#screen(raw(read("../ex/ug-remove/pts-system.txt")))

Two things have not been measured: a system with #cmd("PURGE=NO"), and a
#cmd("REJECT") after the #cmd("ACCEPT") there. No #cmd("UCLIN") statement was
found that changes the system entry, so this book does not change it. It removes
the FMID in ways that do not need the PTS.

The price of the accept is that a later #cmd("RESTORE") of a PTF can return to
the distribution library, and without the accept it would delete the module
instead of reverting it.
