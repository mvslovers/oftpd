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
  `test/tso/ispf.lua` needs mbt's TN3270 driver with larger models (MBT
  session is on it).
- TBCREATE from a CLIST answers RC 20 on dev1 (also with the variables
  set first, as Wally's CMEMFIND does); not understood.
- Dialog lag: the control port now looks for a request every 0.1 s
  (was 1 s; greeting after 0.04-0.10 s instead of 1.0 s). A request
  still takes 1.1-1.2 s in the dialog against 0.03-0.05 s for a panel
  without one, the same for 5 answer lines (Monitor) as for 9
  (Transfers): a fixed cost per request on the MVS client side, not per
  row or byte. A 0.1 s select() tick on the client changed nothing (A/B
  on dev1). Next suspects: connect() and closesocket() through DYN75.
- `[distribution.library]` for `ispplib/`, installation notes for the
  dialog (ISPPLIB/ISPLLIB, ISPMORE).

- Log to a SYSOUT data set instead of every line by WTO; a log-only
  message class (design 8).
- `docs/`: installation and configuration guide (needed for
  `[distribution]`), then `[distribution]`/`[smp]` with `TOFT`.
- GitHub repository, CI.

## After the MVP

- Restart, buffer compression, NERP generation beyond a relay that gives
  up, post-processing (JCL and exit), naming patterns for received data
  sets, partner maintenance by command/ISPF, virtual local Odette IDs,
  routing beyond one hop, cross-memory transport.

## Maintainer decisions

- Product name; FMID prefix `TOFT` (check `LIST CDS/ACDS SYSMOD(TOFT010)`
  on MVS/CE and TK5, with job numbers); book area (ML09 is the next free
  one); rows in the root `CLAUDE.md`.
- GitHub repository, then `.github/workflows` go live.
