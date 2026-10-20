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
  the width of a wider screen.

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

The console says which route it took: `OFTP004I AUTHORIZED BY LIBRARY` or
`OFTP004I AUTHORIZED BY SVC`. This release needs no authorization yet, so
`OFTP005W NOT AUTHORIZED, RC=n, CONTINUING` is a warning only; later
releases will need it.

## 5. The spool

OFTPD keeps every transfer in a VSAM KSDS, its spool. Copy
`OFTPD.SAMPLIB(OFTPSPL)`, edit the cluster name and a volume with VSAM
space owned by your catalog, and submit it once. A finished transfer --
delivered, confirmed, rejected or failed -- stays in the spool for
`KEEPDAYS` days after its last change (7 by default); OFTPD deletes the
older ones once a day. Size the spool for what arrives and leaves in that
time. Only spool entries are deleted, never data sets.

The spool belongs to OFTPD alone: the utilities and the dialog never open
it, they ask OFTPD.

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

The sockets are the Hercules host's: `127.0.0.1` is the machine Hercules
runs on, not MVS, and the OFTP port is opened there.

Coming from 0.1.0: the TCP control port is gone. Remove `CONTROLPORT=`
from `OFTPPRM0` (OFTPD reports it as an unknown keyword with `OFTP010W`)
and add `SVC=237`, without which the utilities and the dialog cannot
reach OFTPD at all.

## 7. The procedure

Copy `OFTPD.SAMPLIB(OFTPD)` to `SYS2.PROCLIB`. Its `STEPLIB` already names
`OFTPD.LINKLIB`. Its defaults are `SYS2.PARMLIB` (members `OFTPPRM0` and
`OFTPPTN0`) and the spool `OFTP.SPOOL`; give your spool's name either in
the procedure (`S=`) or when starting it.

Under RAKF, the started task's userid may need update access to the
spool and the authority to create data sets under `INPREFIX` and
`TMPPREFIX`.

Start it with `S OFTPD` (or `S OFTPD,S='MY.SPOOL'`). The console shows
`OFTP000I` with the version, `OFTP004I` (step 4), `OFTP014I` with the Odette ID and the number
of partners enabled, `OFTP001I OFTPD READY`, `OFTP051I LISTENING ON PORT
3305, SOCKET n` and `OFTP039I CROSS MEMORY ACTIVE ON SVC 237, SUBSYSTEM OFT1`. A partner entry
with an error is reported with `OFTP010W` and disabled; the others still
work.

The operator commands are listed at the top of the procedure: `F OFTPD,D P`
(partners), `D Q` (the spool), `D S` (sessions), `F OFTPD,CALL name`,
`F OFTPD,REFRESH` (read the partner member again), `F OFTPD,HOLD name`
and `F OFTPD,RELEASE name` (every send to that partner, `ALL` for every
partner: queued and waiting ones are held, held ones queued again),
`F OFTPD,PURGE [days]`
(delete finished transfers older than that now; `KEEPDAYS` without a
number), `F OFTPD,TRACE ON|OFF [partner]` and `P OFTPD`.

`TRACE ON` logs every protocol command of every session, running ones
included, one `OFTP290I` line each, such as
`OFTP290I MVS2: TRACE RECV SFID 165: dsn PAYROLL.DATA fmt F lrecl 10`;
the DATA buffers of a run are one line. With a partner name only that
partner's sessions are traced; `TRACE OFF` without one ends all tracing.
The lines carry the commands' fields, never a password and never file
data. `F OFTPD,D` and `D P` show what is traced (`OFTP127I`).

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
`RETRY`, `CALL`, `PARTNERS` and `PURGE [DAYS(n)]`. `HOLD` and `RELEASE`
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
that queued it. That userid sees and changes it; so does a userid named in
`ADMIN=`, which sees and changes every transfer, including the received
ones, which belong to nobody. Anyone else gets `not your transfer`
(RC 8). `LIST` shows an administrator everything, and `LIST MINE` only
their own.

Without a security product (the system has no RAKF or RACF) there is no
userid to tell callers apart, and OFTPD does not try: **every caller sees
and changes every transfer**. `LIST`, `SHOW`, `HOLD`, `RELEASE`, `RETRY`
and `DELETE` work on any transfer, whoever queued it, the received ones
included, and `LIST MINE` lists them all. A `SEND` is not checked against
a data set profile, since there is none. So on such a system give OFTPD
only to people who may touch each other's transfers.

`CALL`, `PURGE` and `REFRESH` need a userid in `ADMIN=` in either case.
Without a security product there is no security environment and so no
userid, and OFTPD refuses all three. `CALL` and `PURGE` are utility
commands; the utility prints `needs ADMIN= in OFTPPRM` (RC 8). `REFRESH`
is sent by the dialog only, after you edit the partner member (section 9).
The operator's `F OFTPD,CALL`, `F OFTPD,PURGE` and `F OFTPD,REFRESH`
(section 7) are not affected. `PURGE DAYS(0)` deletes every finished
transfer, however recent.

## 9. The ISPF dialog

The dialog `OFTPI` needs its panels in the `ISPPLIB` concatenation and
its module in the `ISPLLIB` concatenation, because it loads `ISPLINK` from
there. This ISPF has no `LIBDEF`, so add the libraries where your system
allocates them, usually in the CLIST that starts ISPF:

- `OFTPD.ISPPLIB` to the `ISPPLIB` allocation,
- `OFTPD.LINKLIB` to the `ISPLLIB` allocation.

Then start it with

```
SELECT CMD(OFTPI) NEWAPPL(OFTP)
```

for example as an entry in a menu panel: a line for the option in the
body, and `O,'CMD(OFTPI) NEWAPPL(OFTP)'` in its `TRANS` (replace `O` with
the option you choose). The dialog reaches OFTPD the way `OFTP` does,
through the SVC and with the same rules. Reading the partner member
again after option 4 is for an administrator; on a system without a
security product nobody is one, and OFTPD refuses it (section 8).

## Removing it

Stop OFTPD, delete the procedure and the parmlib members, take the
libraries out of the ISPF CLIST and, if you added it, the `IEAAPF00`
entry. Remove the SYSMOD from both zones with `UCLIN` and delete
`OFTPD.LINKLIB`, `OFTPD.AOFTPLOD`, `OFTPD.SAMPLIB`, `OFTPD.ISPPLIB` and
the spool. The FMID stays spent: a later release gets a new one.
