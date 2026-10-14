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
| `SYS1.CMDPROC(ISPFBAK)` | 2026-10-12 | **backup** of the `ISPF` CLIST before the change below, byte for byte (2080 bytes) | delete after restoring |

## Changed members

| Data set (member) | Since | Change | Undo |
|---|---|---|---|
| `SYSGEN.ISPF.PLIB(ISPMORE)` | 2026-10-12 | ISPF option M (More) got a line `O  OFTP  Odette file transfer (oftp370)` (since 2026-10-13 `(oftpd)`) and `O,'CMD(OFTPI) NEWAPPL(OFTP)'` in its `TRANS` | copy `ISPMOREB` over `ISPMORE` (binary: the panel holds non-ASCII attribute characters) |
| `SYS1.CMDPROC(ISPF)` | 2026-10-12 | `OFTP370.DEV.LINKLIB` appended to the `ISPLLIB` allocation (a continuation line), so the dialog module `OFTPI` and ISPLINK resolve from the ISPLLIB concatenation; since 2026-10-13 `OFTPD.DEV.LINKLIB` | copy `ISPFBAK` over `ISPF` (binary) |
| `SYS2.PROCLIB(OFTPD)` | 2026-10-14 | the spool default `S='OFTP.SPOOL'` became `S='IBMUSER.OFTP370.SPOOL'`, so a plain `S OFTPD` finds dev1's spool | copy `OFTPDB2` over `OFTPD` |
| `SYS2.PROCLIB(OFTPD)` | 2026-10-13 | `STEPLIB` `OFTP370.DEV.LINKLIB` -> `OFTPD.DEV.LINKLIB` | copy `OFTPDB1` over it (or delete the member, see above) |
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
