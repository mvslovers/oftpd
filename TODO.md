# Open Work, Ranked

Holds order and rationale only; once the repo is on GitHub, its issues are
the source of truth. The MVP scope is `internals/design.md` section 0.

## MVP

1. **Codec** (host-tested): STB framing, every command encoded and decoded
   per level (5, 4, 2/1), DATA subrecords, CP037 <-> ISO 8859-1 tables.
   Byte for byte against the RFC examples.
2. **Session engine** over an abstract transport; two engines against
   each other in one host test.
3. **Host driver** for the engine, then interop with mendelson OFTP2 CE on
   port 3305 (level 5) — before anything runs on MVS.
4. **KSDS smoke test on MVS** through libc370's VSAM API, which has no
   test of its own.
5. **Data set reader/writer** per format (F/V/U/T), checking every write
   and the CLOSE.
6. **OFTPD**: listener, outbound calls, spool server, parmlib partners,
   console commands (ftpd as template).
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
