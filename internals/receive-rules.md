# Receive rules and jobs after receipt

Status: **decided** 2026-10-20 (section 6); **names, `REPLACE` and the
allocation are built** (#44, sections 1 to 3 and 7), **so is the job after
receipt** (#45, section 4 and 8). It turns `design.md` 5.2
("receive rules ... *later*") and 5.3 (post-processing) into something
that can be implemented, after rvsMVS's *Resident Receive Entries* (User
Manual 4.3 and appendix A.3), studied for concepts only. Where sections 1
to 5 still read "proposed", section 6 says what was decided and section 7
what was built and measured; they win.

## 1. What a rule decides

When a file arrives, OFTPD looks for the first rule that matches the
partner and the virtual file name, and the rule decides:

| What | rvs keyword | Proposed | Today |
|---|---|---|---|
| the data set name | `DSNNEW=`, `TSTAMP=`, `COUNTER=` | `DSN=` pattern with symbols (section 3) | `INPREFIX.<vfn>` |
| an existing data set | `REPLACE=` | `REPLACE=N\|Y` | always refused (SFNA 13) |
| where | `UNIT=`, `VOLUME=`, `SPACE=` | `UNIT=`, `VOLSER=`, `SPACE=trk` (only when the SFID gives no size) | global `UNIT`/`VOLSER` |
| translation | -- | `XLATE=Y\|N` over the partner's | the partner's |
| a job afterwards | `JOB=pds(member)` | `JOB=pds(member)` (section 4) | none |
| passing it on | `TO=sid` | `TO=partner` (*later*) | `RELAY=Y` only |
| GDG, SYSOUT, tape, SMS classes, EXPDT | yes | *later* or never (section 5) | -- |

**No rule matches** -- the default: today's behaviour, `INPREFIX.<vfn>`,
no job. So an installation without rules sees no change.

## 2. Where rules live and how they match

**In a parmlib member**, `OFTPRCV0` behind DD `OFTPRCV`, read at start and
on `REFRESH` together with the partner member. This is the way the partner
table went (design Q2); rvs kept its entries in the control data set,
maintained by a batch program and ISPF, and OFTPD can add a command for
that later without changing the format. A missing DD means no rules.

Same syntax as the partner member: `RULE=name` opens an entry, the
keywords after it belong to it:

```
* OFTPRCV0 - receive rules, first match wins
RULE=ORDERS   FROM=PARTNER1  VFN=ORDERS.*
              DSN=PROD.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
              JOB=PROD.OFTP.JCL(ORDERS)
RULE=INVOICE  FROM=*         VFN=INV?????.DATA
              DSN=ACCT.IN.%VFN  REPLACE=Y  VOLSER=WORK01
```

- `FROM=` a partner name or `*` (rvs `SID=ANY`); the partner must exist
  when the member is read, or the rule is refused with a message, as a
  bad partner entry is today.
- `VFN=` the virtual file name, `*` for any number of characters, `?`
  for exactly one (rvs's wildcards since 1.2.06).
- **First match wins**, in member order. rvs matched by station and
  name and does not say how two wildcard entries rank; member order is
  what an operator can see and test. (Decision 6.1.)
- A relayed file (`RELAY=Y`, not for us) never meets a rule: rules are
  about files that end here.

## 3. Names

`DSN=` is the full data set name after substitution, checked as a data
set name (qualifiers of 1-8, starting with a letter or national, at most
44) **when the file arrives**: a name that comes out invalid refuses the
file with SFNA 01, retry N, and the console says which rule made it.

Proposed symbols, a superset of rvs's `DSNNEW` placeholders:

| Symbol | Value |
|---|---|
| `%VFN` | the virtual file name (SFIDDSN) |
| `%Q1`..`%Q9` | its first, second, ... qualifier |
| `%ID` | the partner's name (rvs `?ID`) |
| `%YYYY` `%MM` `%DD` | date of arrival |
| `%HH` `%MI` `%SS` | time of arrival |
| `%CTR` | the OFTP counter of the file's time field, 4 digits (rvs `COUNTER=YES`); a file whose SFID time field is not the full `HHMMSScccc` with digits in the counter has no value, so the name does not come out and the file is refused like any invalid name |
| `%KEY` | the spool key, `T` + 7 digits: unique on this system |

A date or time can never start a qualifier, and a qualifier has at most
eight characters: `Y%YYYY` (5), `D%MM%DD` (5) and `T%HH%MI%SS` (7) fit,
`D%YYYY%MM%DD` (9) does not. That is the reason for the check, not a rule
of its own; the first draft of this section had the nine-character form
as its example. rvs wrote `%J` for the two-digit year and `%N` for the month; the
two-letter forms above are proposed because `%M` (minute in rvs) and
month are easy to confuse. (Decision 6.2.)

`REPLACE=Y`: the old data set is deleted only once the new one has been
received completely and closed -- the new file is under its temporary name
until then, as today -- so a failed receive never costs the old data set.
Not for a data set that is in use (the replace fails, the file is refused
at the end with SFNA 12, and the old one stays). Section 7 has the
mechanism: the old one is renamed aside, not deleted, until the new one
has its name.

A pattern whose first qualifier is built from `%VFN`, `%Qn` or `%CTR` would
hand the partner the whole name space the started task may write, and
`REPLACE=Y` on top of it the right to replace data sets there. **Decided:
the parser refuses it** (reason: the recommendation of the security review
of #44). The part of `DSN=` before the first literal `.` must not contain
`%VFN`, `%Q1`..`%Q9` or `%CTR`; the line is `OFTP010W`, the member is not
used (`OFTP025E`). `DSN=PROD.IN.%VFN` is fine, `DSN=%VFN`, `DSN=PROD%VFN`
and `DSN=%Q1.X` are not. `%ID`, `%KEY` and the date symbols are not
partner text and may stand anywhere. A pattern with no `.` at all is its
own first qualifier, so `DSN=PROD%VFN` is refused too. A `DSN=` that can
name a data set under `TMPPREFIX` (`TMPPREFIX.<anything>`, the received
file's own temporary names and the `.B<key>` backups) is refused as well:
the literal head of the pattern up to its first symbol is compared with
`TMPPREFIX.` and a rule that could clash is not used.

## 4. The job after receipt

*Built in #45 with the symbols `?PFILE ?VFILE ?ID ?KEY`; what differs from
this proposal is in section 8, which wins.*

`JOB=pds(member)`: once the file is under its final name and before the
EERP is queued, OFTPD reads the member, substitutes, and writes it to the
internal reader. rvs's symbols, which rvs users will know, all with `?`:

| Symbol | Value |
|---|---|
| `?PFILE` | the data set name the file was received as |
| `?VFILE` | the virtual file name |
| `?ID` | the partner |
| `?QDATE` `?QTIME` | the sender's date and time from the SFID (`Dyyddd`, `Thhmmsst` as rvs) |
| `?YYY` `?J` `?N` `?D` `?H` `?M` `?S` | now (rvs's set) |
| `?KEY` | the spool key |

- A line that would grow past column 71 (`//` lines) or 80 refuses the
  submit, not the file: the file stays received, the transfer records
  "job not submitted: line n too long", `OFTP2nnW` on the console. rvs
  marks it as an error the same way.
- The spool record has `ppstate` and `pprc` since the MVP for this:
  `S` submitted, `E` not submitted, and the reason. SHOW prints it.
- **Whose job.** It goes through OFTPD's internal reader, so without a
  `USER=` on its job card it runs as whatever an STC's submit gets on the
  system. The member is the installation's, named by a rule in parmlib,
  so whoever may change parmlib already decides what runs; OFTPD checks
  nothing more. (Decision 6.3.)
- The EERP does not wait for the job: it says the file arrived, not
  that it was processed. Proposed so; that rvs did the same is inferred,
  the User Manual does not tie the two together.

*Later*, from rvs: `JOB=` after a send (A.2 #31), `ERRJOB=` when a send
fails for good, and a job when the submit itself fails.

## 5. Not proposed

- **SYSOUT=, COPIES=, DEST=** (print a received file): possible on 3.8j,
  rarely wanted; later if asked for.
- **GDG=YES**: GDGs exist on 3.8j; it needs a catalog LOCATE of the base
  and `(+1)` allocation through `__dsalcf()`, not measured. Later.
- **DEVICE=TAPE**, the SMS classes, `JSSEVENT`, `ACCOUNT`: not on a
  hobbyist 3.8j system.
- **The ISPF maintenance of rules** (rvs options 4 and 5): the member is
  edited with ISPF EDIT, `REFRESH` makes it active.

## 6. Decisions (2026-10-20, the maintainer, on #44)

1. **Matching**: the first matching rule in member order wins. No
   ranking by specificity; the operator reads the order off the member.
2. **Symbols**: only `%YYYY %MM %DD %HH %MI %SS`, as proposed. The rvs
   names (`%YYY %J %N %D %H %M %S`) are not supported, and neither are
   `%YY` and `%JJJ`, which the proposal listed but nobody asked for.
3. **The job's identity** is deferred to MVS-42 (the job after receipt).
   Decided there (2026-10-20, the board, option A): no extra checks, the
   job goes through the internal reader of the OFTPD started task, no
   `JOBUSER=`, no `USER=` added.
4. **The member**: its own, `OFTPRCV0` behind DD `OFTPRCV`, not inside the
   partner member.
5. **Order of work**: names and `REPLACE` first (they change only how the
   data set is allocated), the job after receipt second.

## 7. What was built (#44)

Code: `src/oftp#rcv.c` and `include/oftprcv.h` (portable: parsing,
matching, name expansion; `test/tstrcv.c`, 68 assertions on the host),
the receive path in `src/mvs/oftp#mon.c` (`f_open_in`, `f_close_in`,
`oftp_mon_rules`), `OFTP_DSOPT.space` in `src/mvs/oftp#dsf.c`.

**Format beyond section 2.** `SPACE=` is the primary in tracks, 1 to 9999,
with half of it plus one as secondary, and counts only when the SFID gives
no size (an estimate from the size wins). `XLATE=` overrides the partner's
for this file. Without `DSN=` the file gets the `INPREFIX` name even if the
rule matched, so a rule can set `REPLACE=` or `VOLSER=` alone; `REPLACE=Y`
without `DSN=` replaces nothing, because the default name is made unique
by the transfer number. A rule holds up to 64 entries and a `DSN=` pattern
80 characters. Case does not matter in the member; the match is on the
upper-cased value. Columns 73 to 80 of the member are ignored.

**The set is replaced as a whole.** A member is read into a scratch
`OFTP_RULES` and swapped into `OFTP_MON.rules` under the monitor lock only
when the member has no error at all (a syntax error, an unknown keyword, an
unknown `FROM=` partner, a duplicate rule name, a bad pattern: each is
`OFTP010W` with context `OFTPRCV`). With any error the old set stays and
`OFTP025E` says so; at start that means no rules. A rule is looked up once
per file, at its `F_OPEN`, and kept in the session for the close, so a
`REFRESH` in the middle of a file does not change that file. The `OFTP_MON`
block holds the set (some 11 KB for 64 rules); there is no writable static.
The rules are read after the partners, those of `OFTPPTN` and those kept
by command (`oftp_mon_maint`), because `FROM=` is checked against them.

**`REPLACE=Y`, the mechanism.** A received file is under `TMPPREFIX.<key>`
until `f_close_in` has the closed, complete file. With an existing target
and `REPLACE=Y`: rename the old data set to `TMPPREFIX.B<key without T>`,
rename the temporary one to the final name, then remove the backup.
Only a plain sequential data set (DSORG=PS, nothing else in the DSCB) is
renamed: a PDS, a VSAM cluster or a member name is not something the
backup rename can treat as "a data set to replace". The check is made
twice, at `F_OPEN` (`OFTP234W`, SFNA 12, retry `N`: the file is not
received at all, no space is spent on it) and again at the close before the
first rename (`OFTP226E`, SFNA 12), because the target can change in
between. Either way the old data set is untouched. `oftp_dsf_kind()` in
`oftp#dsf.c` answers it from the DSCB (`__locate`/`__dscbdv`): NONE if the
catalog has no such name, PS, or OTHER for everything else (a PDS or VSAM
name, a member, a name over 44 characters, a DSCB that cannot be read).
A failed first rename (catalog full, say) refuses the file with SFNA 12 and
leaves everything as it was (`OFTP226E`); a failed second rename, which
is what an old data set in use produced when measured, renames the old
one back (`OFTP227E` if even that fails, naming the
backup). A failed remove of the backup leaves the new file in place and
says `OFTP228W`: the new file is complete under the final name and the
backup is a spare copy that is deleted by hand.

**After a crash between the two renames** the final name does not exist and
`TMPPREFIX.B<key>` is the *only* copy of the old data set. Never delete it
first. The transfer's spool record is still ACTIVE (the close did not
complete), so at the next start `oftp_mon_recover` discards the partial
receive and `restore_old()` looks at its backup: target missing and backup
there, it renames the backup back to the target (`OFTP239I`; `OFTP227E` if
that rename fails, naming the backup); both there (the second rename had
happened already), it leaves both and says `OFTP248W` -- the operator
compares and deletes the backup. By hand it is the same rule: check
whether the target exists; if not, rename the backup back; only if the
target exists, delete the backup. `REPLACE=N` on an existing name
is refused at `F_OPEN`, before any allocation, with SFNA 13 and retry `Y`
(`OFTP225W`), the answer an ordinary duplicate gets; a sender that keeps
trying keeps being refused until the data set is deleted or the rule
changes. `OFTP229I` reports a completed replace. Write and close are
checked before the EFPA as before: the rename comes after the close.

Messages: `OFTP024I` rules read, `OFTP025E` not used, `OFTP026W` DD cannot
be opened on a refresh; `OFTP224W` invalid name, `OFTP225W` exists, `OFTP226E`
cannot be replaced, `OFTP227E` not renamed back, `OFTP228W` backup not
deleted, `OFTP229I` replaced; `OFTP234W` the existing data set is not a
sequential one and is not replaced, `OFTP239I` the old data set restored
from its backup at start, `OFTP248W` both the target and the backup exist
after a crash; `OFTP221E` (older) the rename of the temporary file failed.

**Visibility of the backup.** For the short time of a replace the old
content lives under `TMPPREFIX.B<key>`, readable by anyone whom the
installation's security product lets read under that prefix. Protect
`TMPPREFIX` like the data sets the rules write to.

### Measured

In a throwaway MVS/CE container (2026-10-20): two OFTPDs (A and B) with
partners TSTA and TSTB on one system, `test/mvs/tstrcvm.c` as a batch
client that is **not** APF-authorized (it reaches A through the SVC),
sending to B; `test/mvs/tstrcvmd.jcl` sets up the members and the jobs,
`test/mvs/tstrcvm.jcl` runs it. Every file was followed through `SHOW` to
DELIVERED or REJECTED, then the data sets were listed and compared with
IEBCOMPR; every step's return code was checked.

| Case | Result |
|---|---|
| pattern rule: `DSN=...%ID.%Q3`, `VOLSER=WORK01`, `SPACE=2` | the data set under the rule's name on WORK01, the content identical, DELIVERED |
| date rule `D%MM%DD`, `REPLACE=Y` | the day's name, `OFTP229I` when it existed |
| `REPLACE=Y` over an existing data set | the old content gone, the new one in, no `TMPPREFIX.B` left |
| `REPLACE=Y` where the old data set was held by another job | the rename of the held data set to the backup name succeeded and the rename of the temporary file to the final name then failed; the rename back worked; `OFTP221E`, SFNA 12, the old data set intact and compared equal to what it held before |
| `REPLACE=N` over an existing data set | REJECTED with 13, `OFTP225W`, the old data set untouched |
| no rule | `INPREFIX.<vfn>` as before |
| a rule whose name comes out invalid (`%Q3` is `9X`, a qualifier starting with a digit) | REJECTED with 01, `OFTP224W` names the rule |
| `REFRESH` with a second good member | `OFTP024I`, the new rules decide the next file |
| `REFRESH` with a broken member (`%NOSUCH`, `FROM=NOBODY`, `BOGUS=1`) | three `OFTP010W`, `OFTP025E`, the second set still in force |
| `mbt test --mvs`: TSTDSF (space override), TSTRCV | ok |

### Measured, second round (security review fixes)

Another throwaway MVS/CE container (2026-10-20), the merged head of the
branch, same set-up. `test/mvs/tstrcvm.c` has two more cases, `REPLACE=Y`
onto a PDS and onto a VSAM cluster, and `test/mvs/tstrcvr.c` (new, from
`test/mvs/tstrcvr.jcl`, needs no running OFTPD) drives
`oftp_mon_recover()` and `oftp_dsf_kind()` directly.

| Case | Result |
|---|---|
| `oftp_dsf_kind()` of a PS, a name not in the catalog, a PDS, a VSAM cluster, a member | `PS`, `NONE`, `OTHER`, `OTHER`, `OTHER` (TSTRCVR) |
| `REPLACE=Y` onto a PDS (`IBMUSER.RCVTEST.PDS.ONE`, one member) | REJECTED with 12, `OFTP234W ... IS NOT A SEQUENTIAL DATA SET AND IS NOT REPLACED (RULE PDS)`, the waiting send deleted; IEBCOMPR `TYPORG=PO` of the PDS against its copy RC 0 |
| `REPLACE=Y` onto a VSAM cluster | the same, `OFTP234W`; the cluster still in `LISTCAT` |
| after these, `LISTCAT` of `TMPPREFIX` | `NOT FOUND`: no temporary name or backup left |
| `REPLACE=Y` where the old data set was held by another job (again) | REJECTED with 12, now `OFTP226E` (the check at the close; the old data set compared equal to what it held) |
| recovery, backup only (`TMPPREFIX.B<key>` present, target missing) | the backup renamed to the target, the half-received temporary file removed, the spool record deleted (TSTRCVR 1) |
| recovery, both present | both left, the record deleted (TSTRCVR 2); `OFTP248W` is in the code path but was not read from the console |
| recovery, no backup | the target untouched (TSTRCVR 3) |
| `TSTRCVM` 1, 2, 3 | 28/28, 6/6, 3/3; `REFRESH` with the broken member: `OFTP010W` three times (DSN, `BOGUS`, `FROM=NOBODY`), `OFTP025E`, the second set still in force |
| `mbt test --mvs` | all pass except `TSTSCH` (8 assertions, as before this work) and `TSTLOG` (3 assertions, "the record is stamped", from the log work in main); both are clock/time-stamp tests in files this work does not touch, and pass on the host |

Notes on the runs: `TSTRCVR` needs `REGION=4096K` (S878 with the default
region: the test holds an `OFTP_MON`). The two OFTPDs hold both class-A
initiators of the container, so a third initiator is started
(`$SI4`) before `TSTRCVM1` can run. A bad line marks its own rule bad, and
a rule that is bad is not checked for `FROM=` again: the broken member of
`TSTRCVW3` therefore puts `BOGUS=1` into the first rule, so that all three
errors are reported.

**Not measured on MVS**: a receive that fails halfway with `REPLACE=Y` (the
temporary file is removed; the old data set is untouched by construction,
because nothing renames it before the close); the `XLATE=` override; `SPACE=`
for a file whose SFID gives no size (`tstdsf` covers the allocation string,
the container run sent files with a size); a real stop of a running OFTPD
between the two renames and the restore at its next start (TSTRCVR runs the
same function on prepared data sets, not after a crash); the console text of
`OFTP239I` and `OFTP248W`. `TSTRCVM` and `TSTRCVR` need their JCL, not
`mbt test --mvs` (`host = false`, they skip there).

## 8. The job after receipt, as built (#45)

Code: `src/oftp#sub.c` and `include/oftpsub.h` (portable: the symbols and
the card width; `test/tstsub.c`), `src/mvs/oftp#job.c` and
`include/oftpjob.h` (reading the member, the internal reader), `run_job()`
in `src/mvs/oftp#mon.c` called from `f_close_in`, `JOB=` in
`src/oftp#rcv.c`. The hook is one call, after a `REPLACE=Y` has been
settled and before the record goes to RECEIVED, so the job is submitted
only for a file that is under its final name, and the EERP, which goes out
afterwards, never waits for the job to run. Relay files never meet a rule,
so they get no job.

**What differs from section 4.**

- Only `?PFILE ?VFILE ?ID ?KEY` are implemented. The rvs date and time
  symbols (`?QDATE ?QTIME ?YYY ?J ?N ?D ?H ?M ?S`) are not: their formats
  are not verified and nobody asked for them (the issue says "at least"
  the four). A `?` that starts any other name stays as it is, so a member
  written with them is not damaged, only not substituted.
- The result is in the spool record's `ppstate` (`S` submitted, `E` not),
  `pprc` (0 or 8) and `pptext` (the JES job id or the reason; a field
  appended to the record, so older spool data sets still read, with an
  empty value). It is not in `text`, because `f_erp_sent` overwrites that
  ("received, EERP sent"). `SHOW` prints `job submitted JOBnnnnn` or `job
  not submitted: <reason>`; `LIST` is unchanged.
- **Two passes over the member.** A job that is closed half written on the
  internal reader is run by JES all the same, so the first pass reads and
  substitutes every record with no reader open (the width, `?VFILE`, a
  record past column 80, an empty member) and only the second pass opens
  the reader and writes. Between the passes the member could be edited; a
  failure in the second pass closes the reader, and what was written is
  then run. That window is the reading of one member.
- **Width.** A record starting `//` has its text in columns 1 to 71, column
  72 is the continuation mark and 73 to 80 are sequence numbers (a comma
  that landed in 72 would be read by JES as a continuation mark, measured
  as IEF618I): with no symbol in it the record goes through as
  it is, with one the text is substituted and padded. Any other record has
  80 columns. A record that grows past its columns is not submitted
  (`OFTP247W`, "LINE TOO LONG AFTER SUBSTITUTION (RECORD n)"), nor cut.
- **`?VFILE` is partner text in JCL**, so it is limited to letters, digits
  and `. - _ @ # $`; a job that uses `?VFILE` and meets another character
  is not submitted. This goes beyond decision 6.3 (which checks nothing
  more about the *job*): it protects the *member* from the partner, the
  way the first-qualifier rule of `DSN=` does. `?PFILE ?ID ?KEY` come from
  OFTPD, the rule and the partner table.
- `JOB=` takes `pds(member)` with a data set name of up to 44 characters
  and a member of 1 to 8; it may not hold a symbol (a data set name that
  came from the partner would choose what runs). A rule with a bad
  `JOB=` is an error and the member is not used (`OFTP010W`, `OFTP025E`).
- Messages: `OFTP246P` (log) submitted, `OFTP247W` (console and log) not
  submitted.

**Measured** (mvsce container, two OFTPDs, `test/mvs/tstrcvm*.jcl`):

- A file whose rule has `JOB=` is received, the job goes to the internal
  reader and runs in about the time the EERP takes: `OFTP246P TSTA:
  T0000028 JOB IBMUSER.TSTRCVM.JCL(RCVJOB) SUBMITTED FOR
  IBMUSER.RCVTEST.JOB.ONE AS JOB00034` on the log, the copies named after
  `?ID` and `?VFILE` are there with the file's content. `SHOW` prints
  `100 job submitted JOB00036 (JES job id)`, `100 job not submitted: LINE
  TOO LONG AFTER SUBSTITUTION (RECORD 5)` and `100 job not submitted:
  MEMBER NOT FOUND OR NOT READABLE`. The long line and the missing member
  do not refuse the file (delivered, EERP back) and no job runs.
- **The job's identity.** A job from the internal reader of a started task
  with no `USER=` runs as RAKF's default user (`PROD`), not as the task:
  the first run failed with `IEF720I USER NOT AUTHORIZED TO DEFINE THIS DATA
  SET` on `IBMUSER.RCVTEST.*`. The test set-up needs a RAKF profile that
  lets that user define `IBMUSER.RCVTEST.JOB*` (`tstrcvmd.jcl`). This is
  what decision 6.3 option A means in practice; the alternative that was
  weighed (`JOBUSER=`) would have given the job a user of the operator's
  choosing.
- **Column 72.** A substituted `//` line that ended in a comma in column 72
  was read by JES as a continuation mark (`IEF618I`): the text of a JCL
  statement stops at column 71, which is why the width is 71, not 72.
