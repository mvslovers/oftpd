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
  **Piece two is on `main`** (#16, PR #17, merged 2026-10-18,
  `internals/xmem.md` section 11): the slot thread, the wake ECB, the
  SSCT, the liveness check, the sweep, the client side and
  `test/mvs/tstsvc.c`. Host build and 220/220 host tests clean on
  `main` after the merge, and **nothing has run on MVS** -- which is
  why #16 stays open. Every CSA access is bracketed and lives in
  `oftp#svc.c`; the handlers work on a key-8 copy, so **#15 can only
  let a bracket come off, it can never force one on** -- which is why
  it did not block this. The one place a key-8 read of subpool 241 is
  unavoidable is the client (`oftp#svcc.c`), where nothing can be
  authorized; libc370's `ssct_find()` reads the same CSA chain in its
  caller's key with no MODESET, which is the evidence -- one library's
  source, not a measurement.
  **Both are measured** (2026-10-18, `internals/xmem.md` section 12),
  in a throwaway MVS/CE container and not on a stand: #16 end to end
  18/18 from a STEPLIB that is not APF-authorized, with the SVC entry
  read three times by a second party and agreeing with OFTPD's own
  messages each time; #15 answering **CC 1 for a key-8 access to
  subpool 241** -- fetch legal, store refused -- against a key-8 heap
  control at CC 0. Paging had to be forced to be tested at all
  (`LIST` on a fresh spool is 0 rows); forced, 5 rows came across in
  2 pieces, each exactly once. On TK5 since in a container only
  (#16: `TSTSVC` 34/34; #15's `TSTPROT` not run there), the
  sweep's state table is still unexercised, and the storage-key
  granularity is explicitly NOT settled.
  **#15 is answered** (`test/mvs/tstprot.c`, `tstprot.jcl`):
  `TPROT` against our own `getmain(n, 241)` and `__loadhi()`, with
  ordinary key-8 heap as the control and a scan for a 2K-but-not-4K
  key boundary. It needs **no OFTPD** but it does need an
  **APF-authorized STEPLIB**, because `TPROT` turns out to be a
  privileged instruction (Hercules `control.c`, `PRIV_CHECK` in the
  `E501` handler) -- so the two runs are independent after all, and
  only #16 needs the stand prepared.
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
  is assumed, not measured -- **#15**, with `TPROT` as the instrument.
  Piece two does *not* depend on it (every read is bracketed), but the
  client side does, and #15 and the `tstsvc` run need the same stand:
  **one slot request, not two.** The second open point has been withdrawn: "no
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
  **The utilities use it** (#22, `internals/xmem.md` section 13): no
  host means the SVC, no fallback to TCP; the caller's userid travels
  beside the request line, and `ADMIN=` in `OFTPPRM` decides who sees
  and changes every transfer (everyone else: only their own). Measured
  in two throwaway containers: `OFTP` batch from a library that is not
  APF-authorized, two owners with and without `ADMIN=`, paging through
  the client, the dialog on a 43-row screen. Then in a TK5 container
  (2026-10-18): OFTPD authorized through SVC 244 from a library not in
  `IEAAPF00`, SVC 237 taken and given back (read by a second party),
  two RAKF userids (HERC01 in `ADMIN=`, HERC02 not), paging, `TSTSVC`
  34/34; then on the stand m38b as a test instance beside its OFTPD
  (`TSTSVC` 20/20, SVC 237 back to `00017068` at the end). Open from it:
  - The dialog has not run on TK5 at all.
  - **`SEND` reads the data set with OFTPD's authority**, not the
    caller's: an ordinary user can send what they could not read. Same
    as over TCP, but now visible. `RACINIT`/`RACHECK` for the caller is
    the candidate; not decided.
  - A request over 255 bytes is refused by the client before the SVC
    (`TSTSVC` step 2b); the routine's own length check is still
    exercised by no test.
  - The dialog's profile on dev1 still holds `127.0.0.1`: clear the host
    under option 5 when the SVC is on there.
- ~~Before 0.2.0: the spool after an abend~~ (#2) -- OFTPD runs VERIFY
  itself when the open fails (rc 4 after a CANCEL, measured on dev1) and
  says so in `OFTP021W`.
- ~~Before 0.2.0: a session subtask that abends~~ -- libc370's subtask
  driver runs it with no recovery, so the session stayed counted, its
  partner was refused (ESID 03), its spool record stayed ACTIVE and
  `P OFTPD` hung (dev1, 2026-10-15). The session now runs under `try()`
  and is cleaned up like one that ends (`OFTP209E`, `OFTP203I`); measured
  on dev1 with a debug-only trigger (`-DOFTP_DEBUG_ABEND`). The engine's
  record buffer is freed after an abend too (it kept about 38 KB per
  abend, measured on dev1 2026-10-18; 6 KB of it and the 0.6 KB per
  abend left are not explained, #13), and the spool's lock is given back
  (hardening: the session holds it for a few pointer stores, an abend
  there needs a damaged spool block). Left open: a session abended while its spool
  request is queued is possible only when its attaching thread gives up
  at stop after 60 s; whether `try()` still recovers there is not
  measured.
- The spool thread runs without recovery: an abend in `serve()` ends it,
  and every spool request after that waits for good, `P OFTPD` included.
- ~~Every OFTPD end writes a SYSUDUMP~~ (#12): `S30A` in VSAM's CLOSE
  (SVC 20 issuing SVC 10), because the spool thread closed a cluster the
  main task had opened. The main task closes it now. In an MVS/CE
  container: before, a 13 971-line dump at the stop; after, none in four
  ends, and transfers kept across a restart with no `OFTP021W`.
- ~~Session subtasks are never detached~~ (#3) -- each kept about 69 KB
  until a restart (50 sessions on dev1, 7536 KB to 4096 KB largest free
  block). The listener and the scheduler now free each subtask once it
  has ended and wait for the rest at stop; flat over 50 sessions, and
  `D M` shows the largest free block and the subtask handles.
- An EERP held at a relay after a call collision (testnet.md, Results),
  **#18**: reproduced between dev1 and m38b on 2026-10-18. Both sides
  mark the partner busy before the call, refuse each other (ESID 03,
  `OFTP202W`), wait RETRY from the same second and collide again until
  timing jitter lets one call through. ~~ESID 08 and a random 5-60 s
  pause after a refused call~~ -- PR #19: in two MVS/CE containers five
  full collisions out of seven trials, each resolved by one call after
  the shorter pause, both files and EERPs delivered after 17-46 s
  (`internals/design.md` 6.1). ~~`SESSIONS=n` per partner, default 1,
  files and EERPs claimed by one session~~ -- PR #20: 20 files over two
  parallel sessions and 12 each way at once, each sent, received and
  confirmed once; with `SESSIONS=1` the collision resolves as before.
  `MAXSESS` reached keeps the silent close. Open: a session ending while
  it holds a claim is read, not measured.
- ~~A file over about 750 KB between two OFTPDs ends EFNA 12~~ (#21):
  the SFID now carries the tracks used at the volume's track length, and
  a size of 0 gets `TRK(15,15)` instead of `TRK(5,3)`. Measured in two
  MVS/CE containers: 64 000 records (5 MB) delivered with the EERP and
  identical; `TSTDSF` 40/40. A file from mendelson is not checked.
- The control port answers one of several requests that arrive at once:
  of four parallel `RELEASE`s one got `200` (containers, 2026-10-18).
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

- ~~Spool housekeeping~~ (from rvsMVS's cleanup job): `KEEPDAYS=n`
  (default 7) deletes finished transfers once a day, `PURGE [DAYS(n)]`
  and `F OFTPD,PURGE [n]` on request (design 3.3.1). Measured in two
  MVS/CE containers and `TSTSPL` 27/27; the daily run deleting a
  transfer really older than a day is not observed, only its rule and the
  request it makes. Later: purge by partner or name, a spool backup job.
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
