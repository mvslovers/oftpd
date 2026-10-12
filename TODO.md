# Open Work, Ranked

Holds order and rationale only; once the repo is on GitHub, its issues are
the source of truth.

1. **Design document** (`internals/design.md`): architecture of the monitor
   (listener, sessions, scheduler, control port), spool record layout and
   states, partner table layout, restart bookkeeping, control protocol.
2. **KSDS smoke test on MVS**: define, write, read, update, delete, START
   through libc370's VSAM API, which has no test of its own.
3. **Protocol codec** (host-testable): STB framing, command encode/decode
   per level, exchange buffer subrecords, buffer compression. Test against
   the RFC examples byte for byte.
4. **Session state machine**, then the monitor around it (ftpd as template).
5. **Interop**: mendelson OFTP2 CE on port 3305 (level 5); a 1.x peer for
   levels 4 and 2 is still to be found.
6. Maintainer decisions: product name; FMID prefix `TOFT` (check `LIST
   CDS/ACDS SYSMOD(TOFT010)` on MVS/CE and TK5, with job numbers); book area
   (ML09 is the next free one); rows in the root `CLAUDE.md`.
7. GitHub repository, then `.github/workflows` go live.
