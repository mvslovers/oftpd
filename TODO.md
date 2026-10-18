# Open Work, Ranked

Holds order and rationale only; once the repo is on GitHub, its issues are
the source of truth. The MVP scope is `internals/design.md` section 0.

## MVP

1. ~~Codec~~ — done (TSTCMD, TSTDATA; RFC 5024 App. A byte for byte).
2. ~~Session engine~~ — done (TSTSESS, ten scenarios over a socket pair).
3. ~~Host driver~~ `host/oftph` — done; interop with mendelson OFTP2 CE at
   level 5 in both directions with EERPs (`internals/interop.md`).
4. ~~KSDS smoke test on MVS~~ — done (`test/mvs/tstksds.c`, JOB02094);
   inserts need a direct PUT (CLAUDE.md).
5. ~~Data set reader/writer~~ — done (`src/mvs/oftp#dsf.c`, TSTDSF on MVS,
   JOB02101): F, V (empty record kept), U (short last block), T (CR-LF split
   across pieces, long line refused with 05), existing target refused 13.
6. ~~OFTPD~~ — runs as a started task on dev1 from `samplib/oftpd`
   (SYS2.PROCLIB/PARMLIB members installed 2026-10-12): files both ways
   with mendelson, EERPs both ways (`internals/interop.md`).
7. ~~OFTP utility~~ — SEND, LIST, SHOW, DELETE, HOLD, RELEASE, RETRY, CALL,
   PARTNERS from TSO and batch (`samplib/oftpbat`, JOB02133/JOB02137).

8. ~~ISPF dialog~~ — OFTPI under ISPF M -> O on dev1: menu, send, transfers
   (S/H/R/T/D), partners (C, S), configure (edit + REFRESH), monitor;
   `test/tso/ispf.lua` drives it over TN3270 (green 2026-10-12). Panels in
   `ispplib/`; dev1 has them in SYSGEN.ISPF.PLIB, OFTP370.DEV.LINKLIB in
   ISPLLIB (SYS1.CMDPROC(ISPF), backup ISPFBAK; ISPMORE backup ISPMOREB).

## Next

- ~~Prerelease 0.1.0 on the M38 stands~~ — 2026-10-13: installed through
  SMP from the release assets on m38l, m38x, m38b and m38r
  (`internals/m38-changes.md`), `OFTP004I AUTHORIZED BY SVC` on all four;
  test network steps 1-5 passed over up to four hops, EERP and NERP back
  (`internals/testnet.md`, Results). The first final release is 0.2.0
  (`TOFT020`, deletes `TOFT010`).
- **Cross-memory transport** (#1, `internals/xmem.md`, a private SVC):
  SVC 237, free on all five stands (measured 2026-10-14); the ESTAE is
  in (d5b8bdc, measured with a CANCEL on dev1), #2 too; the handlers
  answer through an output function (eeace93); `asm/oftpsvc.asm` is
  written and assembles clean under as370 1.6.3, all 29 field offsets
  holding against `include/oftpsvc.h` under cc370, nothing run on MVS.
  `[module.OFTPSVC]` and `SVC=` (200-255, no default) are in too, so
  mbt assembles and links the routine and `module-data` reports no
  writable data (read the output, not the exit code -- `module-data`
  warns and still exits 0, `internals/xmem.md` section 8).
  **Piece one of the install is on `main`** (PR #11, merged 2026-10-18):
  `src/mvs/oftp#svc.c` with the
  anchor GETMAIN, `__loadhi` and the anchor patch, the SVC
  reserve/restore and the read-back, the new `OFTP_MON` fields, the
  `oftpd.c` wiring and the restore as the first act of
  `oftp_mon_abend()`. It installs and does **not** activate:
  `OFTP_ANC_ACTIVE` stays clear, so a caller gets `OFTP_SVC_DOWN`
  before claiming a slot and there is nothing to leak.
  Next is **#16**: the slot thread, the wake ECB, the SSCT, the liveness
  check and the sweep, then the probe round-tripping `STATUS`. It is
  blocked by **#15**, the key-8 measurement below.
  **Measured on m38x 2026-10-18** (`internals/xmem.md` section 10): the
  SVC entry taken and given back, twice -- once through the abend path
  and once through the clean stop -- every reading taken by a second
  party with a BREXX `STORAGE()` job and agreeing byte for byte with
  OFTPD's own `OFTP031I`/`OFTP032I`. OFTPSVC is 1144 bytes, the anchor
  and pool 9536. The first stop abended S0C4 on a key-8 store into the
  key-0 anchor; the ESTAE gave the entry back regardless, because the
  restore is its first act. Fixed, and the fix's prediction held on the
  next run.
  Open from that run: **whether a key-8 read of subpool 241 is legal**
  is assumed, not measured, and piece two depends on it (the slot
  thread reads slots from key 8 continuously) -- **#15**, with `TPROT`
  as the instrument. The second open point has been withdrawn: "no
  SYSUDUMP appeared" was never observed, only the console log was read
  and a dump goes to SYSOUT (#12 covers the dumps OFTPD leaves on dev1;
  `internals/xmem.md` section 10 says what is and is not known).
  A program check inside the routine gets no recovery of its own: it is
  prevented where it can be -- `TPROT` on both ends of every caller
  range before the claim, so a forged pointer is refused instead of
  faulting in key 0 -- and swept where it cannot
  (`internals/xmem.md` section 6, with the `COPYOUT` state telling a
  faulted copy-out from a caller that is merely swapped out). Both of
  those are in; the sweep itself is part of `oftp#svc.c`, and whoever
  frees a slot owns its in-flight decrement -- a second decrement
  underflows the count and wedges `P OFTPD`. It replaces the TCP
  control port, which goes off by default in 0.2.0.
- ~~Before 0.2.0: the spool after an abend~~ (#2) -- OFTPD runs VERIFY
  itself when the open fails (rc 4 after a CANCEL, measured on dev1) and
  says so in `OFTP021W`.
- ~~Before 0.2.0: a session subtask that abends~~ -- libc370's subtask
  driver runs it with no recovery, so the session stayed counted, its
  partner was refused (ESID 03), its spool record stayed ACTIVE and
  `P OFTPD` hung (dev1, 2026-10-15). The session now runs under `try()`
  and is cleaned up like one that ends (`OFTP209E`, `OFTP203I`); measured
  on dev1 with a debug-only trigger (`-DOFTP_DEBUG_ABEND`). Left open: an
  abend inside the spool's lock or a queued spool request, and the
  session engine's buffer after an abend.
- ~~Session subtasks are never detached~~ (#3) -- each kept about 69 KB
  until a restart (50 sessions on dev1, 7536 KB to 4096 KB largest free
  block). The listener and the scheduler now free each subtask once it
  has ended and wait for the rest at stop; flat over 50 sessions, and
  `D M` shows the largest free block and the subtask handles.
- An EERP held at a relay after a call collision (testnet.md, Results):
  seen once at m38b, not reproduced. Find out whether a refused call
  (ESID 03, `OFTP202W`) can leave owed EERPs uncalled beyond RETRY.
- `F OFTPD,D P`: name the `VIA` partner instead of `-:3305`.
- ~~mendelson: the M38 nodes as partners there~~ — 2026-10-14, with
  the rename DEV1 -> `O0000000000DEV1`, MENDEL -> MAC1
  (`O0000000000MAC1`): M38R -> MAC1 and MAC1 -> M38R (DEV1 as
  mendelson's gateway) both delivered with the EERP back
  (`internals/testnet.md`, "Renamed").
- ~~dev1 on the new names~~ — 2026-10-13 (STC00661, `OFTP004I
  AUTHORIZED BY SVC`, ISPF test green, `OFTP370.DEV.LINKLIB` scratched;
  `internals/dev1-changes.md`).
- Retry limit (`MAXRETRY`, design 3.3): measured on dev1 2026-10-13
  (`internals/interop.md`). Still untried there: a transfer cut off
  mid-file, the wait after a call that failed before any file, a NERP
  for a level 1.3 sender.
- Dialog: `S` (show) and Configure not yet driven by the TN3270 test.
  Panels for 80x43, measured on dev1 2026-10-13 at 80x43 and 160x62
  (s3270 `-model 3279-4-E`, `-model 3279-5-E -oversize 160x62`): on
  Wally's ISPF `EXPAND(//)` works on TBDISPL panels, inside the ZCMD
  field and in `)MODEL` lines (`@Z / /+` lets the last field grow); a
  two-line model works. A model line without it ends at column 80 of the
  source; a field after `/ /` keeps its literal length, so pad it and end
  it with an attribute; `&VAR` in text is substituted before expanding
  and swallows the blank after it. Menu and Send need more than 24 rows;
  `test/tso/ispf.lua` runs at 80x43 (`[tso.ispf] size`, mbt 3.0.0-dev).
- TBCREATE from a CLIST answers RC 20 on dev1 (also with the variables
  set first, as Wally's CMEMFIND does); not understood.
- Dialog lag: the control port now looks for a request every 0.1 s
  (was 1 s; greeting after 0.04-0.10 s instead of 1.0 s). A request
  still takes 1.1-1.2 s in the dialog against 0.03-0.05 s for a panel
  without one, the same for 5 answer lines (Monitor) as for 9
  (Transfers): a fixed cost per request on the MVS client side, not per
  row or byte. A 0.1 s select() tick on the client changed nothing (A/B
  on dev1). Next suspects: connect() and closesocket() through DYN75.

- Log to a SYSOUT data set instead of every line by WTO; a log-only
  message class (design 8).
- `docs/`: a configuration guide beside `docs/installation.md`.
- ~~GitHub repository, CI~~ — `mvslovers/oftpd` (2026-10-13).

## After the MVP

- Restart, buffer compression, NERP generation beyond a relay that gives
  up, post-processing (JCL and exit), naming patterns for received data
  sets, partner maintenance by command/ISPF, virtual local Odette IDs,
  routing beyond one hop, cross-memory transport.

## Maintainer decisions

- ~~Product name~~ — OFTPD, repo `oftpd` (2026-10-13).
- FMID prefix `TOFT` (check `LIST CDS/ACDS SYSMOD(TOFT010)` on MVS/CE
  and TK5, with job numbers); book area (ML09 is the next free one);
  rows in the root `CLAUDE.md`.
