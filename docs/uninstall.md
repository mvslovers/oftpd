# Removing OFTPD

This is the supported way to take OFTPD back off an MVS 3.8j system and free
its FMID for a re-install. The same procedure is chapter 7, "Removing OFTPD",
of the _OFTPD User's Guide_ (ML09-0001).

The main way lets SMP do the work: a dummy function, `TOFTDEL`, deletes every
release of OFTPD, and the load modules leave both libraries with it. A way that
uses `UCLIN` alone stays as the [fallback](#the-fallback-uclin-alone).

> **If you are upgrading rather than removing, you are in the wrong document.**
> A release deletes its predecessor as part of its own install, and the
> scratch step below would delete the libraries the new release installs into.
> See
> [Upgrading](https://github.com/mvslovers/oftpd/blob/main/docs/installation.md#upgrading).

## What this release put on the system

| | |
|---|---|
| FMID | `TOFT100` |
| Load modules | `OFTPD`, `OFTP`, `OFTPI`, `OFTPSVC` |
| Target library | `OFTPD.LINKLIB` |
| Distribution library | `OFTPD.AOFTPLOD` |
| Sample library | `OFTPD.SAMPLIB` |
| ISPF panel library | `OFTPD.ISPPLIB` |

The prerelease 0.1.0-dev put `TOFT010` on the system, with the three load
modules `OFTPD`, `OFTP` and `OFTPI`, into the same libraries.

The staging library `OFTPD.OFTPLOAD` is not on the list, because the cleanup
step of the install job scratched it.

**An earlier release leaves its FMID behind.** `TOFT100` deletes `TOFT010`
(its `++VER` reads `DELETE(TOFT010)`), and SMP leaves the id of the deleted
release in the inventory as a mark, in both zones:

```
TOFT010   TYPE            = FUNCTION
          DELBY           = TOFT100
```

The mark is there on a system that went from the prerelease to 1.0.0, and on a
system that installed 1.0.0 new and never had the prerelease. Deleting the id of
the current release clears only that one. Both ways below therefore name
**every id OFTPD has used** (`TOFT010` and `TOFT100`), and you need not know
which releases the system has seen. The jobs are written for releases up to
1.0.0. If a later release adds an id, it is added to every list here.

---

## 1. Stop the monitor

```
/P OFTPD
```

Wait until the task has ended, and make sure that no batch job or TSO session
is still using `OFTPD.LINKLIB`. OFTPD gives its SVC entry back when it ends. If
the log says `OFTP036W` or `OFTP037W`, it has left the routine and the anchor in
common storage, and that storage stays allocated until the next IPL (with
`OFTP036W` the SVC number may stay unusable as well). That does not change
anything below. See the message descriptions in the Reference.

## 2. Delete the release with the dummy function

SMP deletes a function when a later function names it in a `DELETE` operand,
and it removes the load modules of the deleted function from the libraries on
the way. The job below receives, applies and accepts such a function.
`TOFTDEL` has no elements. It deletes all OFTPD ids up to 1.0.0 in one `DELETE`
statement.

**`TOFTDEL` is not a release FMID.** No release of OFTPD ships it, the job
brings it in by itself, and step 3 frees it again in the same flow. Do not use
it for anything else, and do not keep it on the system.

Two things in the job matter:

- The `APPLY` and the `ACCEPT` step each carry a `HMASMP.LINKLIB` and a
  `HMASMP.AOFTPLOD` statement. SMP deletes the modules from the libraries that
  these DD names point to. Without `HMASMP.AOFTPLOD`, the `ACCEPT` ends with
  return code 12 and the message `HMA2833 ** AOFTPLOD DDCARD MISSING`. Without
  `HMASMP.LINKLIB`, the `APPLY` still ends with return code 04 and prints the
  same `HMA2240` lines, but it deletes from the library that the procedure names
  itself (`SYS1.LINKLIB` on the systems measured), and `OFTPD.LINKLIB` keeps its
  members. The member list of the last step is the only proof. The statements
  come after the `EXEC` statement and before `SMPCNTL`, as in the install job.
- The later steps run only on `COND=(4,LT,...)`. The `RECEIVE` ends with return
  code 04, because a function without elements raises `HMA3971`. With the usual
  `COND=(0,NE,...)` the job would stop right after the receive.

Submit this job:

```
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
```

Read the three SMP steps one by one. The receive ends with return code 04:

```
 RECEIVE SELECT(TOFTDEL) .
----------++FUNCTION(TOFTDEL) .
----------++VER(Z038) DELETE(TOFT010,TOFT100) .
HMA3971    SYSMOD TOFTDEL HAS NO ELEMENTS
HMA3930    SYSMOD TOFTDEL SUCCESSFULLY RECEIVED
HMA2050    RECEIVE PROCESSING COMPLETED - HIGHEST RETURN CODE IS 04
```

The apply deletes the four load modules from the target library. It also ends
with return code 04, because SMP does not find the id that was applied in the
restore data set (`HMA2461`). Here that is `TOFT100`; on a system that has only
the prerelease it is `TOFT010`, and the apply deletes three load modules:

```
 APPLY S(TOFTDEL) DIS(WRITE) .
HMA2240    SUCCESSFULLY DELETED LMOD OFTP ON LINKLIB LIBRARY
HMA2240    SUCCESSFULLY DELETED LMOD OFTPD ON LINKLIB LIBRARY
HMA2240    SUCCESSFULLY DELETED LMOD OFTPI ON LINKLIB LIBRARY
HMA2240    SUCCESSFULLY DELETED LMOD OFTPSVC ON LINKLIB LIBRARY
HMA2270    APPLY PROCESSING SUCCESSFULLY COMPLETED FOR SYSMOD TOFTDEL
HMA2461    SYSMOD TOFT100 NOT FOUND ON SMPSCDS LIBRARY
HMA2050    APPLY PROCESSING COMPLETED - HIGHEST RETURN CODE IS 04
```

The accept deletes the four copies from the distribution library and ends with
return code 00:

```
 ACCEPT S(TOFTDEL) DIS(WRITE) .
HMA2240    SUCCESSFULLY DELETED MODULE OFTP ON AOFTPLOD LIBRARY
HMA2240    SUCCESSFULLY DELETED MODULE OFTPD ON AOFTPLOD LIBRARY
HMA2240    SUCCESSFULLY DELETED MODULE OFTPI ON AOFTPLOD LIBRARY
HMA2240    SUCCESSFULLY DELETED MODULE OFTPSVC ON AOFTPLOD LIBRARY
HMA2270    ACCEPT PROCESSING SUCCESSFULLY COMPLETED FOR SYSMOD TOFTDEL
HMA2050    ACCEPT PROCESSING COMPLETED - HIGHEST RETURN CODE IS 00
```

**The result is in the last step, not in the return codes.** Both libraries
must list no member. Everything else is the proof that the job ran, not that the
work happened:

```
 LISTDS 'OFTPD.LINKLIB' MEMBERS
OFTPD.LINKLIB
--RECFM-LRECL-BLKSIZE-DSORG
  U     **    15040   PO
--VOLUMES--
  WORK00
--MEMBERS--
 LISTDS 'OFTPD.AOFTPLOD' MEMBERS
OFTPD.AOFTPLOD
--RECFM-LRECL-BLKSIZE-DSORG
  U     **    15040   PO
--VOLUMES--
  WORK01
--MEMBERS--
```

If a step ends higher than 04, the following `APPLY` or `ACCEPT` step does not
run; the last step does, and shows what is still there. Read the output of the
failed step; do not submit the whole job again before you know what it did. An
`ACCEPT` that ended with return code 12 on `HMA2833` has still rewritten the
ACDS, and the distribution library still lists the first module of the
function. Submitting the `ACCEPT` step alone, with both `HMASMP` statements,
ends with return code 00 and `HMA2270`, but deletes only the other modules; the
first one stays in `OFTPD.AOFTPLOD`, where SMP no longer knows it. That return
code 00 does not mean the library is empty: list it as shown above. Scratch the
library with step 5 if OFTPD goes for good.

**What has been measured.** The job was run on test systems (MVS/CE) in two
states: with the prerelease 0.1.0-dev alone, and after the upgrade from the
prerelease to 1.0.0-dev, the build of the tree before the release. A system
with more than one product in the same zones, a system with other service on
it, and a dummy function with a `++MAC` element instead of none have not been
measured. The recovery after a failed `ACCEPT` was measured with the prerelease
only. Whatever the system, the `LIST` in step 4 is the proof.

## 3. Free the ids

The dummy function has not made the ids free. It has left `TOFTDEL` itself in
both zones, with the status `REC APP ACC` (the ACDS adds `RGN`), and a mark
`DELBY = TOFTDEL` on every id it deleted. This job removes the three entries from the CDS (what is applied)
and the ACDS (what is accepted). It changes the two SMP data bases and no
library:

```
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
```

Every `DEL` finds its entry, because the dummy function left a mark for every id
it named. That includes an id that the system never had: on a system that had
only the prerelease, `TOFT100` was marked as well, and the `DEL` found it. Both
`UCLIN` steps end with return code 00, and each `DEL` reports
`HMA2550 UPDATE COMPLETE`. This is the output of step `UCLCDS`:

```
 UCLIN CDS .
  DEL SYSMOD(TOFTDEL) .
HMA2550    UPDATE COMPLETE FOR TOFTDEL
  DEL SYSMOD(TOFT100) .
HMA2550    UPDATE COMPLETE FOR TOFT100
  DEL SYSMOD(TOFT010) .
HMA2550    UPDATE COMPLETE FOR TOFT010
 ENDUCL .
HMA2050    UCLIN PROCESSING COMPLETED - HIGHEST RETURN CODE IS 00
```

Step `UCLACDS` gives the same. Any other message is not expected.

## 4. Read the LIST — this is the actual result

The last step of the job is the result. For every id, in both zones, it must
answer that the entry was not found. These are the first two of the six
statements, as the step printed them:

```
 RESETRC .
 LIST CDS  SYSMOD(TOFTDEL) .
LIST     SELECT SUMMARY REPORT FOR SMPCDS
THE FOLLOWING SELECTED ENTRIES WERE NOT FOUND OR WERE NOT ELIGIBLE FOR PROCESSING
 TYPE        NAME
 SYSMOD      TOFTDEL
 LIST ACDS SYSMOD(TOFTDEL) .
LIST     SELECT SUMMARY REPORT FOR SMPACDS
THE FOLLOWING SELECTED ENTRIES WERE NOT FOUND OR WERE NOT ELIGIBLE FOR PROCESSING
 TYPE        NAME
 SYSMOD      TOFTDEL
```

The step must end with the highest return code 04. **Return code 04 together
with this answer means that the FMID is free**, and `TOFTDEL` with it. Both
zones count: an id gone from one and still in the other is not free.

**Read the stanzas, not the return code of the step.** A mark that is still
there answers with return code 00, so it does not raise the return code of the
step above the 04 that you expect. The job has six `LIST` statements, and all
six must show the id as not found. A stanza that names `TYPE = FUNCTION` is an
entry that is still there, whether it shows a status such as `REC APP ACC` or only a
`DELBY` of its successor. This is what the statements
`LIST CDS SYSMOD(TOFT100)` and `LIST CDS SYSMOD(TOFTDEL)` print after step 2,
before step 3 has run:

```
 LIST CDS  SYSMOD(TOFT100) .
SMPCDS   SYSMOD ENTRIES
  NAME
TOFT100   TYPE            = FUNCTION
          DELBY           = TOFTDEL

 LIST CDS  SYSMOD(TOFTDEL) .
SMPCDS   SYSMOD ENTRIES
  NAME
TOFTDEL   TYPE            = FUNCTION
          STATUS          = REC  APP  ACC
          FMID            = TOFTDEL
          DATE/TIME REC   = 26.294  05:43:43
                    APP   = 26.294  05:43:52
                    ACC   = 26.294  05:43:57
          DELETE VER(001) = TOFT010   TOFT100
```

A mark that was there before stays as it was: on a system that has 1.0.0,
`TOFT010` still reads `DELBY = TOFT100` after the dummy function, not
`DELBY = TOFTDEL`. The `DEL` of step 3 clears it all the same.

## 5. Scratch the libraries

After step 2 the two libraries are empty, but they still exist. The allocation
job of the installation creates both with `DISP=(NEW,CATLG,DELETE)`. Run against
libraries that still exist, it does not fail: it ends with return code 00,
reports `IEF287I` with `NOT CATLGD` for both, and leaves uncataloged duplicates
behind. Delete the libraries in a job of your own before you install again:

```
  DELETE OFTPD.LINKLIB  NONVSAM SCRATCH PURGE
  DELETE OFTPD.AOFTPLOD NONVSAM SCRATCH PURGE
```

**Do this only when you remove OFTPD for good.** After an upgrade, these are the
data sets that the next release installs into; leave them. `OFTPD.SAMPLIB` and
`OFTPD.ISPPLIB` may stay. The install job scratches and receives them again the
next time.

## The fallback: UCLIN alone

Use this way when the dummy function cannot be used. After step 1, submit this
job instead of steps 2 and 3. It changes the two SMP data bases and no library:

```
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
```

Every `DEL` that finds its entry reports `HMA2550 UPDATE COMPLETE`. An entry
that is not in the zone is reported as `HMA2532` and `HMA2560`, and the step
ends with return code 08. That is expected for what your system never had; the
statements after it are still processed. It is no sign that a line did what you
wanted, and a step that ends with return code 08 can also hide a statement that
failed for another reason. Read every message, and take the `LIST` of step 4 as
the only proof.

On a system that installed 1.0.0 new, every `DEL` finds its entry, because
`TOFT100` named `TOFT010` and left a mark for it. Both steps end with return
code 00. On a system that has only the prerelease, `TOFT100`
and the module `OFTPSVC` were never in the zone. This is the output of step
`UCLCDS` there:

```
 UCLIN CDS .
  DEL SYSMOD(TOFT100) MOD(OFTP) .
HMA2532 ** SYSMOD ENTRY TOFT100 TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL SYSMOD(TOFT100) MOD(OFTPD) .
HMA2532 ** SYSMOD ENTRY TOFT100 TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL SYSMOD(TOFT100) MOD(OFTPI) .
HMA2532 ** SYSMOD ENTRY TOFT100 TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL SYSMOD(TOFT100) MOD(OFTPSVC) .
HMA2532 ** SYSMOD ENTRY TOFT100 TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL MOD(OFTP) .
HMA2550    UPDATE COMPLETE FOR OFTP
  DEL MOD(OFTPD) .
HMA2550    UPDATE COMPLETE FOR OFTPD
  DEL MOD(OFTPI) .
HMA2550    UPDATE COMPLETE FOR OFTPI
  DEL MOD(OFTPSVC) .
HMA2532 ** MODULE ENTRY OFTPSVC TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL LMOD(OFTP) .
HMA2550    UPDATE COMPLETE FOR OFTP
  DEL LMOD(OFTPD) .
HMA2550    UPDATE COMPLETE FOR OFTPD
  DEL LMOD(OFTPI) .
HMA2550    UPDATE COMPLETE FOR OFTPI
  DEL LMOD(OFTPSVC) .
HMA2532 ** LMOD ENTRY OFTPSVC TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL SYSMOD(TOFT100) .
HMA2532 ** SYSMOD ENTRY TOFT100 TO BE DELETED DOES NOT EXIST
HMA2560    UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE
  DEL SYSMOD(TOFT010) .
HMA2550    UPDATE COMPLETE FOR TOFT010
 ENDUCL .
HMA2050    UCLIN PROCESSING COMPLETED - HIGHEST RETURN CODE IS 08
HMA2050    HMASMP PROCESSING COMPLETED - HIGHEST RETURN CODE IS 08
```

Step `UCLACDS` gives the same, without the `LMOD` statements. Any other message
is not expected.

**The two zones have a step each for a reason.** After a step has ended with
return code 08, SMP does not execute the next `UCLIN` of the same step: it
answers `HMA2293 ** CONTROL STATEMENT IGNORED DUE TO PREVIOUS ERROR` and does
nothing. With both blocks in one step, the entries of a system that has only
the prerelease would leave the ACDS untouched.

Then read the `LIST` as in step 4, here four statements without `TOFTDEL`, and,
if you remove OFTPD for good, scratch the libraries as in step 5. `UCLIN`
changes only the inventory: until you scratch them, the load modules are still
in the target library, and the copies that SMP accepted are in the distribution
library.

**What has been measured.** The fallback was run on a system with 1.0.0-dev
installed new and on a system with the prerelease alone. After the first, and
after scratching the libraries, a new install of 1.0.0-dev ran through with
return code 00 in every step. A system that went from the prerelease to 1.0.0 was not run through the
fallback; the mark of `TOFT010` is there as well, so the same result is
expected, but it has not been seen.

## What is not removed, because SMP never owned it

- The procedure `OFTPD` in the procedure library, and the members `OFTPPRM0`,
  `OFTPPTN0` and `OFTPRCV0` that you copied into the parameter library, with
  your partners and rules in them.
- The entry for `OFTPD.LINKLIB` in the APF list (`IEAAPF00`), if you made one,
  and the libraries in the ISPF allocations, with the menu entry for the dialog
  if you added it.
- **Your spool** (`OFTP.SPOOL` in the sample job, or the name you chose), with
  the transfers in it. None of the steps above touches it. Delete it only when
  you no longer need what it holds.
- The data sets that OFTPD received files into.

Those are yours to delete.

---

## Why `RESTORE` and `REJECT` do not work

Worth knowing, because the messages point away from the cause. The install job
accepts the FMID in the same run in which it applies it. That closes both of the
usual ways back:

```
HMA2452 ** SYSMOD TOFT100 SELECTED FOR RESTORE HAS BEEN ACCEPTED
HMA2462 ** SYSMOD TOFT100 NOT FOUND ON SMPPTS LIBRARY
```

`RESTORE` refuses a SYSMOD that has been accepted (`HMA2452`, return code 12).
`REJECT` then fails for a reason that looks unrelated: `HMA2462` says that the
SYSMOD is not on the `SMPPTS` data set, although the job received it from there.

The cause, on the system where this was measured, is the option `PURGE=YES` in
the system entry of the `SMPPTS`. With it, SMP deletes a SYSMOD from the PTS as
soon as it has accepted it. The entry shows with `LIST PTS .`:

```
SMPPTS  SYSTEM ENTRY

SYSTEM    OPTIONS         = PAGELEN=0061  PEMAX=9999  PURGE=YES  REJECT=YES
```

Two things have not been measured: a system with `PURGE=NO`, and a `REJECT`
after the `ACCEPT` there. No `UCLIN` statement was found that changes the system
entry, so this document does not change it. It removes the FMID in ways that do
not need the PTS.

## Why the install job accepts at all

The `ACCEPT` fills the distribution library, which is the base a later
`RESTORE` of a **PTF** returns to. Without it, a `RESTORE` would delete the
module rather than revert it, because there would be no previous level to go
back to. The price is what this document is about: the FMID itself cannot be
taken back with `RESTORE` or `REJECT`.
