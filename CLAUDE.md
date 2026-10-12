# CLAUDE.md — oftp370

Extends the ecosystem root `CLAUDE.md` (`~/repos/mvs/CLAUDE.md`); nothing
here contradicts it.

## What this is

An OFTP (ODETTE File Transfer Protocol) implementation for MVS 3.8j: a
monitor running as a started task (`OFTPD`) plus TSO and batch utilities
(`OFTP`). Not meant for production use; the aim is a managed file transfer
tool for the MVS hobbyist community — MVS to MVS over TCP/IP, and MVS to PC
peers. The commercial rvsMVS is the model for the operational design.

Product name: still open. Until it is decided, use the repo name. Mind the
root rule: `X/370` names can collide with IBM product names.

## Decisions taken (2026-10-12)

- **Protocol level: OFTP 2.0 on the wire, with every security feature off,
  and downgrade to 1.4 and 1.3** (SSIDLEV 5, 4, 2/1). One engine; the
  negotiated level selects the field layouts. The SSID is 61 bytes at every
  level, so the responder parses it before it knows the outcome. Why 2.0:
  the only maintained free test peer (mendelson OFTP2 CE) speaks level 5
  only, and 2.0 costs little more than 1.4. Details and offsets:
  `internals/protocol.md`.
- **No TLS, no certificates**: no SECD/AUCH/AURP (SSIDAUTH=N), no CMS
  signing or encryption, no signed EERP/NERP (an unsigned EERP is permitted,
  RFC 5024 s3.3.5). Refuse per file with SFNA 15/16/18/19. TLS, if wanted,
  comes from a gateway in front of port 3305. CMS compression (zlib) is a
  maybe-later.
- **The monitor is the sole owner** of the spool (a VSAM KSDS, one record per
  transfer, also holding the restart state) and of the partner table. The
  utilities never touch them; they ask the monitor.
- **Utilities reach the monitor by cross-memory — that is the target** —
  because only there is the caller's identity real. **During development
  they use a TCP control port without security.** Both carry the same
  requests; keep the request layer transport-neutral and never treat a TCP
  caller as authenticated. (The sockets are the Hercules host's, so
  "loopback" is the PC, not MVS.) The client side lives in the shared code,
  so `OFTP`, batch and later ISPF dialogs (libc370 `mvs/ispf.h`) are all
  the same kind of client. The operator uses MODIFY/STOP on the started
  task.

## Build

mbt 3 (`mbt.toml`, no submodule, no Makefile). `mbt build`, `mbt test`
(host), `mbt test --mvs`, `mbt module-data`. Shared code is `src/oftp#*.c`
(`[internal]`, also linked into host tests via `[build.host]`); module mains
carry no `#`. `[distribution]`/`[smp]` are added with the first PROC; the
proposed FMID prefix is `TOFT` (0.1.0 = `TOFT010`), not yet checked on any
stand.

## Rules specific to this project

- **Cross-build before every commit** (`mbt build --all --tests`): the
  host compiler accepts what cc370 refuses. cc370 treats `yn`, `logf` and
  the other C99 math names as built-ins, so a static helper of that name
  fails the MVS build while the host tests pass.
- **No writable module data in OFTPD.** It is AC(1) and RENT; a writable
  static abends in key 0 (ftpd#101). State lives in a `main()` local reached
  through the GRT. `mbt module-data` must stay clean.
- **Every command byte is ASCII on the wire**, at every level. Translate
  explicitly; never compare against ASCII codes in logic (EBCDIC host).
  libc370 has no ASCII/EBCDIC tables — take ftpd's.
- **Check every write and the close** before confirming a file (EFPA/EERP):
  a full volume shows up only at CLOSE (libc370 consumer notes).
- **libc370's VSAM on a KSDS, measured** (`test/mvs/tstksds.c`,
  `test/mvs/tstspl.c`): needs libc370 **2.10.1** or later -- before it,
  `vswrite()` on a dynamic handle was a sequential PUT (an insert below the
  position and a duplicate both reason 12) and `vsread()` of a missing key
  said reason 88 (libc370#514, found here). A never-loaded cluster *opens*
  for UPD, but in load mode: POINT and PUT then fail with reason 116
  (X'74', JOB02112), so the spool loads its control record through a
  sequential OUT open first. Any other call: measure before use.
- **VSAM space on dev1**: `PUB000` (catalog `UCPUB000`) has no room for a
  suballocated cluster; a test cluster is defined `UNIQUE`.
- Socket traps (libc370 sockets reference, ftpd): no `setsockopt`, a
  blocking `connect()` gives up after about 1 s, a blocking `recv()` waits
  for all `len` bytes, socket numbers are system-wide and survive the
  program. ftpd is the worked example for the started task as a whole.
