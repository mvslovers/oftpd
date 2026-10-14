# The cross-memory transport: a private SVC

Status: proposed 2026-10-14, the transport decided by the maintainer the
same day (a dynamically installed SVC, not the subsystem interface);
mvslovers/oftpd#1.
Nothing here is built or measured yet; every "measure" below is open.

This replaces the TCP control port (`design.md` section 7) as the way the
utilities (`OFTP` in TSO and batch, the dialog `OFTPI`) reach OFTPD. The
requests and answers stay exactly as they are: one request line, `1xx`
lines, one final line (`src/mvs/oftp#cli.c`). Only the transport changes.

Models, read before writing any of it:

- **nsf370** ADR-0038 (the private SVC: slot steal, type 3, CSA routine,
  restore at stop and in the ESTAE), ADR-0039 (`MVCK` for caller storage),
  ADR-0040 (client-death guard before the reply POST), ADR-0042/0045
  (slot pool, sweep of slots whose owner is gone), ADR-0044 (DONTSWAP),
  ADR-0048 (SSCT rendezvous, adopting a retained anchor). nsf370 measured
  all of this live on MVS/CE; OFTPD copies the structure, not the code:
  its routine is a thin text-in/text-out router, a fraction of
  `nsfvsvc.asm`.
- **ufsd** `internals/cross-as-reference.md`: the cross-AS POST and key
  rules, the in-flight drain, and why the subsystem is not named like
  the procedure (ufsd#55).

## 1. Why an SVC and not the subsystem interface

UFSD's route (an SSI router entered through `IEFSSREQ`) needs an
authorized caller: libc370's `iefssreq()` issues `MODESET MODE=SUP`
itself, the router switches keys with `__super()`, which returns `EPERM`
unless the task is authorized, and libufs says "Caller must be
APF-authorized". UFSD's clients are servers that authorize themselves.
OFTP under TSO, in batch and under ISPF is not authorized, and making it
authorize itself (SVC 244) would hand every user's TSO session APF
authorization to queue a file.

An SVC is the one way an unauthorized problem-state program gets into
supervisor state without APF: the SVC handler dispatches the routine in
supervisor state, key 0, whatever the caller is. nsf370 measured exactly
this with an unauthorized client (ADR-0038, `TESTAUTH FCTN=1`).

## 2. The pieces

| Piece | What it is |
|---|---|
| `OFTPSVC` | the SVC routine, S/370 assembler, its own `[[module]]` (`startup=false`, `ac=1`, RENT, no C runtime). Loaded into CSA by OFTPD with `__loadhi`. |
| the anchor | key-0 CSA (subpool 241): eye catcher, version, flags (`ACTIVE`), the SVC number, OFTPD's ASCB and ECB, the in-flight counter, the slot pool. |
| the slot | one per request in flight: state, owner ASCB and ASID, the userid, the request line, the reply area, a key-0 reply ECB. |
| SSCT `OFT1` | discovery only, no SSVT: `ssctsuse` points at the anchor. Clients find it with `ssct_find()`, a read of the SSCT chain that needs no authorization. |
| the SVC slot | one installation SVC number, stolen at start, restored at stop and in the ESTAE. |

**The subsystem name is `OFT1`**, never `OFTP`/`OFTPD` or a prefix of
either: ufsd#55 met a subsystem named like the procedure taking the start
command down the master-subsystem path (`IEF612I PROCEDURE NOT FOUND`).

The anchor's address is also patched into a fixed word of the CSA copy of
`OFTPSVC` once, before the slot is stolen (ADR-0038 section 3), so the
routine never walks the SSCT chain.

## 3. The SVC number

`SVC=nnn` in `OFTPPRM0` (237, section 3), **no default**. At start OFTPD reads that SVC
table entry (`CVT` -> `CVTABEND` X'C8' -> `SCVTSVCT` X'84' -> `+nnn*8`,
libc370 `ibm/mvs/ihascvt.h`) and steals it only if it holds the
invalid-SVC entry point -- the entry point most of the slots 200-255
share, as NSFV determines it. Anything else: `OFTPnnnE`, no transport,
OFTPD runs on with TCP only. It never picks a number by itself: two
products scanning for "a free one" on the same stand would take the
same slot.

**The ecosystem needs a registry of stolen SVC numbers**, as it has for
FMIDs: nsf370 took **SVC 239** on MVS/CE (240-255 were in use there). Proposed:
a table in the root `CLAUDE.md` beside the FMID table, one row per
product, with the stands each number was measured free on, with job
numbers. Until it exists, this table is the record.

**OFTPD takes SVC 237** (maintainer, 2026-10-14): free on MVS/CE and on
TK5, two away from nsf370's 239, outside TK5's block 220-234 and not
230, the one hole inside that block.

### Measured 2026-10-14, read-only

The SVC table 200-255 of every stand. "Free" means the entry point is the
invalid-SVC routine's, the one most slots of 200-255 share -- not zero.
dev1 through HTTPD's `/.dm`; the M38 stands with a BREXX batch job
(`STORAGE()` along the same chain), purged afterwards.

| Stand | System | Read by | Invalid-SVC EP | Used in 200-255 | Free in 200-255 |
|---|---|---|---|---|---|
| dev1 | MVS/CE | `/.dm` | `00CCC8` | 240-255 | 200-239 |
| m38l | MVS/CE | JOB01705 | `00CCC8` | 240-255 | 200-239 |
| m38x | MVS/CE | JOB00627 | `00CCC8` | 240-255 | 200-239 |
| m38b | TK5 | JOB01099 | `017068` | 220-229, 231-234, 241-244, 248 | 200-219, 230, 235-240, 245-247, 249-255 |
| m38r | TK5 | JOB00436 | `017068` | 220-229, 231-234, 241-244, 248 | 200-219, 230, 235-240, 245-247, 249-255 |

Free on all five: 200-219, 230, 235-239. SVC 244 (the authorization SVC
OFTPD, UFSD, FTPD and HTTPD use) is in use everywhere, as expected. No
stand has an `IEASVCxx` member (MVS 3.8j has none) or an `SVC=` in
`IEASYS00`/`IEASYS99`, so nothing beyond these tables is added at IPL.
239 read free on dev1: NSFV was not running.

## 4. A request, step by step

Client (problem state, its own key):

1. `ssct_find("OFT1")`; no SSCT, wrong eye catcher or `ACTIVE` off:
   "OFTPD is not running" (rc 12, as today when the port does not
   answer).
2. Build a request block in its own storage: eye catcher `OFTQ`, version,
   the request line (at most 256 bytes), the address and length of its
   reply buffer, a resume index (0).
3. Issue the SVC with R1 = the block (`EX` of an `SVC 0` with the number
   from the anchor, as nsf370 does).

`OFTPSVC` (supervisor state, key 0, throughout):

4. Check the anchor (eye, `ACTIVE`); copy the block in with **`MVCK`
   under the caller's key**, after checking address and length, so a
   bad pointer faults instead of a key-0 move reading anything.
5. **Read the caller's userid itself**: `PSAAOLD` X'224' -> ASCB ->
   `ASCBASXB` X'6C' -> `ASXBSENV` X'C8' -> ACEE -> `ACEEUSER` (the path
   of libc370 `racf_get_acee()`, in the caller's address space). The block
   carries no userid; OFTPD never reads one from the client. Today the
   request line carries it as `OWNER=` (`SEND`, `LIST MINE`); under the
   SVC, OFTPD ignores that keyword and uses the slot's userid.
6. Claim a slot (interlocked, as nsf370's pool). None free: answer
   `421 busy, try again` without waiting.
7. Fill the slot (owner ASCB/ASID, userid, request), count in-flight,
   POST OFTPD's ECB with `__xmpost` (CVT0PT01), WAIT on the slot's key-0
   reply ECB (legal from supervisor state, ADR-0038 unknown 1).
8. On the post: `MVCK` the reply into the caller's buffer under the
   caller's key, set the block's rc, length and resume index, free the
   slot, count in-flight down **last**, return R15 = rc.

OFTPD (the control-port thread becomes the slot thread):

9. Wakes on its ECB, takes each filled slot, runs the same `ctl_*`
   handlers with an output function that appends lines to the slot's
   reply area instead of a socket.
10. Before the reply POST: **check that the owner is still there** (ASVT
    entry for the ASID holds the same ASCB address; UNKNOWN counts as
    alive, ADR-0040). Gone: free the slot, no POST -- a POST into a
    reused ASID would wake a stranger.

**Long answers** (`LIST`, `PARTNERS`) come in pieces: when the reply area
is full, OFTPD stops, sets the resume index to the number of `1xx` lines
sent and marks the answer incomplete. The client issues the same request
again with that index; OFTPD regenerates the answer and skips what was
sent. That keeps OFTPD free of per-client state, as `design.md` section 7
asks, at the price of regenerating the list once per piece -- accepted for
the MVP (a spool LIST is a VSAM read, not a scan of anything large).

## 5. CSA

| Item | Size |
|---|---|
| anchor header | about 64 bytes |
| slot | 64 header + 256 request + 2048 reply = 2368 bytes |
| pool | 4 slots = 9472 bytes |
| `OFTPSVC` | under 2 KB, to be measured |

So about **12 KB of key-0 CSA**, held while OFTPD runs. Four slots because
requests are short and a full pool only answers `421`; the reply area at
2 KB holds about 25 `LIST` lines, the rest comes by resume. Both are
constants in one header; change them there.

## 6. Start, stop, abend

**Start** (OFTPD authorized, `SVC=` given): ESTAE first; then allocate the
anchor and pool, `__loadhi` `OFTPSVC`, patch the anchor address, steal the
slot (save the old 8 bytes in the anchor), install the SSCT, set
`ACTIVE`; `OFTP0nnI CROSS MEMORY ON SVC nnn`. **OFTPD has no ESTAE
today** (`oftp#mon.c` has none, nor does `oftpd.c`); it comes first,
with libc370 `mvs/recovery.h` (`estae()`).

**Not authorized** (`OFTP005W` today): no steal, `OFTP0nnW NO CROSS
MEMORY, NOT AUTHORIZED`, TCP only. So authorization becomes required for
the transport, not for OFTPD; `docs/installation.md` step 4 says so.

**Stop** (`P OFTPD`): clear `ACTIVE`; POST every parked slot with a
sentinel so the routine wakes, sees `ACTIVE` off and returns "OFTPD
stopped"; wait for in-flight to reach 0 (bounded, 10 s); restore the SVC
slot and read it back; remove the SSCT; free module, pool and anchor --
or, if the drain timed out, free nothing and say so (`OFTP0nnW CSA
RETAINED`): a caller may still be inside the routine.

**Abend** (the ESTAE, under RTM): restore the SVC slot (the number is
safe system-wide again), clear `ACTIVE`, POST the parked slots with the
sentinel, **free nothing**, percolate. Order and reasons as ADR-0038
section 4. A stolen slot left behind breaks that SVC number for every
program on the system, so the restore is the one step that must not be
skipped.

**No SDWA, no recovery.** libc370's ESTAE stub percolates without
calling the C routine when RTM passes no SDWA (R0 = 12, storage
shortage). Such an abend skips the slot restore and the socket close; it
cannot be fixed in C and stays a residual risk.

**After an abend** the next `S OFTPD` finds the SSCT `OFT1` still
installed with a retained anchor. MVP: it removes that SSCT and starts
fresh, and the old anchor, pool and module stay in CSA until the next
IPL (about 12 KB per abend), with a message saying so. Adopting the
retained anchor (nsf370 ADR-0048, ufsd#49) is later.

**Client gone while parked** (cancelled TSO user, `S322`): the SVRB is
purged with its task; the slot stays claimed. OFTPD's liveness check
(step 10) frees it when its answer is ready; a sweep on every wake-up
frees claimed slots whose owner is gone (ADR-0045).

**Swapping**: open. nsf370 measured a 12-minute stall of a swapped-out
STC and cures it with `SYSEVENT DONTSWAP` without explaining it
(ADR-0044). OFTPD is not a dependency the way a TCP stack is -- a stalled
OFTPD delays one `OFTP LIST` -- so the MVP does without, and adds it
(warn and continue) if the stall shows.

## 7. Transition

- Client: SSCT `OFT1` found -> SVC. `HOST(...)`/`PORT(...)` given -> TCP,
  as today. Neither -> rc 12, "OFTPD is not running".
- OFTPD: `CONTROLPORT=0` turns the TCP port off. With the SVC working on
  every stand, 0.2.0 ships with `CONTROLPORT` off by default; the TCP code
  stays for the PC side (the host driver, tests from the PC) until
  nothing uses it.
- The dialog and `OFTP` need no change above `oftp_cli_request()`.

## 8. What the toolchain has to be checked for

- **`MVCK` in as370 -- fixed in as370 1.6.3 (cc370#947, released
  2026-10-14).** as370 1.6.2 (7b7506e) assembled `MVCK 0(4,2),0(5),3` to
  `D903 2000 5000`: it read the "4" as an SS length and dropped R3, and
  `MVCP`/`MVCS` had the same defect; a missing R3 assembled at rc 0.
  POPS has R1 (the length register) and R3 (the source key) as the two
  nibbles of byte 1, `D9 | R1 R3 | B1 D1 | B2 D2`.
  Checked here with as370 1.6.3 (802dde1) on 2026-10-14, the object deck
  read back, not yet run on MVS:

  | Statement | Bytes |
  |---|---|
  | `MVCK 0(4,2),0(5),3` | `D943 2000 5000` |
  | `MVCK 16(4,10),32(11),7` | `D947 A010 B020` |
  | `MVCK 0(1,4),0(5),3` | `D913 4000 5000` (nsf370's raw `DC`) |
  | `MVCP 0(4,2),0(5),3` | `DA43 2000 5000` |
  | `MVCS 0(4,2),0(5),3` | `DB43 2000 5000` |
  | `MVCK 0(4,2),0(5)` | severity 12, "needs a third operand" |

  So the routine uses the mnemonic, no raw `DC`. **1.6.2 assembles it
  silently wrong**: raise `[toolchain] cc370` in `mbt.toml` to 1.6.3 in
  the commit that brings the first `.asm` using it.
- **`TPROT` is fine -- the nsf370 note was stale.** as370 has had `TPROT`
  as SSE since cc370#56: `TPROT 4(5),8(9)` gives `E501 5004 9008` (cc370
  session, 2026-10-14, pinned to IFOX00). So no raw `DC` is needed for
  `TPROT`; nsf370's "as370 has no SSE format" is out of date. Note that
  IFOX00 does not know `MVCK`/`MVCP`/`MVCS` at all (an undefined opcode
  there), so those three cannot be cross-checked against IFOX -- only
  against POPS.
- **Column 72.** A continuation in column 72 makes as370 swallow the next
  line without a word (nsf370, ADR-0038 history). The routine is short;
  keep its lines short.

## 9. The plan

0. ~~Measure, read-only~~ -- done 2026-10-14 (section 3): SVC 237.
1. ESTAE in OFTPD (useful on its own: it closes the listeners and the
   open connections, which otherwise hold their ports until Hercules is
   restarted).
2. The output function under `ctl_*` (socket or slot); host tests stay
   green.
3. `OFTPSVC` and the anchor; a probe client that round-trips `STATUS`.
   **Not started -- the routine still has to be written** (see section 10).
4. `OFTP`/`OFTPI` switch over; OFTPD takes the owner from the slot.
5. Tests on MVS, dev1 first, then the M38 stands:
   - `TESTAUTH FCTN=1` proves the client unauthorized;
   - a client whose request line says `OWNER=` someone else is
     answered as itself;
   - two batch clients at once; a full pool answers `421`;
   - a `LIST` longer than one reply area arrives whole;
   - a client cancelled while parked: the slot comes free;
   - `P OFTPD` with a client parked: the client gets "stopped", the SVC
     entry reads back equal to the saved one;
   - a second `S OFTPD`; `S OFTPD` after `P` steals cleanly again.
   The induced-abend path is built and reasoned through, and run only
   with the maintainer's go-ahead (it leaves a dump and retained CSA).
6. Documentation: `docs/installation.md` (four modules, `SVC=`,
   authorization), `OFTPPRM0`.

**Effort**: more than the "two to three days" first said, which was for
an SSI router in C. This is an assembler routine plus ESTAE, liveness and
sweep. With nsf370's ADRs as the map, about a week of work plus the
stand time; uncertain mainly through how many SVC numbers are free per
distribution and how the ESTAE behaves on the first live abend (the
`MVCK` encoding is settled -- section 8).

## 10. Where step 3 stands (handoff, 2026-10-14)

Steps 0, 1 and 2 are done and on `main` (`26f8995`). Step 3 -- the SVC
routine `OFTPSVC`, the CSA anchor and the STC-side install -- has not
been written. **The maintainer writes it** (decided 2026-10-14), adapting
nsf370's committed, measured `nsfvsvc.asm` / `nsfv.c`. One artefact
exists for it, on `main` since `edf7a69`:

- **`include/oftpsvc.h`**: the shared layout of the anchor, the slot and
  the client request block, with the state and rc constants. It is the
  mirror `asm/oftpsvc.asm` and the probe both build against. Review it
  first; the field offsets in it are the contract.

Nothing else for step 3 is on disk: `asm/oftpsvc.asm`, `src/mvs/oftp#svc.c`,
the `[module.OFTPSVC]` block in `mbt.toml`, the `SVC=` keyword in
`oftp#cfg.c` and the probe `test/mvs/tstsvc.c` are all still to come.

**The model is nsf370**, which solved exactly this problem and measured it
live on MVS/CE. Read, in this order:

- `nsf370/asm/nsfvsvc.asm` -- the routine to copy the *structure* of (not
  the code): entry register discipline (R4/R5 are destroyed, re-derive the
  TCB from `PSATOLD` and the SVRB from `TCBRBP`), the `CS` slot claim, the
  save/restore of registers across the branch POST in the SVRB's
  `RBEXSAVE`, the `MVCK` copy in/out under the caller's key, the exit
  paths. OFTPD's routine is a thin text-in/text-out router -- a fraction of
  `nsfvsvc.asm`, which also carries ECHO/XFER/RQE verbs OFTPD does not
  need.
- `nsf370/src/nsfv.c` -- the STC side: `nsfv_anchor_alloc` (getmain SP 241
  under `__super(PSWKEY0)`), `nsfv_router_load` (`__loadhi`, then patch the
  anchor address into the routine's word at `epa + OFTP_SVC_ANCH_OFF`),
  `nsfv_svc_steal`/`nsfv_svc_restore` (walk `CVT->cvtabend->scvtsvct`,
  verify the slot holds the modal invalid-SVC EP before taking it, write
  `C0 00 00 00` to the attribute bytes, keep the 8 saved bytes for
  restore), the liveness guard (ASVT entry for the ASID still equals the
  recorded ASCB), and the shutdown/ESTAE order.
- `nsf370/src/nsfreqc.c` / `nsf370/test/mvs/tstsvc.c` -- how an
  unauthorized client issues the SVC: `EX Rn,SVC0` with the number in a
  register.

**The relevant libc370 helpers** (all present, measured): `__super` /
`__prob` / `super_key_do` (`mvs/apf.h`), `getmain` / `freemain`
(`mvs/storage.h`, SP 241 = key-0 CSA), `__loadhi` (`mvs/link.h`, needs APF
+ key 0, reads STEPLIB only), `__xmpost` (`mvs/xmem.h`, the `CVT0PT01`
branch POST -- needs supervisor key 0, does no MODESET itself), `ssct_new`
/ `ssct_install` / `ssct_find` / `ssct_remove` (`mvs/subsys.h`), `__cas` /
`__uinc` / `__udec` (`s370/atomic.h`). The ACEE userid walk is
`racf_get_acee()`'s path: PSA X'224' -> ASCB, +X'6C' -> ASXB, +X'C8' ->
ACEE, `aceeuser` at +X'14' (`ibm/mvs/ihaacee.h`). SVC table and ASVT
structs: `ibm/mvs/ihascvt.h` (`svctable`/`svcentry`), `ibm/mvs/ihaasvt.h`
(`asvtenty`), `ibm/mvs/cvt.h` (`cvtabend` X'C8', `cvt0pt01` X'98',
`cvtasvt` X'22C').

**The two key/ECB rules that cost nsf370 debugging rounds:**

- OFTPD's wake ECB lives in OFTPD's private key-8 storage (its cthreads
  WAIT in problem state; a key-0 ECB there is `S047`). The slot's reply
  ECB is key-0 CSA and the *routine* WAITs on it in supervisor key 0,
  which is legal.
- Every store into caller-supplied storage (the reply bytes, rc, length,
  resume index) runs under the caller's key via `SPKA`; everything into
  the CSA slot runs in key 0. OFTPD builds the reply into a private buffer
  through the step-2 output function, then one key-0 window does: copy to
  the slot, liveness check, state -> DONE, `__xmpost`.

**Build order** (cross-build clean at each step, `mbt module-data` clean):
`oftpsvc.h` + `SVC=` in `oftp#cfg.c` -> `asm/oftpsvc.asm` +
`[module.OFTPSVC]` (`entry="OFTPSVC" startup=false rent=true reus=true`)
-> `src/mvs/oftp#svc.c` + the new `OFTP_MON` fields -> `oftpd.c` start/stop
wiring (restore is the *first* act of `oftp_mon_abend`) -> `test/tstsvc.c`.
A host test that asserts `offsetof()` on every `oftpsvc.h` field against
the `.asm` EQUs is the only mirror check runnable off-MVS; run it before
any deploy.

**MVS test plan**: section 9 step 5. Ask MBT for a dev1 slot first; read
the slot-237 entry with `svcdm.py` *before* and *after* (`P OFTPD` must
leave it reading back equal to the saved bytes). If an abend path or a
bad restore is suspected, do not `S OFTPD` again -- report.

**Why this handoff exists**: this session's environment stops the
assistant from writing or correcting the routine itself (it reads a
slot-stealing SVC as a privilege-escalation pattern regardless of
context). The design above is complete and the toolchain blocker (`MVCK`)
is resolved since as370 1.6.3 (section 8); what remains is writing the
four files.
