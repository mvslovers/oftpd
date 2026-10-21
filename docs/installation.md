# Installing OFTPD

OFTPD (oftpd) is an OFTP (ODETTE File Transfer Protocol) server for
MVS 3.8j: a monitor that runs as a started task, the `OFTP` utility for
TSO and batch, and an ISPF dialog. It speaks OFTP 2.0 without its security
features, and OFTP 1.4 and 1.3, over plain TCP/IP. It is meant for
hobbyist and test systems, not for production use: there is no TLS and
no certificate. The utilities reach the monitor through an SVC of its
own, so the monitor knows who is asking.

This package installs it through SMP 4, the SMP that comes with MVS 3.8j.

## What you need

- MVS 3.8j on Hercules with TCP/IP for MVS programs (Hercules' DYN75
  instruction, as TK4-, TK5 and MVS/CE have it), and SMP usable through
  the `SMPREC` and `SMPAPP` procedures.
- A userid that may create the data sets below and submit jobs.
- For the dialog: ISPF (Wally Mclaughlin's, as on TK5 and MVS/CE) and a
  screen of more than 24 rows: the main menu and the send panel do not
  fit on 24. The panels are made for 43 rows (3278/3279 model 4) and use
  the width of a wider screen. On 24 rows TK5's ISPF does not say what is
  wrong: it shows `ISPF SERVICE ERROR DISPLAY` with `SERVICE NAME DISPLAY`
  and `RETURN CODE 20`. Ask your 3270 emulator for model 4.

## The package

| File | What it is |
|---|---|
| `oftpd-<v>-load.xmit` | the load modules `OFTPD`, `OFTP` and `OFTPI` |
| `oftpd-<v>-samplib.xmit` | `OFTPD.SAMPLIB`: the procedure, sample settings and jobs |
| `oftpd-<v>-ispplib.xmit` | `OFTPD.ISPPLIB`: the dialog's panels |
| `oftpd-<v>-alloc.jcl` | allocates the SMP target and distribution libraries, once |
| `oftpd-<v>-inst.jcl` | receives the three files and installs the SYSMOD |

The SYSMOD is a `++FUNCTION` with the FMID `TOFT` + version digits:
0.1.0 is `TOFT010`.

## 1. Check that the FMID is free

```
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 LIST CDS SYSMOD(TOFT010) .
 LIST ACDS SYSMOD(TOFT010) .
/*
```

Return code 4 and nothing listed means it is free. Anything listed, even
an entry showing only `TYPE = FUNCTION` and `DELBY = ...`, means it is
taken: stop and ask.

## 2. Upload the three TRANSMIT files

Upload `-load.xmit`, `-samplib.xmit` and `-ispplib.xmit` **in binary**
into sequential data sets with `RECFM=FB,LRECL=80`, for example
`HERC01.OFTPD.LOAD.XMIT`, `HERC01.OFTPD.SAMPLIB.XMIT` and
`HERC01.OFTPD.ISPPLIB.XMIT` (IND$FILE, FTP in binary, or the mvsMF
REST API).

## 3. Allocate and install

1. Submit `oftpd-<v>-alloc.jcl` once. It allocates `OFTPD.LINKLIB` and
   `OFTPD.AOFTPLOD`; edit `UNIT=` or add a `VOL=SER=` if `SYSDA` is not
   what you want.
2. In `oftpd-<v>-inst.jcl`, replace the three `CHANGE.ME...` names with
   the data sets of step 2, then submit it. On MVS/CE, TSO `RECEIVE`
   needs a volume: add `VOLUME('PUB000')` (or another volume of yours)
   to each of the three `RECEIVE` commands, as in

   ```
     RECEIVE INDSN('IBMUSER.OFTPD.LOAD.XMIT') -
       DATASET('OFTPD.OFTPLOAD') -
       VOLUME('PUB000')
   ```

The job deletes and re-receives `OFTPD.SAMPLIB` and `OFTPD.ISPPLIB`, so
copy members out of them before you change them, and never edit them in
place. The copies you make in steps 5 to 7 (`SYS2.PARMLIB`,
`SYS2.PROCLIB`) are not touched by a re-install.

**Check the result by the libraries, not by the condition codes.** SMP
can end with condition code 0 and have copied nothing. In the job output
of `APPLY`, look for

```
HMA2380 COPY SUCCESSFUL - MOD=OFTPD ...
```

for each of `OFTP`, `OFTPD` and `OFTPI`, and list the members of
`OFTPD.LINKLIB`: all three must be there.

## 4. Authorization

OFTPD asks for authorization when it starts, as UFSD does: it runs
authorized from an APF-authorized library, and otherwise asks for it
through SVC 244, where RAKF permits that. To authorize `OFTPD.LINKLIB`,
add it with its volume to `SYS1.PARMLIB(IEAAPF00)`; MVS 3.8j reads that
member only at IPL.

The log (section 7) says which route it took: `OFTP004P AUTHORIZED BY
LIBRARY` or `OFTP004P AUTHORIZED BY SVC`. This release needs no
authorization yet, so `OFTP005W NOT AUTHORIZED, RC=n, CONTINUING` (on the
console as well) is a warning only; later releases will need it.

## 5. The spool

OFTPD keeps every transfer in a VSAM KSDS, its spool. Copy
`OFTPD.SAMPLIB(OFTPSPL)`, edit the cluster name and a volume with VSAM
space owned by your catalog, and submit it once. A finished transfer --
delivered, confirmed, rejected or failed -- stays in the spool for
`KEEPDAYS` days after its last change (7 by default); OFTPD deletes the
older ones once a day. Size the spool for what arrives and leaves in that
time. Only spool entries are deleted, never data sets. A short spool also
keeps `LIST` fast (section 8).

The spool belongs to OFTPD alone: the utilities and the dialog never open
it, they ask OFTPD.

The spool also holds the partners kept by command (section 6), **with their
passwords in clear text**, as `OFTPPTN` does. Protect the cluster and every
backup or copy of it the way you protect `OFTPPTN`: only OFTPD's userid and
the administrators may read it. If OFTPD cannot read the partner records at
start it stops with `OFTP019E` instead of starting with the parmlib table
alone, which could bring back a partner that was disabled by command.

## 6. Settings and partners

Copy `OFTPD.SAMPLIB(OFTPPRM0)` and `(OFTPPTN0)` to `SYS2.PARMLIB` and edit
them. Each member describes its keywords at the top.

- `OFTPPRM0`: this system's Odette ID (`ODETTEID`, required), the OFTP
  port (`PORT`, 3305), the SVC the utilities reach OFTPD through
  (`SVC=237`; without it they find no OFTPD), the userids that may manage
  every transfer (`ADMIN=`, one per userid; without a security product
  nobody can be one, section 8), and the prefixes for received files
  (`INPREFIX`, `TMPPREFIX`). A received file is stored as
  `INPREFIX.<virtual file name>`. `RELAY=Y` makes this system a node that
  passes files on to its partners. `KEEPDAYS` is how long finished
  transfers stay in the spool (section 5; `0` keeps them for ever).
- `OFTPPTN0`: one `PARTNER=` entry per partner, with its Odette ID, the
  passwords both ways, and `HOST`/`PORT` if this system is to call it. A
  partner reached through another one gets `VIA=name` instead of a host.
  `SESSIONS=n` lets up to n sessions with the partner run at once (1 by
  default); give both sides the same number, since a call beyond the
  other side's limit is refused and made again after a short pause.

`OFTPPTN0` is the seed. Partners can also be added, changed, disabled and
deleted while OFTPD runs, from the `OFTP` utility and the ISPF dialog,
without a parmlib edit or a restart; OFTPD keeps those in its spool and
the spool wins by name. `docs/partners.md` describes both.

The sockets are the Hercules host's: `127.0.0.1` is the machine Hercules
runs on, not MVS, and the OFTP port is opened there.

### Receive rules

Without any rule a received file is stored as `INPREFIX.<virtual file
name>`; if that name exists already, OFTPD adds the transfer number as a last
qualifier (`INPREFIX.<virtual file name>.T0000012`) and refuses the file only
if that exists too. A rule changes this for the files it matches. Copy
`OFTPD.SAMPLIB(OFTPRCV0)` to `SYS2.PARMLIB` and add rules; the sample
describes every keyword at the top and has two examples. A member with only
comments changes nothing.

OFTPD itself does not need the `OFTPRCV` DD: without it there are no rules,
and the start says nothing about it. It is in the sample procedure, which
points it at the parmlib member (`R=OFTPRCV0`). So either copy `OFTPRCV0` to
the parmlib or delete the DD from your copy of the procedure.

```
RULE=ORDERS  FROM=PARTNER1  VFN=ORDERS.*
             DSN=PROD.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
RULE=INVOICE FROM=*  VFN=INV?????.DATA
             DSN=ACCT.IN.%ID.%VFN  REPLACE=Y  VOLSER=WORK01  SPACE=30
```

`RULE=name` opens a rule (1 to 8 characters, unique) and the keywords after
it belong to it, written as in the partner member. Lines are read to column
72 (columns 73 to 80 are ignored, so sequence numbers do no harm); `*` in
column 1 starts a comment. Keywords and values are not case sensitive. A
member holds at most 64 rules.

| Keyword | Meaning |
|---|---|
| `FROM=` | a partner name from `OFTPPTN0`, or `*` (the default) for any |
| `VFN=` | the virtual file name; `*` is any number of characters, `?` exactly one, case does not matter (default `*`) |
| `DSN=` | the data set name, with the symbols below, at most 80 characters; without it the file gets the `INPREFIX` name, even if the rule matched |
| `REPLACE=` | `Y`: an existing data set is replaced once the new file has been received and closed; `N` (default): the file is refused. Both need `DSN=`: without it nothing is replaced or refused, the name is made unique with the transfer number as described above |
| `UNIT=` `VOLSER=` | where the data set is allocated, instead of the `OFTPPRM0` values |
| `SPACE=` | primary space in tracks (1 to 9999), the secondary is half of it plus one; used only when the sender's file description gives no size |
| `XLATE=` | `Y` or `N`: translate this file or not, instead of what the partner entry says |
| `JOB=` | `pds(member)`: a job to run once the file has been received (see below) |

**The first rule in member order that matches the partner and the file
name decides**, so put the narrow rules before the wide ones. If none
matches, the file is treated as it was before rules existed. The rule is
chosen once, when the file is opened: a `F OFTPD,REFRESH` while the file is
arriving does not change it. A file that OFTPD passes on (`RELAY=Y`) never
meets a rule; rules are about files that end here.

Symbols in `DSN=`; everything else is taken literally (letters, digits,
`@ # $ - .`):

| Symbol | Value |
|---|---|
| `%VFN` | the virtual file name |
| `%Q1` to `%Q9` | its first to ninth qualifier (between the dots) |
| `%ID` | the partner's name |
| `%YYYY` `%MM` `%DD` `%HH` `%MI` `%SS` | the time the file arrived |
| `%CTR` | the 4 digits of the sender's file counter; a file whose time field has no such counter is refused (reason 01) |
| `%KEY` | the transfer's spool key, `T` and 7 digits |

The name is checked as a data set name when the file arrives: qualifiers of
1 to 8 characters starting with a letter or a national character, 44 in all.
A date in a qualifier therefore needs a letter in front of it and at most 7
characters in all: `Y%YYYY` is 5, `D%MM%DD` is 5, `T%HH%MI%SS` is 7, but
`D%YYYY%MM%DD` is 9 and comes out invalid. A name that comes out invalid
refuses the file with reason 01 (the sender does not retry) and the console
shows `OFTP224W` with the rule's name. A file that exists with `REPLACE=N`
is refused with reason 13 and `OFTP225W`; the sender tries again later, so
delete the data set or give the rule `REPLACE=Y`.

With `REPLACE=Y` the old data set is renamed out of the way only when the
new file is complete and closed, then the new one takes its name and the old
one is deleted (`OFTP229I`). A file that fails on the way costs nothing. A
data set that is in use makes the replace fail: the file is refused at the
end (reason 12, `OFTP221E` or `OFTP226E`) and the old data set stays.
`REPLACE=Y` works on a plain sequential data set only: if the name is a PDS
or a VSAM cluster, the file is refused (reason 12, `OFTP234W`) and
nothing is touched.

While a replace is running, the old content is under the name
`TMPPREFIX.B<key>`; protect `TMPPREFIX` like the data sets your rules write
to. If OFTPD or the system stops in the instant between the two renames,
that backup is the **only copy** of the old data set. At the next start OFTPD
renames it back if the target is missing (`OFTP239I`); if both exist it
leaves both and says `OFTP248W`. If you have to do it by hand: look whether
the target data set exists. If it does not, rename `TMPPREFIX.B<key>` back
to the target name. Delete the backup only when the target exists and is the
file you want.

**The first qualifier must be fixed.** `%VFN`, `%Q1` to `%Q9` and `%CTR`
carry text chosen by the partner into the data set name; if they could
stand in the first qualifier, a partner could create -- and with
`REPLACE=Y` replace -- any data set the OFTPD started task may write.
OFTPD therefore refuses a rule whose `DSN=` has any of them before the
first `.` (`DSN=%VFN`, `DSN=PROD%VFN` and `DSN=%Q1.X` are errors, `DSN=ACCT.IN.%VFN`
is fine; the member is not used, see `OFTP010W`). A `DSN=` that can name a
data set under your `TMPPREFIX` is refused for the same reason. Even so, use
`REPLACE=Y` only for a partner and a name you trust.

### A job after receipt

`JOB=ACCT.JCL(ORDERS)` makes OFTPD read that member once the file is under
its final name, put the symbols below into it, and give it to the internal
reader. The EERP does not wait for the job: it says the file has arrived,
not that anything was done with it. The data set name and the member are
read when the file is received, so the member can be changed any time.

| Symbol | Value |
|---|---|
| `?PFILE` | the data set the file was received as |
| `?VFILE` | the virtual file name |
| `?ID` | the partner's name |
| `?KEY` | the transfer's spool key, `T` and 7 digits |

The names are upper case. A name ends at the first character that is no
letter or digit, so `?ID.X` and `?ID-?KEY` are symbols, while `?IDX`,
`?PFILE1` and `?KEYS` are other names and stay as they are. A `?` followed by
anything else stays as it is as well.
A JCL statement (a line starting `//`) has its text in columns 1 to 71
(72 is the continuation mark); any other line has 80. A line that comes
out longer, after the symbols went in, is **not** cut: the job is not
submitted. The file is kept and acknowledged all the same, and the console
says `OFTP247W` with the reason. A job is
not submitted either when the member is not there, is empty, or has a
record past column 80, or its record format is undefined (`RECFM=U`) or
its record length is not positive, or it is read to a different end the
second time (`MEMBER CHANGED DURING READ`: it was edited meanwhile, or the
read failed). The record of the transfer says what happened (`OFTP SHOW`
prints `job submitted JOBnnnnn (JES job id)`, or `job not submitted:`
and the reason); a submitted job is `OFTP246P` in the log.

The virtual file name is chosen by the partner and ends up in the JCL, so
`?VFILE` stands only for letters, digits and `. - _ @ # $`: a name with
another character (a quote, a blank, a comma, `&`) does not get a job.
The other three are made by OFTPD.

That filter protects the card, not the meaning. `SYS1.PARMLIB`, `IEFBR14`
and `A.B.C` all pass it, and each is a valid name in JCL. So the partner's
file name must never decide by itself what the job touches or runs:

- put `?VFILE` only behind a fixed high-level qualifier the partner cannot
  leave, as in `DSN=ACCT.IN.?VFILE`;
- never as a whole name (`DSN=?VFILE`), never as `PGM=?VFILE`, never inside
  `PARM=`, never on the job card, and never in an operand that names an
  identity: `USER=`, `PASSWORD=` or `GROUP=`;
- keep the `VFN=` pattern of the rule as narrow as the application allows
  (`VFN=INV?????.DATA`, not `VFN=*`).

A partner can have one job queued for every file it may send under a rule
that matches, so `VFN=` is also what limits how many. A partner name used
with `?ID` should be a plain name: `OFTPPTN0` does not restrict its
characters, and OFTPD does not check `?ID` the way it checks `?VFILE`.
`?PFILE` is the name OFTPD built from the rule's `DSN=`: a valid data set
name, but one with `%VFN` in it carries the partner's text.

**Whose job it is.** It goes through the internal reader of the OFTPD
started task, but it does not run as that task. OFTPD sets no `USER=`, and
on a system with RAKF a job without one starts as the default user
(`PROD`, measured), not as the started task. The job can do what that user may do under your RAKF
profiles, and that can be a lot. What the default user may do on your
system is not something OFTPD knows or this manual can say: before you
write `JOB=`, look it up in your profiles, and give that user only the
access the jobs need, or let the member carry a `USER=` and `PASSWORD=` of
its own on the job card; a `PASSWORD=` there is plain text in the member,
so protect the member accordingly. A job that fails there fails in the JES log of the
job, not in OFTPD: `job submitted` says only that JES took it, and not that
the member was read to its end (`rread()` does not tell an end of file from
a read error). OFTPD checks nothing more; the member is named in the rules
member, so whoever may change that member decides what runs. Protect the
rules member and the library of the jobs like `SYS2.PARMLIB` itself.

Not part of it: a job after a send, a job when a send fails (`ERRJOB`), a
job for a file that was refused.

`F OFTPD,REFRESH` reads the rule member again together with the other two,
and replaces the whole set. **A member with any error is not used at all**:
the lines in error are reported with `OFTP010W`, `OFTP025E` says the old
rules stay, and the files that arrive meanwhile follow the old rules. A
good member is acknowledged with `OFTP024I RECEIVE RULES: n READ`, at start
as well. `OFTP026W` means the DD cannot be opened now and the old rules
stay. A rule for a partner that is not in `OFTPPTN0` is an error, so
refresh the partners and the rules together.

Not (yet) part of the rules: passing a file on, GDGs, SYSOUT and tape.

## 7. The procedure

Copy `OFTPD.SAMPLIB(OFTPD)` to `SYS2.PROCLIB`. Its `STEPLIB` already names
`OFTPD.LINKLIB`. Its defaults are `SYS2.PARMLIB` (members `OFTPPRM0`,
`OFTPPTN0` and `OFTPRCV0`; `R=` names another rules member) and the spool `OFTP.SPOOL`; give your spool's name either in
the procedure (`S=`) or when starting it. TK5 has no `SYS2.PARMLIB`: put
the members into `SYS1.PARMLIB` and start with
`S OFTPD,D='SYS1.PARMLIB'` (or change `D=` in the procedure).

Under RAKF, the started task's userid may need update access to the
spool and the authority to create data sets under `INPREFIX` and
`TMPPREFIX`. Receive rules (section 6) name their data sets themselves,
under any first qualifier, so it may also need ALTER on those: to create a
data set, and with `REPLACE=Y` to rename and delete the old one.

Start it with `S OFTPD` (or `S OFTPD,S='MY.SPOOL'`). The console shows
`OFTP000I` with the version and `OFTP001I OFTPD READY`; a partner entry
with an error is reported with `OFTP010W` and disabled, the others still
work. The rest of the start -- `OFTP004P` (step 4), `OFTP014P` with the
Odette ID and the number of partners enabled, `OFTP051P LISTENING ON PORT
3305, SOCKET n` and `OFTP039P CROSS MEMORY ACTIVE ON SVC 237, SUBSYSTEM
OFT1` -- is in the log only.

### The log

`DD OFTPLOG` of the procedure is a SYSOUT data set that gets every
message, one per line, with the date and time in front:

```
2026-10-20 14:03:51 OFTP211P PEER1: T0000012 PEER1.TEST.DATA SENT, 120 RECORDS
```

The console gets only the messages an operator must act on: start and
stop, errors, a partner that is not authorized, a transfer held or given
up. A message of class `P` (as in `OFTP211P`) goes to the log and never to
the console; classes `I`, `W` and `E` go to both. Every session message
begins with the partner's name, so a partner's lines can be picked out of
the log. Appendix A of the OFTPD Reference (ML09-0002) lists every message
with its destination.

- Give the DD `RECFM=F` or `FB` and say so in a `DCB=`, as the procedure
  does (`DCB=(RECFM=FB,LRECL=160,BLKSIZE=160)`). A SYSOUT DD without a DCB
  is refused with `OFTP022W`. A line is at most 143 characters with its
  time; a longer one is cut and ends in `>`. A record length below 143
  cuts lines the same way.
- JES2 keeps a SYSOUT data set for the job, and you read it when the
  started task ends or when you `$` it free; the console is the live view.
  With `SYSOUT=*` a started task's output usually goes to a class that
  JES2 prints and purges at once, so the log is gone when you look. Use a
  held class (`SYSOUT=H`, say) if you want to read it afterwards.
  There is no log switching and no size limit: stop and start OFTPD to get
  a new data set.
- Without the DD, or if it will not open, OFTPD says `OFTP022W` and
  sends everything to the console, class `P` included. If a write to the
  log fails later, it says `OFTP023E` once and does the same from then on.

The operator commands are listed at the top of the procedure: `F OFTPD,D P`
(partners), `D Q` (the spool), `D S` (sessions), `F OFTPD,CALL name`,
`F OFTPD,REFRESH` (read the partner and rules members again), `F OFTPD,HOLD name`
and `F OFTPD,RELEASE name` (every send to that partner, `ALL` for every
partner: queued and waiting ones are held, held ones queued again),
`F OFTPD,PURGE [days]`
(delete finished transfers older than that now; `KEEPDAYS` without a
number), `F OFTPD,TRACE ON|OFF [partner]` and `P OFTPD`.

`TRACE ON` logs every protocol command of every session, running ones
included, one `OFTP290P` line each, such as
`OFTP290P MVS2: TRACE RECV SFID 165: dsn PAYROLL.DATA fmt F lrecl 10`;
the DATA buffers of a run are one line. With a partner name only that
partner's sessions are traced; `TRACE OFF` without one ends all tracing.
The lines carry the commands' fields, never a password and never file
data. Wherever a message or a display shows a file name or an Odette ID
that came from a partner -- the trace lines, the other `OFTP290P` and
`OFTP290W` lines of a session, the monitor's transfer messages such as
`OFTP240P`, the operator display `D Q` and the transfer list and detail
display -- a character that cannot be printed shows as a dot. The spool keeps
the name as received.
`F OFTPD,D` and `D P` show what is traced (`OFTP127I`): `D` names the traced
partners, at least ten of them (more when the names are short; a longer list
is cut at the end of the line, marked with `>`), and `D P` marks every traced
partner.

Tracing is a diagnostic: switch it off when you are done. Every protocol
command is a line in the log, and a session that moves three small files
takes about 28 of them, so a busy partner fills the log data set quickly
(there is no size limit). With `TRACE ON` for all sessions, the first two
lines of a session a partner started, `SEND SSRM` and `RECV SSID`, carry the
caller's address instead of the partner name, because OFTPD does not know the
partner yet; a trace for one partner starts with the `RECV SSID` line for a
session that partner started, and with `RECV SSRM` for one OFTPD started.

The operator commands `PURGE`, `HOLD`, `RELEASE` and `CALL` take one
operand at most. A second one, as in `F OFTPD,HOLD P1,P2`, is refused with
`OFTP122E`; nothing is done for the first.

### As a batch job

`OFTPD.SAMPLIB(OFTPDJOB)` runs OFTPD as a batch job instead of a started
task: a first step defines the spool if it does not exist yet, the second
runs OFTPD until `P OFTPD`. Edit the names, the volume, the Odette ID and the
partner in it, as its comments say; it takes the settings in-stream, not
from the parmlib. **Its `STEPLIB` names `OFTPD.DEV.LINKLIB`, the library of
development builds.** An SMP installation has only `OFTPD.LINKLIB`
(section 3): change the `STEPLIB` to it, and authorize it as section 4
says. A development library has no place on a production system.

**Keep `TIME=1440` on the `EXEC` of OFTPD.** The step time limit of a batch
job is a limit on CPU time, and OFTPD uses CPU when it works. With the JES2
default (100 seconds of CPU on the system this was measured on) the step
ends with `S322` once that is used up. OFTPD then reports `OFTP098E OFTPD ABEND, n SOCKETS CLOSED, SVC
RESTORED`. The sample procedure `OFTPD` (section 7) has `TIME=1440` on its
`EXEC` too; keep it in a procedure of your own.

## 8. The OFTP utility

`OFTP` sends files and looks at the spool from batch and TSO;
`OFTPD.SAMPLIB(OFTPBAT)` shows both. In batch it reads its commands from
`SYSIN`:

```
//BATCH    EXEC PGM=OFTP
//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
SEND PARTNER(PARTNER1) DSN('MY.DATA') VFN(MY.FILE)
LIST MINE
/*
```

Under TSO it runs as the command `OFTP` when `OFTPD.LINKLIB` is in the
link list or in the logon procedure's `STEPLIB`, as in the `TSO` step of
`OFTPBAT`.

The commands are `SEND`, `LIST`, `SHOW`, `DELETE`, `HOLD`, `RELEASE`,
`RETRY`, `CALL`, `PARTNERS`, `PARTNER` (maintaining the partner table,
`docs/partners.md`) and `PURGE [DAYS(n)]`. `HOLD` and `RELEASE`
take a transfer's key, or `PARTNER(name)` (`*` for all) and optionally
`VFN(name)` or `VFN(prefix*)` for every send that matches and that you
may change. `OFTP` needs no authorization of its own:
it reaches OFTPD through OFTPD's SVC, and OFTPD reads the userid of the
job or TSO session itself.

`SEND` can wait: `STRTTIME(hh:mm)` and `STRTDATE(*|*+n|ddd)` (today, n
days on, or that day of the year) give the earliest start, local time;
`SENDWIN(hh:mm-hh:mm)` lets the send start only inside that window, every
day, also across midnight (`22:00-06:00`). A send already running is not
stopped by the end of its window. `SHOW` prints both.

`SEND ... DISP(DELETE)` deletes the data set once the partner has
confirmed it with its EERP, never earlier; a refusal (NERP) keeps it.
It needs the authority to delete the data set (ALTER) and takes a whole
data set, not a member.

`SEND` checks your authority, not OFTPD's: OFTPD asks the security
product (RAKF) whether your userid may read the data set, and refuses
with `not authorized` (RC 8) when it may not -- an administrator
included. So a data set needs the profile that protects it; under RAKF a
data set no profile covers may be read by everyone. On a system with a
security product, a caller that has no userid at all, an address space
without a security environment, is refused the same way, with
`not authorized: the caller has no userid` (RC 8), also for
`DISP(DELETE)`: there is no one whose authority could be checked.

### Who may see and change a transfer

With a security product (RAKF or RACF), a transfer belongs to the userid
that queued it. That userid sees and changes it; so does a userid named
in `ADMIN=`, which sees and changes every transfer, including the received
ones, which belong to nobody.
Anyone else gets `not your transfer` (RC 8). `LIST` shows an
administrator everything, and `LIST MINE` only their own.

Without a security product (the system has no RAKF or RACF) there is no
userid to tell callers apart, and OFTPD does not try: **every caller sees
and changes every transfer**. `LIST`, `SHOW`, `HOLD`, `RELEASE`, `RETRY`
and `DELETE` work on any transfer, whoever queued it, the received ones
included, and `LIST MINE` lists them all. A `SEND` is not checked against
a data set profile, since there is none. So on such a system give OFTPD
only to people who may touch each other's transfers.

`CALL`, `PURGE`, `REFRESH` and the `PARTNER` commands that change the
partner table (`ADD`, `CHANGE`, `ENABLE`, `DISABLE`, `DELETE`) need a userid
in `ADMIN=` in either case. Without a security product there is no security
environment and so no userid, and OFTPD refuses all of them. `CALL`, `PURGE`
and the `PARTNER` commands come from the `OFTP` utility or from the ISPF
dialog (its `C` line command on the partner list sends a `CALL`, its `M`,
`E`, `X` and `D` the `PARTNER` commands); the utility prints
`needs ADMIN= in OFTPPRM` (RC 8). `REFRESH` is sent by the dialog only,
after you edit the partner member (section 9). `PARTNER LIST` and `SHOW`
work for anyone. So on a system without a security product partners are
maintained in one way only: edit `OFTPPTN0` and give the operator command
`F OFTPD,REFRESH`. The operator's `F OFTPD,CALL`, `F OFTPD,PURGE` and
`F OFTPD,REFRESH` (section 7) are not affected. `PURGE DAYS(0)` deletes
every finished transfer, however recent.

> **A defect** (oftpd#101): `LIST` and `LIST MINE`, in `OFTP` and in the
> dialog's transfer list, get slower than the number of transfers
> suggests. A long answer comes in pieces, and for every piece OFTPD reads
> the whole spool again, so the time grows with the square of the number of
> transfers in the spool. On a short spool the delay is small. On a
> long one a `LIST` can take seconds, and longer while the system is busy.
> `SHOW`, `DELETE` and `STATUS` do not grow with the spool the way `LIST`
> does. Until this is fixed, keep the spool short:
> `KEEPDAYS` (section 5) bounds how long finished transfers stay, and
> `PURGE` (an administrator's command) deletes them at once.

## 9. The ISPF dialog

The dialog `OFTPI` needs its panels in the `ISPPLIB` concatenation and
its module in the `ISPLLIB` concatenation, because it loads `ISPLINK` from
there. This ISPF has no `LIBDEF`, so add the libraries where your system
allocates them, usually in the CLIST that starts ISPF:

- `OFTPD.ISPPLIB` to the `ISPPLIB` allocation,
- `OFTPD.LINKLIB` to the `ISPLLIB` allocation.

Add both at the end of the concatenation. Measured on TK5: with
`OFTPD.ISPPLIB` (block size 3120) in front of `HERC01.ISP.PLIB` (block
size 27920), ISPF ended with `FUNCTION HAS ABENDED` on the next panel it
read. The cause is not known (no dump was analysed); with the library at
the end the dialog ran. `LISTDS 'name'` shows the block sizes of your
libraries. `OFTPD.LINKLIB` in `ISPLLIB` was only ever tried at the end.

Then start it with

```
SELECT CMD(OFTPI) NEWAPPL(OFTP)
```

for example as an entry in a menu panel: a line for the option in the
body, and `O,'CMD(OFTPI) NEWAPPL(OFTP)'` in its `TRANS` (replace `O` with
the option you choose). The dialog reaches OFTPD the way `OFTP` does,
through the SVC and with the same rules. Reading the partner member
again after option 4 is for an administrator; on a system without a
security product nobody is one, and OFTPD refuses it (section 8). Option 3 maintains the
partner table (`docs/partners.md`).

## Upgrading

**From 0.1.0:** the TCP control port is gone. Remove `CONTROLPORT=` from
`OFTPPRM0` (OFTPD reports it as an unknown keyword with `OFTP010W`) and add
`SVC=237`, without which the utilities and the dialog cannot reach OFTPD at
all.

**The procedure:** the sample procedure now has an `OFTPRCV` DD for the
receive rules (section 6). If you take the new procedure over your old one,
copy `OFTPRCV0` to your parmlib first, or delete the DD from your copy.

### On TK5

TK5 builds the allocations at logon, in `SYS1.CMDPROC(ISPLOGON)`, with the
REXX exec `$CONCAT` (ddname, data set, and `BOT` to add at the end). The
same procedure then runs `userid.CMDPROC(MYLOGON)` if there is one. Create
it:

```
PROC 0
CONTROL NOMSG,NOLIST,NOSYMLIST,NOCONLIST,NOFLUSH
RX $CONCAT ISPPLIB OFTPD.ISPPLIB BOT
RX $CONCAT ISPLLIB OFTPD.LINKLIB BOT
EXIT
```

and log on again. The menu entry goes into the "Additional applications"
panel (option `M` of the primary menu), `TSOAPPLS` in `SYS2.ISP.PLIB`. Do
not change that library: copy the member to a panel library of your own that
comes before `SYS2.ISP.PLIB` in `ISPPLIB` (check with `LISTA STATUS`),
and add the line and the `TRANS` entry there:

```
%   O @OFTPD       + Odette file transfer
...
                O,'CMD(OFTPI) NEWAPPL(OFTP)'
```

The line is the one of another entry with the name changed. Then
`ISPF`, `M`, `O` opens the dialog. Without the entry, option `6` of the primary
menu and `ISPEXEC SELECT CMD(OFTPI) NEWAPPL(OFTP)` do the same.

Option 4 asks for the partner member and keeps it in your ISPF profile.
The first time it offers `SYS2.PARMLIB(OFTPPTN0)`: on TK5 type the member
you use, for example `SYS1.PARMLIB(OFTPPTN0)`. The editor is TK5's
`REVEDIT`; leaving it with PF3 makes OFTPD read the member again.

### The test

`mbt test --tso --only ispf --target NAME` (`test/tso/ispf.lua`) goes
through the dialog over TN3270 on an 80x43 screen: the monitor, the
partner list, a held send, show, hold and delete in the transfer list,
the partner pick for the send panel, the maintenance of a partner (new,
modify, disable, enable, delete, and the two refusals of a delete), and
an edit of a copy of the partner member, which it opens again to see the
change kept. It needs OFTPD running with the test user in `ADMIN=`, the
entry `O` in the "Additional applications" panel, and the data set
`userid.OFTPD.SAMPLE` (any sequential data set, empty is enough; the
transfers are held and deleted again). It passes on TK5.

## Removing it

Stop OFTPD, delete the procedure and the parmlib members, take the
libraries out of the ISPF CLIST and, if you added it, the `IEAAPF00`
entry. Remove the SYSMOD from both zones with `UCLIN` and delete
`OFTPD.LINKLIB`, `OFTPD.AOFTPLOD`, `OFTPD.SAMPLIB`, `OFTPD.ISPPLIB` and
the spool. The FMID stays spent: a later release gets a new one.
