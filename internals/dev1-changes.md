# Changes oftpd made to dev1's system libraries

dev1 is a shared stand. Everything oftpd (until 2026-10-13 oftp370) changed outside its own data
sets is listed here, with how to undo it. All of it was approved by the
maintainer at the time; the dates are when it was done.

dev1 was moved to the new names on 2026-10-13: OFTPD and the dialog run
from `OFTPD.DEV.LINKLIB`. The spool keeps its name `IBMUSER.OFTP370.SPOOL`
(a start parameter, `S=`), as do the other `IBMUSER.OFTP370.*` data sets.

## New members

| Data set (member) | Since | What it is | Undo |
|---|---|---|---|
| `SYS2.PROCLIB(OFTPD)` | 2026-10-12 | the started task procedure, `samplib/oftpd` with `@LINKLIB@` = `OFTP370.DEV.LINKLIB` | delete the member |
| `SYS2.PARMLIB(OFTPPRM0)` | 2026-10-12 | OFTPD's settings for dev1 (Odette ID, ports, prefixes, `RELAY=Y`) | delete the member |
| `SYS2.PARMLIB(OFTPPTN0)` | 2026-10-12 | OFTPD's partners for dev1 (MENDEL, since 2026-10-14 MAC1; BEHIND, MACA, MACB; the test network's M38 nodes) | delete the member |
| `SYSGEN.ISPF.PLIB(OFTP@PRI)`, `(OFTPSND)`, `(OFTPLST)`, `(OFTPSHW)`, `(OFTPPTL)`, `(OFTPCFG)`, `(OFTPMON)` | 2026-10-12 | the ISPF dialog's panels, `ispplib/`; replaced by the 80x43 versions 2026-10-13 | delete the seven members |
| `SYSGEN.ISPF.PLIB(OFTPLSW)` | 2026-10-13 | the transfer list for 110 columns and more, `ispplib/` | delete the member |
| `SYSGEN.ISPF.PLIB(OFTB@PRI)`, `(OFTBSND)`, `(OFTBLST)`, `(OFTBSHW)`, `(OFTBPTL)`, `(OFTBCFG)`, `(OFTBMON)` | 2026-10-13 | **backup** of the seven panels above as of 2026-10-12, byte for byte (OFTP→OFTB in the name) | delete when no longer needed |
| `SYSGEN.ISPF.PLIB(ISPMOREB)` | 2026-10-12 | **backup** of `ISPMORE` before the change below, byte for byte (3760 bytes) | delete after restoring |
| `SYS2.PROCLIB(OFTPDB1)`, `SYS1.CMDPROC(ISPFBK1)`, `SYS2.PARMLIB(OFTPPTNB)` | 2026-10-13 | **backup** of `OFTPD`, `ISPF` and `OFTPPTN0` before the rename (2480, 2160, 800 bytes), byte for byte, read back | copy over the member to go back to `OFTP370.DEV.LINKLIB` and the old partners; delete when no longer needed |
| `SYS2.PROCLIB(OFTPDB2)` | 2026-10-14 | **backup** of `OFTPD` before its spool default changed (2480 bytes), byte for byte, read back | copy over `OFTPD`; delete when no longer needed |
| `SYS2.PROCLIB(OFTPDB19)`, `SYS2.PARMLIB(OFTPPR19)` | 2026-10-19 | **backup** of `OFTPD` and `OFTPPRM0` before the move to SVC 237 (2480, 480 bytes), binary, read back byte-identical | copy `OFTPPR19` over `OFTPPRM0`; delete when no longer needed |
| `SYS1.CMDPROC(ISPFBAK)` | 2026-10-12 | **backup** of the `ISPF` CLIST before the change below, byte for byte (2080 bytes) | delete after restoring |

## Changed members

| Data set (member) | Since | Change | Undo |
|---|---|---|---|
| `SYSGEN.ISPF.PLIB(ISPMORE)` | 2026-10-12 | ISPF option M (More) got a line `O  OFTP  Odette file transfer (oftp370)` (since 2026-10-13 `(oftpd)`) and `O,'CMD(OFTPI) NEWAPPL(OFTP)'` in its `TRANS` | copy `ISPMOREB` over `ISPMORE` (binary: the panel holds non-ASCII attribute characters) |
| `SYS1.CMDPROC(ISPF)` | 2026-10-12 | `OFTP370.DEV.LINKLIB` appended to the `ISPLLIB` allocation (a continuation line), so the dialog module `OFTPI` and ISPLINK resolve from the ISPLLIB concatenation; since 2026-10-13 `OFTPD.DEV.LINKLIB` | copy `ISPFBAK` over `ISPF` (binary) |
| `SYS2.PROCLIB(OFTPD)` | 2026-10-14 | the spool default `S='OFTP.SPOOL'` became `S='IBMUSER.OFTP370.SPOOL'`, so a plain `S OFTPD` finds dev1's spool | copy `OFTPDB2` over `OFTPD` |
| `SYS2.PROCLIB(OFTPD)` | 2026-10-13 | `STEPLIB` `OFTP370.DEV.LINKLIB` -> `OFTPD.DEV.LINKLIB` | copy `OFTPDB1` over it (or delete the member, see above) |
| `SYS2.PARMLIB(OFTPPRM0)` | 2026-10-19 | `PORT=3305 CONTROLPORT=3306` became `PORT=3305 SVC=237`: the control port is gone from `main` (#32), and the utilities reach OFTPD through SVC 237 | copy `OFTPPR19` over it, and run a build from before #32 |
| `SYS2.PARMLIB(OFTPPTN0)` | 2026-10-13 | the four test-network partners M38L, M38B (direct), M38X, M38R (`VIA=`) appended, `internals/testnet.md` | copy `OFTPPTNB` over it |

Why the CLIST and not `SYSGEN.ISPF.LLIB`: copying `OFTPI` into the ISPF
load library ended SD37 (JOB02144, its primary space is full); nothing was
copied. This ISPF has no `LIBDEF`, so the panels and the module must be in
the concatenations ISPF allocates.

## oftpd's own data sets on dev1

`OFTPD.DEV.LINKLIB` (WORK00, since 2026-10-13), `IBMUSER.OFTPD.SAMPLE`
(the ISPF test's file); `IBMUSER.OFTP370.*` (spool
`IBMUSER.OFTP370.SPOOL` on PUB000, test library, sample data),
`IBMUSER.OFTPIN.*` (received files), `IBMUSER.OFTPTMP.*` (files while
received or relayed).

## Operator actions that affected others

2026-10-12, about 15:03-15:10 MVS time: `P TSO`/`S TSO`, then `P TSO`,
`Z NET,QUICK`, `S NET` (VTAM restarted; it started TSO itself), because
TN3270 logons hung at the welcome screen; and `C U=IBMUSER` for a test
session left logged on. Since then a slot is asked from the MBT session
before any deploy, start/stop or operator command on dev1.

2026-10-13, slot from the MBT session, about 00:42-00:49 MVS time: `P
OFTPD`; `OFTP370.DEV.LINKLIB` reallocated by `mbt deploy --reallocate`
(144 tracks on WORK00, nothing held it: no TSO users, OFTPD stopped;
JOB02156 probe, JOB02157 deploy); `OFTPPRM0` changed for the test to
`RETRY=10 MAXRETRY=2`, OFTPD run as STC00639; then `OFTPPRM0` restored
from `OFTPPRMB` and compared byte for byte, OFTPD started again with the
normal settings as STC00640. Test records T0000013-T0000015 deleted from
the spool. The backup `SYS2.PARMLIB(OFTPPRMB)` was deleted afterwards
(maintainer's OK); `OFTPPRM0` compared again, unchanged.

2026-10-13, slot from the MBT session, about 03:25-04:00 MVS time: the
seven panels backed up as `OFTB*` and replaced, `OFTPLSW` added; a test
panel `SYSGEN.ISPF.PLIB(OFTPTST)` and the CLIST `IBMUSER.OFTP370.TSTPNL`
used and deleted again; `P OFTPD` / `mbt deploy` / `S OFTPD` three times
(JOB02163, JOB02164, JOB02165; STC00645, STC00646, now STC00647); TSO
logons as IBMUSER over TN3270 at 80x43 and 160x62, each logged off.

2026-10-13, slot from the MBT session, from about 06:30 MVS time: the
rename. Backups as above; `mbt deploy` allocated `OFTPD.DEV.LINKLIB` on
WORK00 and copied OFTP, OFTPD, OFTPI in; `OFTPD` PROC, `ISPF` CLIST,
`OFTPPTN0`, `ISPMORE` and the eight panels changed and each read back
(panels compared line by line with `ispplib/`); `P OFTPD` (STC00647),
`S OFTPD,S='IBMUSER.OFTP370.SPOOL'` -> STC00661, STEPLIB
`OFTPD.DEV.LINKLIB`, console `OFTP000I oftpd 0.1.0-dev (1ca9786)`,
`OFTP004I AUTHORIZED BY SVC` (the library is not APF-authorized: SVC 244
under RAKF, `RAKF0010I ... DEFAULT STC ACCOUNT`), `8 OF 8 PARTNERS
ENABLED`. `mbt test --tso --only ispf` passed (one IBMUSER logon,
logged off; D TS,L 0 users before and after).
`OFTP370.DEV.LINKLIB` scratched afterwards (maintainer's OK; HTTP 204,
no `OFTP370.*` data set left). No APF entry named it: `IEASYS00` says
`APF=00`, and `IEAAPF00` has no OFTP line.

2026-10-14, slot from the MBT session, from about 09:45 local time: the
Odette IDs of the test network renamed (`internals/testnet.md`,
"Renamed"). `OFTPPRM0` got `ODETTEID=O0000000000DEV1` (was
`O01791824916311`); in `OFTPPTN0` MENDEL became MAC1
(`O0000000000MAC1`, `PASSWORD=MAC1 OURPASSWORD=DEV1`, was `TEST` both
ways) and BEHIND `VIA=MAC1`. Both members backed up binary beforehand
(480 and 1360 bytes, kept off the stand) and read back after the write:
identical to what was meant. `P OFTPD` (STC00661); a plain `S OFTPD`
failed with a JCL error (`IEF212I ... OFTPSPL - DATA SET NOT FOUND`,
STC00674: the PROC's default spool name) and was repeated as
`S OFTPD,S='IBMUSER.OFTP370.SPOOL'` -> **STC00675**, 8 partners.
Undo: the two lines back to the old values.

Afterwards, in a short slot of its own: the PROC's spool default set to
dev1's cluster (backup `OFTPDB2` first, byte-identical; the PROC read back
as meant), `P OFTPD` (STC00675) and a plain `S OFTPD` -> **STC00676**,
`OFTP000I oftpd 0.1.0-dev (1ca9786)`, `OFTP004I AUTHORIZED BY SVC`,
`OFTP014I ODETTE ID O0000000000DEV1, 8 OF 8 PARTNERS ENABLED`, control
port answering.

Later that day, slot from the MBT session: the ESTAE test (#1 step 1).
`P OFTPD` (STC00676), `mbt deploy -target dev1 -module OFTPD` (an
IEBCOPY merge into `OFTPD.DEV.LINKLIB`; the build is the source of
d5b8bdc, its banner says `fbb96c6-dirty`), `S OFTPD` -> STC00688. Then
`C OFTPD`: `OFTP098E OFTPD ABEND, 2 SOCKETS CLOSED`, `ABEND S222`;
`ss -ltn` on mvsdev showed 3305 and 3306 free. No dump (SYSUDUMP empty).
The next start, STC00689, failed on the spool left not properly closed
(`IEC161I 056-084`, `OFTP020E`; #2); `VERIFY DATASET(IBMUSER.OFTP370.SPOOL)`
(JOB02258, CC 0, 36 records as before) mended it, and `S OFTPD` ->
**STC00690**, both ports listening. The outputs of STC00688, STC00689 and
JOB02258 purged afterwards (maintainer's OK).

Then the fix for #2, in a slot of its own: `P OFTPD` (STC00690), deploy
of `OFTPD` (source as committed next, banner `822ee57-dirty`), a plain
`S OFTPD` -> STC00691 opened the spool without a VERIFY (35 transfers).
`C OFTPD`; the next plain `S OFTPD` -> STC00692 printed `IEC161I
056-084` for data and index, then `OFTP021W SPOOL WAS NOT CLOSED (OPEN
RC 4), VERIFIED`, both ports listening, 35 transfers. So the open that
an abend leaves behind answers rc 4. `P OFTPD` and a plain `S OFTPD`
once more -> **STC00693**, no VERIFY, 35 transfers.

Step 2 of #1 (the control handlers answer through an output function):
`P OFTPD` (STC00693), deploy of `OFTPD` built from eeace93, a plain
`S OFTPD` -> **STC00694**, banner `(eeace93)`. Control-port answers
compared byte for byte with those taken from STC00693 just before:
`PARTNERS`, `LIST` (37 lines), `SHOW 0000001` and an unknown request
identical, `STATUS` only in its version line. `mbt test --tso --only
ispf` passed.

2026-10-17, slot from the MBT session, from about 11:17 MVS time: the
storage measurement for #3. `P OFTPD` (STC00715), `mbt deploy --module
OFTPD` (IEBCOPY merge into `OFTPD.DEV.LINKLIB`, a build with the display
`D M`, banner `b6e4ed5-dirty`), `S OFTPD` -> STC00765; 50 calls `F
OFTPD,CALL MAC1` to mendelson on the Mac, all ESID 00, no files. Largest
free block in subpool 0 fell from 7536 KB to 4096 KB, about 69 KB per
session (issue #3, comment of the same day). `P OFTPD` ended within a
second; `S OFTPD` -> **STC00766**, 7536 KB again. STC00765's output left
on the spool. `OFTPD.DEV.LINKLIB` is 94% full (180 tracks, 2 extents).

Then, in a second slot from the MBT session, the fix for #3 measured the
same way. `P OFTPD` (STC00766); the plain deploy refused (10 of 180
tracks free, 19 needed, nothing copied) and OFTPD was started on the old
build as STC00767. With the maintainer's OK: `P OFTPD` (STC00767), `D
TS,L` 0 users, `mbt deploy --reallocate` (deleted and allocated again on
WORK00 as 180 tracks, OFTP, OFTPD, OFTPI copied in, 21% used), `S OFTPD`
-> STC00768. 50 calls to MAC1, all ESID 00: `D M` read 7448 KB and 6
handles after every tenth call (the last session, freed at the
scheduler's next wake), 7536 KB and 5 handles 15 s later, as at the start.
`P OFTPD` ended within a second; `S OFTPD` -> **STC00769**, the fix
build. STC00767 and STC00768 left on the spool.

A third, short slot from the MBT session, about 12:09-12:11 MVS time:
which thread the fifth handle `D M` counts is. `P OFTPD` (STC00769),
`mbt deploy --module OFTPD` with a diagnostic `D M` that listed every
handle (never committed), `S OFTPD` -> STC00771, one `D M`: handle 0 is
the main task itself (no function, no stack), which `cthread_create_ex()`
registers as the owner when a task without a handle attaches its first
thread; the other four are the spool, listener, scheduler and
control-port threads. Then `P OFTPD`, deploy of the clean build, `S
OFTPD` -> **STC00772**, banner `15335f5`. STC00771 left on the spool.

2026-10-18, a slot from the MBT session, about 02:36-02:44 MVS time: the
engine's record buffer after a session abend (the open points of the #4
entry in `TODO.md`). Three builds, all from libc370 2.17.0 / cc370 1.6.3,
each `mbt deploy --module OFTPD` (IEBCOPY merge into `OFTPD.DEV.LINKLIB`)
after `P OFTPD`. The sender was `host/oftph` on the Mac as partner MACA,
one file per call, `OFTPD.TEST.ABEND`, F, LRECL 32760, so the debug
trigger in `f_write` ends each session in an S0C4 with the receive
buffer allocated.

- Debug build of main, banner `e20dfc3-dirty`, STC00781: 50 sessions,
  50 x `OFTP209E` and 50 x `OFTP203I ... ENDED BY ABEND S0C4`, no
  `OFTP216E`, no `OFTP096W`. `D M` largest free block: 7532 KB at the
  start, 7412 after the first, 6728 / 6372 / 5980 / 5624 after the 20th
  / 30th / 40th / 50th (the read after the 10th is missing): about 38 KB
  per abend, against a 32760-byte buffer. The 6 KB above the buffer are
  not explained. 5 handles throughout.
- Debug build with the fix, banner `2253a7a-dirty`, STC00782: the same
  50 sessions and messages. 7532 KB at the start, 7524 / 7520 / 7512 /
  7508 / 7504 after the 10th to the 50th: 28 KB over 50 abends, about
  0.6 KB each at the probe's 4 KB steps. Not explained; normal sessions
  stayed flat over 50 in the #3 slot, so it is particular to an abend.
- Clean build, banner **`2253a7a`**, **STC00783**, 7532 KB.

No `IBMUSER.OFTPTMP.*` data set and no spool record was left by the
100 abends. `OFTPD.DEV.LINKLIB` is at 76% of 180 tracks. STC00781 and
STC00782 left on the spool.

Found on the way, not caused by this slot: every OFTPD end on dev1 writes
a SYSUDUMP after `OFTP003I OFTPD ENDED`, with `COND CODE 0000` on the
step. Where session subtasks were still attached (before #3) it is
`SA03`, PSW `070C1000 00E0D054` (STC00661, 00675, 00715, 00765);
otherwise `S30A`, PSW `075C2000 00F18ACE` (STC00676, 00690, 00693,
00694, 00766-00769, 00771, 00772, 00781, 00782). In the dumps of
STC00772, 00781 and 00782 the abending TCB is a subtask of the job step
TCB (`JST` and `OTC` both point to it). Which thread it is has not been
looked at.

2026-10-19, in a block slot from the MBT session, about 11:17 MVS time:
dev1 on `main` (`7a9d9a9`, libc370 2.21.0) over SVC 237. Before, dev1
ran `2253a7a` on libc370 2.17.0, whose `rename()` cut a new name at 32
characters: a file from M38X as `IBMUSER.OFTPIN.M38X.SVC.TO.DEV1.F`
(33) failed with `OFTP221E ... NOT RENAMED` and `EFNA 12`
(`internals/m38-changes.md`). SVC 237 read before (JOB02461) `0000CCC8
80000000`, 40 of 56; `P OFTPD` (STC00813), `mbt deploy`, which left the
library at 98% of 180 tracks, so `mbt deploy --reallocate` while OFTPD
was down (22% afterwards). **The reallocation deleted the library**, and
its members were not listed before; afterwards it holds OFTP, OFTPD,
OFTPI and OFTPSVC, which is what `mbt deploy` puts there. `OFTPPRM0` as
in the table, read back identical; the PROC unchanged. `S OFTPD` ->
**STC00814**: `OFTP000I oftpd 0.1.0-dev (7a9d9a9)`, `OFTP004I AUTHORIZED
BY SVC`, `8 OF 8 PARTNERS ENABLED`, `OFTP031I ... FOUND 0000CCC8
80000000`, `OFTP039I CROSS MEMORY ACTIVE ON SVC 237`. SVC 237 active
(JOB02465) `00A904C8 C0000000`, 39 of 56; `OFTP` check JOB02466 CC 0,
8 partners. The ISPF dialog loads `OFTPI` from the same library, so it
reaches OFTPD through the SVC as well; not tried in this slot. In the
network retest (`m38-changes.md`) the 33-character name arrived, and a
41-character one too.
