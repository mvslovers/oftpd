# CLAUDE.md — oftpd

Extends the ecosystem root `CLAUDE.md` (`~/repos/mvs/CLAUDE.md`); nothing
here contradicts it.

## What this is

An OFTP (ODETTE File Transfer Protocol) implementation for MVS 3.8j: a
monitor running as a started task (`OFTPD`) plus TSO and batch utilities
(`OFTP`). Not meant for production use; the aim is a managed file transfer
tool for the MVS hobbyist community — MVS to MVS over TCP/IP, and MVS to PC
peers. A commercial OFTP product is the model for the operational design.
Never name it, in the repo or on GitHub: call it "The Other" (or "other"),
and describe its behaviour instead of citing its manuals.

Product name: **OFTPD**, repo `oftpd` (decided 2026-10-13, matching FTPD
and HTTPD; the repo was called oftp370 before). The started task and its
module carry the same name, as FTPD's do.

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
- **Utilities reach the monitor by cross-memory, through OFTPD's own SVC**
  (`internals/xmem.md`), because only there is the caller's identity real.
  The TCP control port they used during development was removed on
  2026-10-19 (#32): it had no security, and a second way in would be one
  where nobody is known. Keep the request layer transport-neutral all the
  same (the handlers write through an output function), so another
  transport -- an HTTP API, say -- can be added without touching them; it
  must bring a real identity with it. (The sockets are the Hercules
  host's, so "loopback" is the PC, not MVS.) `OFTP`, batch and the ISPF
  dialog are all the same kind of client. The operator uses MODIFY/STOP
  on the started task.

## Build

mbt 3 (`mbt.toml`, no submodule, no Makefile). `mbt build`, `mbt test`
(host), `mbt test --mvs`, `mbt module-data`. Shared code is `src/oftp#*.c`
(`[internal]`, also linked into host tests via `[build.host]`); module mains
carry no `#`. `[distribution]`/`[smp]` carry the FMID prefix `TOFT`: 0.1.0-dev is
`TOFT010` (spent), 1.0.0 is `TOFT100` and deletes it.

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
