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

## Next

- Log to a SYSOUT data set instead of every line by WTO; a log-only
  message class (design 8).
- `docs/`: installation and configuration guide (needed for
  `[distribution]`), then `[distribution]`/`[smp]` with `TOFT`.
- GitHub repository, CI.

## After the MVP

- Restart, buffer compression, NERP generation, post-processing (JCL and
  exit), naming patterns for received data sets, partner maintenance by
  command/ISPF, virtual local Odette IDs, routing, cross-memory transport.

## Maintainer decisions

- Product name; FMID prefix `TOFT` (check `LIST CDS/ACDS SYSMOD(TOFT010)`
  on MVS/CE and TK5, with job numbers); book area (ML09 is the next free
  one); rows in the root `CLAUDE.md`.
- GitHub repository, then `.github/workflows` go live.
