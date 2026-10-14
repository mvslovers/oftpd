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

Now running: dev1 STC00676 (a plain `S OFTPD` since the PROC fix,
`internals/dev1-changes.md`), m38l STC00779, m38x STC00509, m38b STC00074,
m38r STC00034.
