# Receive rules and jobs after receipt -- a proposal

Status: **proposal**, 2026-10-18, for the maintainer to decide; nothing
of it is built. It turns `design.md` 5.2 ("receive rules ... *later*")
and 5.3 (post-processing) into something that can be implemented, after
rvsMVS's *Resident Receive Entries* (User Manual 4.3 and appendix A.3),
studied for concepts only. Section 6 lists what needs a decision.

## 1. What a rule decides

When a file arrives, OFTPD looks for the first rule that matches the
partner and the virtual file name, and the rule decides:

| What | rvs keyword | Proposed | Today |
|---|---|---|---|
| the data set name | `DSNNEW=`, `TSTAMP=`, `COUNTER=` | `DSN=` pattern with symbols (section 3) | `INPREFIX.<vfn>` |
| an existing data set | `REPLACE=` | `REPLACE=N\|Y` | always refused (SFNA 13) |
| where | `UNIT=`, `VOLUME=`, `SPACE=` | `UNIT=`, `VOLSER=`, `SPACE=trk` (only when the SFID gives no size) | global `UNIT`/`VOLSER` |
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
              DSN=PROD.ORDERS.D%YYYY%MM%DD.T%HH%MI%SS
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
| `%YYYY` `%YY` `%MM` `%DD` `%JJJ` | date of arrival (`%JJJ` the day of the year) |
| `%HH` `%MI` `%SS` | time of arrival |
| `%CTR` | the OFTP counter of the file's time field, 4 digits (rvs `COUNTER=YES`) |
| `%KEY` | the spool key, `T` + 7 digits: unique on this system |

A date or time can never start a qualifier (`D%YYYY%MM%DD`, not
`%YYYY%MM%DD`): that is the reason for the check, not a rule of its own.
rvs wrote `%J` for the two-digit year and `%N` for the month; the
two-letter forms above are proposed because `%M` (minute in rvs) and
month are easy to confuse. (Decision 6.2.)

`REPLACE=Y`: the old data set is deleted only once the new one has been
received completely, closed and is about to be renamed -- the new file is
under its temporary name until then, as today -- so a failed receive never
costs the old data set. Not for a data set that is in use (the rename
fails, the file is refused at the end, EFNA, and the old one stays).

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
- rvs's EERP waits for nothing: the EERP is about the file having
  arrived, not about the job. Proposed the same.

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

## 6. To decide

1. **Matching**: first match in member order, or most specific first
   (exact name before a wildcard, a partner before `*`)?
2. **Symbols**: `%YYYY %MM %DD %HH %MI %SS` as proposed, or rvs's exact
   `%YYY %J %N %D %H %M %S`, so that an rvs installation's names carry
   over? (Both could be accepted.)
3. **The job's identity**: is "whoever may write parmlib decides" enough,
   or should a rule name the userid the job runs as (`JOBUSER=`), with
   OFTPD putting `USER=` on the job card?
4. **The member name**: `OFTPRCV0` and DD `OFTPRCV`, or rules inside the
   partner member (one file to edit)?
5. **Order of work**: names and `REPLACE` first (they change only how the
   data set is allocated), the job second?
