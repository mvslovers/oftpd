# oftpd on the four M38 stands

The M38 stands are scheduled by the MBT session, one slot per stand. This
file lists what oftpd installed and changed on each, with job numbers, and
how to undo it. The members are those of `internals/testnet.md`.

## m38l (MVS/CE LAB), 2026-10-13, about 13:15-13:40 MVS time

Installed from the **release assets** of v0.1.0-dev, as a user would:
TOFT010 `REC APP ACC` in CDS and ACDS, owning MOD `OFTP`, `OFTPD`, `OFTPI`
(FMID and RMID TOFT010, both zones, `OFTPDCHK JOB01692`).

| Job | What |
|---|---|
| JOB01690 | alloc: `OFTPD.LINKLIB`, `OFTPD.AOFTPLOD` on WORK00 |
| JOB01691 | install: RECEIVE (with `VOLUME('PUB000')`) -> `OFTPD.SAMPLIB`, `OFTPD.ISPPLIB` on PUB000; 6 x `HMA2380`; all steps CC 0000 |
| JOB01688 | spool `IBMUSER.OFTPD.SPOOL`, KSDS UNIQUE on PUB000, catalog UCPUB000 |
| JOB01693 | M38L -> DEV1 test (`IBMUSER.OFTPD.TESTF`, an F and a T file) |
| JOB02215 (dev1) | DEV1 -> M38L test (`IBMUSER.OFTPD.SAMPLE`, an F and a T file) |

**A first install went wrong and was undone.** JOB01684/01685 installed the
local `dist/` package, which was built before the last commit of the
release (`3f6080d-dirty`, no `OFTP004I`). Found by the startup banner.
Undone with the maintainer's OK: `P OFTPD`, UCLIN of TOFT010 and its three
MODs and LMODs in both zones (`OFTPDUCL JOB01689`, 17 x `HMA2550`; the
REJECT step ended RC 12 `HMA2462 NOT FOUND ON SMPPTS`: the ACCEPT had
already removed it from the PTS), the five LISTs RC 04, `OFTPD.LINKLIB`
and `OFTPD.AOFTPLOD` scratched, then installed again from the release
(the jobs above). **Lesson: install from the release assets, and check the
banner in the load XMIT before uploading it.**

Why the spool is `IBMUSER.OFTPD.SPOOL`, not `OFTPD.SPOOL`: `OFTPD` has no
alias on MVS/CE, so `OFTPD.*` goes to the master catalog, which owns VSAM
space only on MVSRES and PAGE00 (`LISTCAT SPACE ALL`, JOB01686). The
`IBMUSER` alias points to UCPUB000 (`LISTCAT ALIAS ALL`, JOB01687), as on dev1; no catalog was changed.
The PROC names the spool with `S=`.

New members (none of the names existed before): `SYS2.PROCLIB(OFTPD)`
(the sample, `S='IBMUSER.OFTPD.SPOOL'`), `SYS2.PARMLIB(OFTPPRM0)` and
`(OFTPPTN0)` from `internals/testnet.md`, each read back identical.

Started as **STC00779**: `OFTP000I oftpd 0.1.0-dev (1ca9786)`, `OFTP004I
AUTHORIZED BY SVC` (`RAKF0010I ... DEFAULT STC ACCOUNT`), `5 OF 5
PARTNERS ENABLED`, ports 3315/3316. It keeps running for the network
tests. (STC00778 was the wrong level, stopped.)

Direct link M38L <-> DEV1: all four files delivered with the EERP back
(`D` on the sender, `C` on the receiver), each compared with its source:
identical. Received on m38l as `OFTPD.IN.DEV1.TO.M38L.F/.T` (WORK01;
the default STC account may create `OFTPD.*`), on dev1 as
`IBMUSER.OFTPIN.M38L.TO.DEV1.F/.T`.

Removed at the end: the three staging XMITs `IBMUSER.OFTPD.*.XMIT`.

Undo: `P OFTPD`; delete the three members; UCLIN as in JOB01689;
delete `OFTPD.LINKLIB`, `.AOFTPLOD`, `.SAMPLIB`, `.ISPPLIB`,
`OFTPD.IN.*`, `IBMUSER.OFTPD.TESTF` and the cluster `IBMUSER.OFTPD.SPOOL`.

## m38b (TK5, MVSTK5-BLD), 2026-10-13, about 15:45-15:55 MVS time

Installed from the release assets as HERC01; the load XMIT's banner
checked against the tag (`1ca9786`) before the upload, each upload read
back identical. TOFT010 was still free (`OFTPDCAT JOB00723`: CDS and ACDS
RC 04, no MOD OFTP/OFTPD/OFTPI); afterwards `REC APP ACC` in both zones,
MOD FMID=RMID=TOFT010 in both (`OFTPDCHK JOB00728`).

| Job | What |
|---|---|
| JOB00724 | alloc: `OFTPD.LINKLIB`, `OFTPD.AOFTPLOD` on TSO001 (UNIT=SYSDA) |
| JOB00725 | install, no `VOLUME` needed on TK5: `OFTPD.SAMPLIB`, `OFTPD.ISPPLIB` on TSO001; 6 x `HMA2380`; all steps CC 0000 |
| JOB00729 | spool `HERC01.OFTPD.SPOOL`, KSDS UNIQUE on TSO001, catalog SYS1.UCAT.TSO |
| JOB00730 | M38B -> DEV1 test (`HERC01.OFTPD.TESTF`, F and T) |
| JOB02216 (dev1) | DEV1 -> M38B test (`IBMUSER.OFTPD.SAMPLE`, F and T) |

The spool: `OFTPD` has no alias on TK5 either (master catalog
SYS1.MCAT.TK5, VSAM space on PAGE00 and TK5CAT only); `HERC01` points to
SYS1.UCAT.TSO (JOB00723). No catalog changed.

New members (none existed): `SYS2.PROCLIB(OFTPD)` (`S='HERC01.OFTPD.SPOOL'`),
`SYS2.PARMLIB(OFTPPRM0)`, `(OFTPPTN0)`, read back identical.

Started as **STC00071**: `OFTP000I oftpd 0.1.0-dev (1ca9786)`, **`OFTP004I
AUTHORIZED BY SVC`** -- the SVC 244 route works on TK5 as on MVS/CE --,
`5 OF 5 PARTNERS ENABLED`, ports 3335/3336. Keeps running.

Direct link M38B <-> DEV1: four files, all `D` at the sender with the EERP
back, each identical to its source. Received on m38b as
`OFTPD.IN.DEV1.TO.M38B.F/.T` (TSO002), on dev1 as
`IBMUSER.OFTPIN.M38B.TO.DEV1.F/.T`.

Removed at the end: the three staging XMITs `HERC01.OFTPD.*.XMIT`.
Undo as for m38l, with `HERC01.OFTPD.TESTF` and `HERC01.OFTPD.SPOOL`.

## m38x (MVS/CE EXP), 2026-10-13, about 13:50-14:00 MVS time

As on m38l, from the release assets (banner `1ca9786` checked, uploads
read back identical). TOFT010 free before (`OFTPDCAT JOB00605`, four LISTs
RC 04); afterwards `REC APP ACC` in both zones, MOD FMID=RMID=TOFT010 in
both (`OFTPDCHK JOB00608`).

| Job | What |
|---|---|
| JOB00606 | alloc: `OFTPD.LINKLIB`, `OFTPD.AOFTPLOD` on WORK00 |
| JOB00607 | install with `VOLUME('PUB000')`; 6 x `HMA2380`; all steps CC 0000 |
| JOB00609 | spool `IBMUSER.OFTPD.SPOOL`, UNIQUE on PUB000, UCPUB000 (same catalog layout as m38l, JOB00605) |
| JOB00610 | M38X -> M38L test (`IBMUSER.OFTPD.TESTF`, F and T) |
| JOB01694 (m38l) | M38L -> M38X test (`IBMUSER.OFTPD.TESTF` of m38l, F and T) |

New members (none existed): `SYS2.PROCLIB(OFTPD)`, `SYS2.PARMLIB(OFTPPRM0)`,
`(OFTPPTN0)`, read back identical.

Started as **STC00509**: `OFTP000I oftpd 0.1.0-dev (1ca9786)`, `OFTP004I
AUTHORIZED BY SVC`, `5 OF 5 PARTNERS ENABLED`, ports 3325/3326. Keeps
running.

Direct link M38X <-> M38L: four files, `D` at the sender, identical to
their sources; received as `OFTPD.IN.M38L.TO.M38X.F/.T` on m38x and
`OFTPD.IN.M38X.TO.M38L.F/.T` on m38l.

Removed at the end: the three staging XMITs `IBMUSER.OFTPD.*.XMIT`.

## m38r (TK5 REF), 2026-10-13, about 15:55-16:05 MVS time

As on m38b, from the release assets (banner `1ca9786` checked, uploads
read back identical). TOFT010 free before (`OFTPDCAT JOB00378`, four LISTs
RC 04; `OFTPD` no alias, `HERC01` -> SYS1.UCAT.TSO); afterwards `REC APP
ACC` in both zones, MOD FMID=RMID=TOFT010 in both (`OFTPDCHK JOB00381`).

| Job | What |
|---|---|
| JOB00379 | alloc: `OFTPD.LINKLIB`, `OFTPD.AOFTPLOD` on TSO001 |
| JOB00380 | install; 6 x `HMA2380`; all steps CC 0000 |
| JOB00382 | spool `HERC01.OFTPD.SPOOL`, UNIQUE on TSO001, SYS1.UCAT.TSO |
| JOB00383 | M38R -> M38B test (`HERC01.OFTPD.TESTF`, F and T) |
| JOB00732 (m38b) | M38B -> M38R test (`HERC01.OFTPD.TESTF` of m38b, F and T) |
| JOB00734 (m38b) | `OFTP LIST` over the control port: all eight transfers there DELIVERED/CONFIRMED |

New members (none existed): `SYS2.PROCLIB(OFTPD)`, `SYS2.PARMLIB(OFTPPRM0)`,
`(OFTPPTN0)`, read back identical.

Started as **STC00033**: `OFTP000I oftpd 0.1.0-dev (1ca9786)`, `OFTP004I
AUTHORIZED BY SVC`, `5 OF 5 PARTNERS ENABLED`, ports 3345/3346. Keeps
running.

Direct link M38R <-> M38B: four files, delivered with the EERP back,
identical to their sources; received as `OFTPD.IN.M38B.TO.M38R.F/.T`
(TSO002) on m38r and `OFTPD.IN.M38R.TO.M38B.F/.T` on m38b.

Removed at the end: the three staging XMITs `HERC01.OFTPD.*.XMIT`.

A console note: while other jobs run on a stand, `mbt target console`
can return their messages instead of the answer to `F OFTPD,D Q`;
`OFTP LIST` over the control port answers regardless.

## The network slot, 2026-10-13, about 16:55-17:10 local time

Tests in `internals/testnet.md` (Results). Changed on the way, and back:

- m38b: `SYS2.PARMLIB(OFTPPRM0)` for the step 5 test set to `RETRY=10
  MAXRETRY=2 RELAY=Y` (binary backup kept locally, 400 bytes), restored
  afterwards and read back byte-identical to the backup. OFTPD restarted
  three times: test settings (presumably STC00072: the number was not
  read), restored settings without control port (STC00073, `OFTP052W
  ... ERRNO 48`), then **STC00074** with the control port.
- m38r: `P OFTPD` for step 5, started again as **STC00034**.

Now running: dev1 STC00661, m38l STC00779, m38x STC00509, m38b STC00074,
m38r STC00034. Test files on the stands: `*.OFTPD.TESTF` and the received
`OFTPD.IN.*` / `IBMUSER.OFTPIN.*` (HOP1, HOP3, HOP4, NERP, EERP).

## The rename, 2026-10-14, about 09:55-10:05 local time

One block slot from the MBT session for all five stands. DEV1's Odette
ID became `O0000000000DEV1` and MENDEL became MAC1 (`O0000000000MAC1`),
`internals/testnet.md` "Renamed". On each M38 stand only
`SYS2.PARMLIB(OFTPPTN0)` changed: DEV1's `ODETTEID`, and the MENDEL
line became `PARTNER=MAC1 ODETTEID=O0000000000MAC1 VIA=...`. Backed up
binary beforehand (m38l and m38b 640 bytes, m38x and m38r 560, kept off
the stand), written, read back identical, then `F OFTPD,REFRESH` on the
running STC (no restart): `OFTP016I PARTNERS RELOADED: 5 ENABLED, 1 NEW,
1 DISABLED` on all four; `OFTP PARTNERS` shows DEV1 with the new ID,
MAC1 enabled and MENDEL disabled until the next start.

Tests: M38R -> MAC1 over three hops (m38r JOB00414, EERP back) and
MAC1 -> M38R through DEV1 (received as `OFTPD.IN.REN.MAC1.TO.M38R` on
m38r, EERP back to mendelson); both identical to their sources.

Undo: the two lines of each `OFTPPTN0` back to the old IDs and names,
then `F OFTPD,REFRESH`.

2026-10-14, later: `P OFTPD` and a plain `S OFTPD` on all four, to drop
the disabled MENDEL entry: `5 OF 5 PARTNERS ENABLED` and `F OFTPD,D P`
lists DEV1, MAC1 and the three other stands only, on each. The ISPF test
(`mbt test --tso --only ispf`, types MAC1) passed on dev1.

Now running: dev1 STC00676 (a plain `S OFTPD` since the PROC fix,
`internals/dev1-changes.md`), m38l STC00798, m38x STC00528, m38b STC00076,
m38r STC00038.

## m38b: the SVC transport, 2026-10-18, about 22:01-22:06 MVS time

Slot from the MBT session. A test instance `OFTPDT` beside the running
OFTPD, which was not touched; `internals/xmem.md` section 13, "TK5 stand
m38b". Everything it created had a name of its own and was absent before:
`SYS2.PROCLIB(OFTPDT)`, `SYS2.PARMLIB(OFTPPRMT)`, `(OFTPPTNT)`, and
`HERC01.OFTPDT.LOAD`, `HERC01.OFTPT.LOAD`, `HERC01.OFTPDT.SPOOL`,
`HERC01.OFTPDT.TESTA`. Port 3337, no control port.

SVC 237 read before, while active and after (JOB02714, 02720, 02725):
`00017068 80000000` again at the end. Removed at the end: the members
(mvsMF DELETE, then only OFTPD/OFTPPRM0/OFTPPTN0 left in the lists) and
the data sets (OFTPDEL JOB02727, CC 0); the job outputs JOB02714-02727
and STC00109 were purged. Left: `HERC01.OFTPD.V0R1M0D.TESTLIB`.

## m38x: OFTPD from main over SVC 237, 2026-10-19, about 14:33-15:05 MVS time

Slot from the MBT session, after the host outage of the same day. The
stand was in its 2026-10-14 state (MBT's read and ours): STEPLIB
`OFTPD.LINKLIB` only, `PORT=3325 CONTROLPORT=3326`, OFTPD running.

Backed up first, binary and as text, kept off the stand and compared
with each other: `SYS2.PROCLIB(OFTPD)` 2480 bytes, `SYS2.PARMLIB(OFTPPRM0)`
400, `(OFTPPTN0)` 560 (unchanged).

| What | Result |
|---|---|
| SVC 237 before (`JOB00645`, BREXX `STORAGE()`) | `0000CCC8 80000000`; modal EP `0000CCC8` in 40 of 200-255, 235-239 all free -- as on 2026-10-18 |
| `mbt deploy --reallocate` | `OFTPD.DEV.LINKLIB` deleted and allocated again on WORK00, 160 tracks, 25% used; OFTP, OFTPD, OFTPI, OFTPSVC |
| `SYS2.PROCLIB(OFTPD)` | `OFTPD.DEV.LINKLIB` ahead of `OFTPD.LINKLIB` in the STEPLIB; read back identical |
| `SYS2.PARMLIB(OFTPPRM0)` | `PORT=3325 CONTROLPORT=3326` became `PORT=3325 SVC=237`; read back identical |
| `P OFTPD` | STC00611, `OFTP003I`, 0 sessions before |
| `S OFTPD` | **STC00612**: `OFTP000I oftpd 0.1.0-dev (7a9d9a9)`, `OFTP004I AUTHORIZED BY SVC`, `OFTP031I SVC 237 ENTRY FOUND 0000CCC8 80000000`, `OFTP030I ... MODULE=00A904C8 (1280 BYTES)`, `OFTP039I CROSS MEMORY ACTIVE ON SVC 237, SUBSYSTEM OFT1` |
| SVC 237 active (`JOB00648`) | `00A904C8 C0000000`, modal EP in 39 of 56 |
| `OFTP` batch from the DEV library (`JOB00649`, CC 0) | `PARTNERS` (5), two `SEND`s to DEV1 (T0000010 F, T0000011 T), `LIST MINE` (7); `JOB00650` `LIST`, `JOB00651` `SHOW` |

`JOB00643` and `JOB00644` were the same SVC reader with a broken
instream DD (`/*` in column 1 ends `DD *`; `DD DATA,DLM=@@` and a
`STDOUT` DD are what BREXX needs) and printed nothing.

**The file to DEV1 did not arrive.** DEV1 is reached via M38L. m38x sent
both files to m38l (`OFTP211I ... SENT`), m38l received them (its
T0000021/22 `RECV CONFIRMED`) and passed them on, and dev1 refused both
on the first attempt with reason 12, no retry (m38l T0000019/20
`REJECTED`, "refused by the partner, reason 12"). m38l then sent the
NERP 12 back, and T0000010/11 on m38x are `REJECTED`. Nothing
`IBMUSER.OFTPIN.M38X.SVC.*` and no `IBMUSER.OFTPTMP.*` exists on dev1
(MBT, read only). The cause, first inferred by MBT and then read on dev1's console by
the maintainer: `OFTP221E M38L: IBMUSER.OFTPTMP.T0000157 NOT RENAMED TO
IBMUSER.OFTPIN.M38X.SVC.TO.DEV1.F`, then `EFNA 12`, the same for
T0000158 (dev1's console messages reach neither the Hercules log nor
STC00813's JESMSGLG). dev1
runs `2253a7a`, built on libc370 2.17.0, whose `rename()` writes the
`ALTER ... NEWNAME()` past column 72 (fixed in 2.18.1, libc370#590).
The final name `IBMUSER.OFTPIN.M38X.SVC.TO.DEV1.F` has 33 characters;
cut at 32 it ends in a dot, the ALTER fails, and `OFTP221E` answers
reason 12 without retry. Gone with dev1 on `main` (below, the
retest): the same name arrived.

**The lasting state: m38x runs a DEV build ahead of the SMP level.**
`OFTPD.LINKLIB` still holds TOFT010; the STEPLIB's first library wins.

Undo: write the two backed-up members back (the STEPLIB line
`//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB` alone, and
`PORT=3325 CONTROLPORT=3326`), then `P OFTPD` and `S OFTPD`; the stop
gives SVC 237 back (`OFTP032I`). `OFTPD.DEV.LINKLIB` can stay or go.

## All five on main over SVC 237, and the retest, 2026-10-19

A block slot from the MBT session for dev1, m38l, m38b and m38r, one
after the other, then the network retest; m38x was done earlier the
same day (above). The build is `7a9d9a9`, linked with libc370 2.21.0
(the module carries no libc370 level; the long names below are the
proof of 2.18.1 or later). dev1 is in `internals/dev1-changes.md`.

**Backups on each stand**, besides the local copies: the original
`SYS2.PROCLIB(OFTPD)` as `(OFTPDB19)` and `SYS2.PARMLIB(OFTPPRM0)` as
`(OFTPPR19)`, new names on all five, written binary and read back
byte-identical (PROC 2480 bytes everywhere, OFTPPRM0 400, dev1 480).

On m38l, m38b and m38r: `OFTPD.DEV.LINKLIB` allocated by `mbt deploy`
(m38l WORK00, m38b and m38r TSO001, 160 tracks each), the STEPLIB with
the DEV library ahead of `OFTPD.LINKLIB`, `PORT=33n5 SVC=237` in place
of `PORT=33n5 CONTROLPORT=33n6`; both members read back identical.

| Stand | SVC 237 before | OFTPD | SVC 237 active | `OFTP` check |
|---|---|---|---|---|
| m38l | JOB01795 `0000CCC8 80000000`, 40/56 | STC00879 | JOB01797 `00A904C8 C0000000`, 39/56 | JOB01798 CC 0 |
| m38b | JOB02728 `00017068 80000000`, 37/56 | STC00132 | JOB02730 `009D60C0 C0000000`, 36/56 | JOB02731 CC 0 |
| m38r | JOB00917 `00017068 80000000`, 37/56 | STC00113 | JOB00919 `009D60F0 C0000000`, 36/56 | JOB00920 CC 0 |

STC00132 and STC00113 showed `OFTP000I oftpd 0.1.0-dev (7a9d9a9)`,
`OFTP004I AUTHORIZED BY SVC`, `OFTP031I ... FOUND 00017068 80000000`,
`OFTP039I`. On m38l the banner was not seen: `S OFTPD` was given while
the old STC00878 was still stopping, so the new one waited for the
spool (`IEF861I`/`IEF099I`) and got it in the same second STC00878
ended; its console lines did not come back. That it runs `main` rests
on SVC 237 being taken (0.1.0 takes none) and the `OFTP` check over it.
On the TK5 stands the old OFTPD's output was printed and purged at its
end (`$HASP150 ... 13,189 LINES`, the #12 SYSUDUMP).

**m38r, the TK5 reference stand, is a development stand now**: a DEV
build sits ahead of its SMP level, so it no longer shows what an
installation from the release does, until it is backed out. The same
holds for m38l, m38b and m38x.

### The retest

Seven sends, one job per sending stand (all CC 0), the received names
up to 41 characters (a virtual file name has at most 26). All seven
`DELIVERED` with the EERP back (LIST JOB00653 m38x, 02468 dev1, 00922
m38r, 02733 m38b, 01800 m38l), and every received data set identical to
its source (compared as text; the check tells a different source and a
missing data set apart).

| Job | From | To | Path | Received as |
|---|---|---|---|---|
| m38x JOB00652 | M38X | DEV1 | via M38L | `IBMUSER.OFTPIN.M38X.SVC.TO.DEV1.F` and `.T` (33) -- the name that failed before |
| m38x JOB00652 | M38X | M38B | M38L, DEV1 | `OFTPD.IN.M38X.SVC.TO.M38B.LONG.NAME` (35) |
| dev1 JOB02467 | DEV1 | M38X | via M38L | `OFTPD.IN.DEV1.SVC.TO.M38X.F` and `.T` |
| m38r JOB00921 | M38R | DEV1 | via M38B | `IBMUSER.OFTPIN.M38R.SVC.TO.DEV1.LONG.NAME` (41) |
| m38b JOB02732 | M38B | M38L | via DEV1 | `OFTPD.IN.M38B.SVC.TO.M38L.F` |
| m38l JOB01799 | M38L | M38R | DEV1, M38B | `OFTPD.IN.M38L.SVC.TO.M38R.HOP3` |

Now running: dev1 STC00814, m38l STC00879, m38x STC00612, m38b STC00132,
m38r STC00113, all `7a9d9a9`, SVC 237 active, no control port.

Undo, per stand: copy `OFTPDB19` over `SYS2.PROCLIB(OFTPD)` and
`OFTPPR19` over `SYS2.PARMLIB(OFTPPRM0)`, `P OFTPD`, `S OFTPD`; the
stop gives SVC 237 back (`OFTP032I`). Then the TOFT010 level runs again
(dev1: the PROC is unchanged, the old OFTPD level is gone, see
`dev1-changes.md`). The test files `*.OFTPIN.*.SVC.*` and
`OFTPD.IN.*.SVC.*` can be deleted.
