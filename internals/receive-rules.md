# Receive rules and jobs after receipt

Status: **decided** 2026-10-20 (section 6); **names, `REPLACE` and the
allocation are built** (#44, sections 1 to 3 and 7), **the job after
receipt is not** (section 4, a later issue). It turns `design.md` 5.2
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
| `%CTR` | the OFTP counter of the file's time field, 4 digits (rvs `COUNTER=YES`) |
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

## 4. The job after receipt

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

- A line that would grow past column 72 (`//` lines) or 80 refuses the
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
   Until then there is no job; section 4 stays a proposal.
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
The rules are read after the partners, because `FROM=` is checked against
them.

**`REPLACE=Y`, the mechanism.** A received file is under `TMPPREFIX.<key>`
until `f_close_in` has the closed, complete file. With an existing target
and `REPLACE=Y`: rename the old data set to `TMPPREFIX.B<key without T>`,
rename the temporary one to the final name, then remove the backup.
A failed first rename (catalog full, say) refuses the file with SFNA 12 and
leaves everything as it was (`OFTP226E`); a failed second rename, which
is what an old data set in use produced when measured, renames the old
one back (`OFTP227E` if even that fails, naming the
backup). A failed remove of the backup leaves the new file in place and
says `OFTP228W`: a backup `TMPPREFIX.B<key>` after a crash or that warning
is the only trace and is deleted by hand. `REPLACE=N` on an existing name
is refused at `F_OPEN`, before any allocation, with SFNA 13 and retry `Y`
(`OFTP225W`), the answer an ordinary duplicate gets; a sender that keeps
trying keeps being refused until the data set is deleted or the rule
changes. `OFTP229I` reports a completed replace. Write and close are
checked before the EFPA as before: the rename comes after the close.

Messages: `OFTP024I` rules read, `OFTP025E` not used, `OFTP026W` DD cannot
be opened on a refresh; `OFTP224W` invalid name, `OFTP225W` exists, `OFTP226E`
cannot be replaced, `OFTP227E` not renamed back, `OFTP228W` backup not
deleted, `OFTP229I` replaced; `OFTP221E` (older) the rename of the temporary
file failed.

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

**Not measured on MVS**: a receive that fails halfway with `REPLACE=Y` (the
temporary file is removed; the old data set is untouched by construction,
because nothing renames it before the close); the `XLATE=` override; `SPACE=`
for a file whose SFID gives no size (`tstdsf` covers the allocation string,
the container run sent files with a size); a crash between the two renames.
`TSTRCVM` needs two running monitors and is run from its JCL, not by
`mbt test --mvs` (`host = false`, it skips there). On the container
`TSTSCH` fails 8 assertions in each of batch and TSO (the "today 22:00",
"tomorrow", "day 300" cases) that pass on the host; the file is untouched
by this work and the cause is not investigated.
