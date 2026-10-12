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
6. **OFTPD**: ~~parmlib partners~~ (TSTCFG), ~~spool server~~ (TSTSPL on
   MVS, JOB02115); next the listener, outbound calls, the glue between
   spool, data sets and session engine, console commands (ftpd as
   template).
   - Drop `put_direct()` once libc370#514 (direct PUT in `vswrite()`) is
     released and the toolchain pin moves to it.
7. **OFTP SEND / LIST** over the TCP control port.

## After the MVP

- Restart, buffer compression, NERP generation, post-processing (JCL and
  exit), naming patterns for received data sets, partner maintenance by
  command/ISPF, virtual local Odette IDs, routing, cross-memory transport.

## Maintainer decisions

- Product name; FMID prefix `TOFT` (check `LIST CDS/ACDS SYSMOD(TOFT010)`
  on MVS/CE and TK5, with job numbers); book area (ML09 is the next free
  one); rows in the root `CLAUDE.md`.
- GitHub repository, then `.github/workflows` go live.
