# The cross-memory transport: a dynamically installed private SVC

This is a systems-programming design for the OFTPD file-transfer service on
MVS 3.8j and compatible hobbyist/test systems. It describes ordinary
retrocomputing work: OFTPD temporarily reserves an otherwise unused SVC table
entry while it runs, then restores the saved entry on stop or recovery. The
checks below are there to avoid conflicting with other software and to keep
callers and system storage safe.

Status: proposed 2026-10-14, the transport decided by the maintainer the
same day (a dynamically installed SVC, not the subsystem interface);
mvslovers/oftpd#1.
Built and measured since, in pieces: sections 10 to 13 say what ran
where. The utilities use it since mvslovers/oftpd#22 (section 13).

This replaces the TCP control port (`design.md` section 7) as the way the
utilities (`OFTP` in TSO and batch, the dialog `OFTPI`) reach OFTPD. The
requests and answers stay exactly as they are: one request line, `1xx`
lines, one final line (`src/mvs/oftp#cli.c`). Only the transport changes.

Models, read before writing any of it:

- **nsf370** ADR-0038 (the private SVC: slot reservation, type 3, CSA routine,
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
libc370 `ibm/mvs/ihascvt.h`) and temporarily reserves it only if it holds the
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
   under the caller's key**, after checking each range with **`TPROT`**
   (section 6). `MVCK` under the caller's key is *not* on its own what
   stops a forged pointer: its source key need only permit a fetch, and
   a key-8 fetch from differently keyed storage whose fetch-protection
   bit is off is permitted -- the normal state of key-0 CSA. `TPROT`
   asking for a store is what makes the range have to be the caller's
   own. This step used to claim the `MVCK` alone was enough.
5. **Read the caller's userid itself**: `PSAAOLD` X'224' -> ASCB ->
   `ASCBASXB` X'6C' -> `ASXBSENV` X'C8' -> ACEE -> `ACEEUSER` (the path
   of libc370 `racf_get_acee()`, in the caller's address space). The block
   carries no userid; OFTPD never reads one from the client. Over TCP the
   request line carries it as `OWNER=` (`SEND`, `LIST MINE`); under the
   SVC, OFTPD drops that keyword and hands the slot's userid to the
   handlers beside the line (section 13).
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
| anchor header | 64 bytes |
| slot | 64 header + 256 request + 2048 reply = 2368 bytes |
| pool | 4 slots = 9472 bytes |
| anchor + pool, one GETMAIN | 9536 bytes (`sizeof(OFTP_ANCHOR)`, asserted at compile time) |
| `OFTPSVC` | **1144 bytes**, measured on m38x 2026-10-18 (`OFTP030I`) |

So about **10.7 KB of key-0 CSA**, held while OFTPD runs -- two
subpool-241 GETMAINs, the anchor's and `__loadhi`'s.

Each carries an 8-byte prefix *below* the returned address: libc370's
`getmain()` returns `block + 8` and `freemain()` reads the subpool and
the length back from there (`src/mvs/storage/getmain.c:89`). So
`freemain(lpa)` is the right teardown for a `__loadhi` module -- `lpa`
is the pointer `getmain()` returned -- and the addresses observed on
m38x fit exactly: module at `00A90608` + X'478' = `00A90A80`, anchor at
`00A90A88`, the eight bytes between them being the anchor's prefix.
`freemain()` also zeroes the block before the FREEMAIN, which is one
more key-0 store. Four slots because
requests are short and a full pool only answers `421`; the reply area at
2 KB holds about 25 `LIST` lines, the rest comes by resume. Both are
constants in one header; change them there.

## 6. Start, stop, abend

**Start** (OFTPD authorized, `SVC=` given): ESTAE first; then allocate the
anchor and pool, `__loadhi` `OFTPSVC`, patch the anchor address, reserve the
slot (save the old 8 bytes in the anchor), install the SSCT, set
`ACTIVE`; `OFTP0nnI CROSS MEMORY ON SVC nnn`. **OFTPD has no ESTAE
today** (`oftp#mon.c` has none, nor does `oftpd.c`); it comes first,
with libc370 `mvs/recovery.h` (`estae()`).

**Not authorized** (`OFTP005W` today): no slot reservation, `OFTP0nnW NO CROSS
MEMORY, NOT AUTHORIZED`, TCP only. So authorization becomes required for
the transport, not for OFTPD; `docs/installation.md` step 4 says so.

**Stop** (`P OFTPD`): clear `ACTIVE`; POST every parked slot with a
sentinel so the routine wakes, sees `ACTIVE` off and returns "OFTPD
stopped"; wait for in-flight to reach 0 (bounded, 10 s); restore the SVC
slot and read it back; remove the SSCT; free module, pool and anchor --
or, if the drain timed out, free nothing and say so (`OFTP0nnW CSA
RETAINED`): a caller may still be inside the routine.

**Abend** (the ESTAE, under RTM): restore the SVC slot (the number is
available system-wide again), clear `ACTIVE`, POST the parked slots with the
sentinel, **free nothing**, percolate. Order and reasons as ADR-0038
section 4. A reserved slot left behind makes that SVC number unavailable
to every program on the system, so the restore is the one step that must not be
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

**A program check inside the routine leaks its slot and the in-flight
count -- open, and it needs a decision.** The slot is claimed *before*
the first `MVCK`, and that `MVCK` reads an address the caller supplied,
so it is the most likely fault point in the routine; the reply copy-out
is the second. Both are meant to fault -- that is what the caller-key
check buys -- but the routine has no FRR or ESTAE, so the fault leaves
`state` at CLAIMED or DONE and `inflight` incremented, with no path
that puts either back. Two consequences: the slot is gone until the
next IPL (the ADR-0045 sweep covers claimed slots whose *owner* is
gone, not slots whose owner faulted and is still there), and the stop
drain in this section can never reach 0, so every later `P OFTPD`
times out into `CSA RETAINED`. With four slots, four deliberately bad
pointers from any unauthorized user disable the transport until the
next IPL.

nsf370 has no recovery routine either, so this is not a divergence from
the model -- it is a gap both share and neither has measured.

**Decided 2026-10-17: prevent the fault first, then sweep what is
left.** A recovery routine (`SETFRR`; the macro is in as370's maclib
along with `IHAFRRS`) was the third candidate and was not taken: it is
the only option that adds an unmeasured MVS mechanism to a routine that
has none, and it is not needed for the cases a caller can provoke on
purpose.

**Prevention, in the routine, done.** `TPRNG` runs `TPROT` over both
ends of each caller-supplied range before the range is touched: the
24-byte header *before the anchor is read and before any slot is
claimed*, and the request text and reply buffer right after the header
describes them. A refused range is a return code instead of a program
check. Three things about it are worth keeping in mind:

- It asks for a **store**, not a fetch, even where the routine only
  reads. That makes the range have to be in the caller's own key, which
  closes a cross-key read `MVCK` alone does not: `MVCK`'s source key
  check permits a fetch from differently keyed storage whose
  fetch-protection bit is off, the normal state of key-0 CSA. The
  routine's older comment claiming a forged pointer "fails the
  caller-key check in MVCK" was true only for fetch-protected storage.
- Two `TPROT`s per range are enough **because every range is at most
  2048 bytes**: at that size it spans at most two protection blocks
  whichever block size holds, so its first and last byte cover both.
  **The reason given here used to read "two 4K blocks", and that was
  the pre-correction one** (KB `MVS-PROT-0001`, 2026-10-18). A storage
  key covers **2 K** on S/370 and 4 K only from 370-XA on, and the
  emulator we run on builds the 2 K case: Hercules defines
  `FEATURE_2K_STORAGE_KEYS` for S/370 mode (`hyperion/feat370.h:30`)
  and sets `STORAGE_KEY_PAGESIZE 2048` under it
  (`feature.h:397-407`). Under 2 K a range of 2049 to 4096 bytes spans
  **three** blocks and a two-ended probe never looks at the middle
  one. So 2048 is the bound that is safe under either value, which is
  what `REPMAX` happens to be; raise it and the check needs a walk,
  one probe per 2 K. The granularity is still a reading of the
  emulator's source and not a measurement -- a `TPROT` at offset
  +2048 across a known boundary settles it, and that probe belongs
  with #15.
- It does not close a TOCTOU window: a second task in the caller's
  address space can free the storage between the `TPROT` and the
  `MVCK`. That window is what the sweep is for.

**The sweep, in `oftp#svc.c` -- written 2026-10-18, see section 11 --
and it is not purely OFTPD-side, which is how this was first
framed.** The leak states are
CLAIMED (the routine faulted before publishing) and DONE (it faulted
during the copy-out); PENDING is not a leak, OFTPD answers it. For
CLAIMED, OFTPD needs no clock: the claim-to-PENDING window contains no
WAIT, so a slot seen CLAIMED on three consecutive sweeps cannot be
legitimate, and the count lives in OFTPD's private storage keyed by
slot index, reset whenever the slot is not CLAIMED. The sweep also has
to decrement `inflight` for each slot it frees, because the routine
that owned it never will -- a pool scan cannot replace the counter,
since a caller between the increment and the claim owns no slot at all.

**DONE would be the hard one, and a timeout alone would be wrong.** A
DONE slot that has sat there a long time is either a routine that
faulted mid copy-out or a *parked caller that has not been dispatched
yet* -- and nsf370 measured a 12-minute stall of a swapped-out address
space (ADR-0044, this section's "Swapping"). Freeing a slot in the
second case is a live corruption: the caller resumes, copies out and
sets FREE while another caller has already claimed the same slot.
Liveness does not separate them, because the owner is alive in both.

**So the routine says when it woke: `OFTP_SL_COPYOUT`** (state 5, in
the shared header since 2026-10-17). The first thing the routine does
after the WAIT, before it reads a single field of the slot, is take it
out of DONE and into COPYOUT. The sweep then reads:

| State, and old | Means | Do |
|---|---|---|
| CLAIMED, 3 sweeps | faulted before publishing | free it |
| PENDING | OFTPD's own to-do | nothing |
| DONE | **not dispatched yet** -- may be swapped out | **nothing, however long** |
| COPYOUT, > 1 s | woke, then faulted | free it |

A copy-out is microseconds of work and by definition its task is
running, so any COPYOUT older than a second is dead. That is a fact,
not the guess a DONE timeout would have been.

It is a **`CS` from DONE, not a store.** If the sweep got there first
the slot is no longer DONE and is very likely another caller's already,
so the routine takes the `SWEPT` exit: it stores nothing into the slot
and does **not** decrement. **Whoever frees a slot owns its
decrement** -- that is the invariant the sweep has to keep, because a
second decrement here would take `inflight` below zero (the `BCTR`
loops do not guard against it) and leave every later `P OFTPD`
draining forever.

**One case remains, and it is why a long timeout still has a job.** A
client whose task is purged while parked -- a cancelled TSO command,
`S322` -- never wakes, so it never writes COPYOUT: the slot stays DONE
with its address space alive, indistinguishable from a swapped-out
caller. COPYOUT does not fix that; what it does is take the *common*
case out of the guess, so the residual timeout only has to cover a rare
one, where being generous costs nothing. Pick it in minutes, not
seconds, and say in the code that it is covering this case and not the
copy-out.

Adding state 5 does not change any offset, so `OFTP_SVC_VER` stays 1.
It does change the routine/OFTPD protocol -- harmless today because
neither side is deployed, and the reason it had to land before the
sweep rather than after it.

**Swapping**: open. nsf370 measured a 12-minute stall of a swapped-out
STC and cures it with `SYSEVENT DONTSWAP` without explaining it
(ADR-0044). OFTPD is not a dependency the way a TCP stack is -- a stalled
OFTPD delays one `OFTP LIST` -- so the MVP does without, and adds it
(warn and continue) if the stall shows.

## 7. Transition

**Finished on 2026-10-19** (#32): the TCP control port is removed, client
and server, and `CONTROLPORT=` is an unknown keyword (`OFTP010W`). The
SVC is the only way in; `oftp_cli_request()` has no host any more.
Measured in a throwaway MVS/CE container the same day, with a member
that still said `PORT=3305 CONTROLPORT=3306 SVC=237`: `OFTP010W OFTPPRM:
line 2: unknown keyword CONTROLPORT`, then `OFTP039I CROSS MEMORY ACTIVE
ON SVC 237` and `OFTP001I OFTPD READY`; nothing listened on 3306
(connection refused, 3305 answered). Twelve `OFTP` steps in one batch
job, all CC 0 (SEND with HOLD and with `DISP(DELETE)`, LIST, LIST MINE,
SHOW, HOLD by partner, RELEASE and HOLD by key, PARTNERS, DELETE, and a
LIST still carrying `HOST(127.0.0.1) PORT(3306)`, which OFTPD now passes
over); `TSTSVC` 20/20; at `P OFTPD`, `OFTP032I SVC 237 RESTORED, ENTRY
READS 0000CCC8 80000000`. The dialog was not run. What follows is how
the transition went, kept as it was written.

- Client: `oftp_cli_request()` with no host goes through the SVC, with a
  host through TCP, and never falls back from one to the other: a caller
  relying on its identity being real must not land on the port unnoticed.
  `OFTP` takes TCP when `HOST(...)` or `PORT(...)` is given, the dialog
  when its profile holds a host. No SSCT, no anchor or the transport not
  active: rc 12, "OFTPD not reached: not running, or without SVC=".
  Done in mvslovers/oftpd#22 (section 13).
- OFTPD: `CONTROLPORT=0` turns the TCP port off. 0.2.0 ships with
  `CONTROLPORT` off by default once the SVC has run on TK5 as well; the
  TCP code stays for tests from the PC until nothing uses it.
- The dialog and `OFTP` needed no change above `oftp_cli_request()`
  except their defaults: `OFTP` defaulted the host to `127.0.0.1`, and the
  dialog wrote `127.0.0.1` into the ISPF profile when the field was
  empty, so every profile would have stayed on TCP. A profile written
  before keeps its host until it is cleared under option 5.

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
  keep its lines short. Check it mechanically --
  `awk 'length($0)>=72' asm/oftpsvc.asm` must print nothing.
- **IFO213 zeroes the offending instruction, and the hard errors are
  caught by the exit status while the warnings are not.** Measured
  2026-10-17 with as370 1.6.3 (802dde1) on the first draft of
  `asm/oftpsvc.asm`: two hard errors, `Assembler Done 2 Statements
  Flagged / 12 was Highest Severity`, **exit 12**. The IFO213 statement
  really is assembled as four zero bytes -- `000202 0000 0000` in the
  listing where `58F0 F098` belongs -- and it was the load of the
  branch-entry POST address, so `BALR R14,R15` would have branched into
  the CVT. But nothing would have got that far: as370 exits with the
  severity, mbt's `as` step fails at rc >= 8 and deletes the object deck
  so a later build cannot take it for up to date, and a `cc370` step
  fails on any rc != 0 (MBT session, measured on its side the same
  day).

  **Confirmed end to end here once `[module.OFTPSVC]` existed**, which
  is the first time mbt assembled this file at all: the same bad line
  put back gives `mbt build` exit 1, `[mbt] ERROR: [as370]
  asm/oftpsvc.asm failed (rc=12)`, and no `oftpsvc.o` survives. So the
  gate that would have caught the zeroed instruction is real and is now
  in the path.

  **An earlier version of this bullet claimed as370 exits 0 at severity
  12.** It does not. That reading came from `as370 … 2>&1 | head -40;
  echo $?`, which reports *head's* status -- the shell-as-instrument
  trap in the root `CLAUDE.md`, in a session that had already caught the
  same mistake once that day. The real measurement is above; re-derive
  it with no pipe (`as370 … > out 2>&1; echo $?`) before trusting any
  exit code in this file.

- **What does pass silently is rc 4.** Warnings return 4, mbt passes
  rc 4 by design (like `COND=(8,LT)`), and as370 has no
  warnings-as-errors switch. That includes **IFO026, the column-72
  continuation** -- the one warning that silently swallows the next
  source line, which cost httprexx a `BALR` and an S0C1. So the
  mechanical check below is the defence, not the exit code: the
  `awk 'length($0)>=72'` run, and reading the output rather than the
  status. Whether rc 4 should fail a build is a policy question for the
  maintainer; the MBT session is putting it to him.
- **`mbt module-data` is an error for a module's own sources and only a
  warning for `[internal]` ones -- and everything in `src/` here is
  `[internal]`.** Two controls, 2026-10-18:

  | Writable `static` in | `mbt module-data` |
  |---|---|
  | `src/oftpd.c`, a `[module.OFTPD]` source with `rent = true` | `ERROR: writable data in OFTPD (rent = true)`, **exit 1**; `mbt build` fails too |
  | `src/mvs/oftp#svc.c`, an `[internal]` source | `WARNING: writable data in - ([internal], linked by autocall if referenced)`, **exit 0** |

  The warning path is by design: the check cannot know which module the
  autocall will pull an `[internal]` object into (MBT, 2026-10-18, ML02-0002).
  But `[internal]` is where almost all of this project's code lives, and
  it is autocalled into OFTPD, which is `rent = true` -- where writable
  module data is an S0C4. **So for anything under `src/`, read the
  output: the check is the absence of a `writable data in` line, not the
  exit code.** Closing that gap needs the `[internal]` members
  attributed per module (from ld370's map); MBT is raising it as an
  issue rather than deciding it.

  Earlier notes in this file and in two pull requests said
  "`module-data` stays clean" on the strength of the exit status. The
  conclusion held and the method did not -- and the first correction of
  it, written from the `[internal]` control alone, overstated the other
  half: it claimed the exit code is never the check. One control, one
  sentence wider than the control. A `static const` table draws no
  warning on either path; `mirror_ok()`'s is one.
- **Never EQU a field name an IBM mapping macro also defines.** Both
  errors above came from one line, `CVT0PT01 EQU 152` beside
  `CVT DSECT=YES`: the duplicate is IFO196, and `CVT0PT01-CVTMAP` then
  mixes an absolute EQU with a relocatable DSECT symbol, which is
  IFO213. Take the symbol from the macro, as libc370's
  `src/mvs/xmem/@@xmpost.c` does.

## 9. The plan

0. ~~Measure, read-only~~ -- done 2026-10-14 (section 3): SVC 237.
1. ESTAE in OFTPD (useful on its own: it closes the listeners and the
   open connections, which otherwise hold their ports until Hercules is
   restarted).
2. The output function under `ctl_*` (socket or slot); host tests stay
   green.
3. `OFTPSVC` and the anchor; a probe client that round-trips `STATUS`.
   In pieces (section 10). **Piece one -- install and restore, not
   activated -- ran on m38x on 2026-10-18 and is measured**: the SVC
   entry taken and given back, through the abend path and through the
   clean stop, read by a second party each time. The slot thread, the
   wake ECB, the SSCT, the liveness check, the sweep and the probe are
   open. The stand is **m38x** (MVSCE-EXP), not dev1: this writes into
   the SVC table of a running system, and a restore that does not take
   costs the number until the next IPL, so it belongs on the experiment
   stand rather than under the day's work (maintainer, 2026-10-18).
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
   - a second `S OFTPD`; `S OFTPD` after `P` reserves it cleanly again.
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

## 10. Where step 3 stands (2026-10-18)

Steps 0, 1 and 2 are done and on `main` (`26f8995`). Of step 3, two
artefacts are on disk:

- **`include/oftpsvc.h`**: the shared layout of the anchor, the slot and
  the client request block, with the state and rc constants. The field
  offsets in it are the contract.
- **`asm/oftpsvc.asm`**: the SVC routine, adapted from nsf370's
  committed, measured `nsfvsvc.asm`. It assembles with **zero**
  diagnostics under as370 1.6.3 and all 29 field offsets hold against
  the header under cc370. **Nothing has run on MVS.**

`[module.OFTPSVC]` and the `SVC=` keyword are in as well: mbt assembles
and links the routine (`entry=OFTPSVC, no crt`), `mbt module-data` stays
clean, and `SVC=` takes 200-255 with **no default**, an out-of-range
number being an error rather than a silent 0 (`test/tstcfg.c`).

### Piece one: install and restore, deliberately not active

`src/mvs/oftp#svc.c` is on `main` with the first of the three pieces
step 3 was split into. It installs the transport and gives it back, and
it does **not** activate it:

- `oftp_svc_start()` -- the mirror check, one subpool-241 GETMAIN of
  `sizeof(OFTP_ANCHOR)` (9536 bytes, anchor plus the four slots) in key
  0, `__loadhi OFTPSVC`, the anchor address patched into the module's
  word at EP+4, and the SVC entry reserved.
- `oftp_svc_stop()` -- clear `ACTIVE`, drain, restore, **read the entry
  back** and compare it with the saved bytes, and free the CSA only if
  the drain and that read-back both agree.
- `oftp_svc_restore()` -- the table write alone, shared by the stop and
  the ESTAE, and the first act of `oftp_mon_abend()`.
- `OFTP_MON` carries all of it (`anchor`, `svc_lpa/epa/size`,
  `svc_slot`, `svc_saved[8]`, `svc_taken`); OFTPD is AC(1) and RENT, so
  the ESTAE has to reach the state through the monitor block.

**`OFTP_ANC_ACTIVE` stays clear, and that is what makes piece one safe
to put on a shared system.** The routine tests `ANCFLAG` X'80' *before*
it claims a slot (`asm/oftpsvc.asm`, right after the in-flight count),
so a caller of the SVC number during the test counts itself in, finds
the flag off and returns `OFTP_SVC_DOWN`. Nothing can park in a slot,
there is no slot to leak, and the drain is trivially zero. The flag goes
on with piece two, together with the slot thread, the wake ECB, the SSCT
and the sweep.

**"Free" is the modal entry point and no lock.** The entry point an
unused slot carries differs per distribution (section 3), so the test
cannot be a constant and must not be zero: `unused_ep()` takes the entry
point shared by most of 200-255, exactly as nsf370's `src/nsfsx.c` does
it (measured on MVS/CE). On top of that the slot's `svclock` at +6 has
to be zero -- the FLIH takes the locks named there before entering, and
the routine is written for a type-3 entry holding none. Only byte +4 is
written, `X'C0'`: `SVCTYPE3` in the top three bits with `svcapf` off,
because reaching the routine from an unauthorized caller is the whole
point. Bytes +5 to +7 stay as found and come back from `svc_saved`.

### The first run on MVS, m38x, 2026-10-18

The install works and the first stop abended. Both halves are worth
keeping, because the second is what the recovery design was for.

| | |
|---|---|
| Read 1, before anything (`JOB00635`) | SVC 237 at `0101C8` = `0000CCC8 80000000`; modal EP `0000CCC8` shared by **40** of slots 200-255 |
| `S OFTPD` | `OFTP000I oftpd 0.1.0-dev (2740462)`, `OFTP004I AUTHORIZED BY SVC`, `OFTP031I SVC 237 ENTRY FOUND 0000CCC8 80000000`, `OFTP030I … ANCHOR=00A90A88 MODULE=00A90608 (1144 BYTES), NOT ACTIVE` |
| Read 2 (`JOB00637`) | `00A90608 C0000000`; modal EP now shared by **39** -- the one missing is 237 |
| `P OFTPD` | `OFTP002I`, then `OFTP098E OFTPD ABEND, 2 SOCKETS CLOSED, SVC RESTORED`, `IEF450I … ABEND S0C4`. **No `OFTP032I`.** |
| Read 3 (`JOB00638`) | `0000CCC8 80000000`, **40** slots again, byte-identical to read 1 |

Three things that settle questions this file had open:

- **The entry point is read the same from both sides.** `OFTP031I`'s
  eight bytes equal the read-only job taken before the test, and the
  modal count moving 40 → 39 → 40 says exactly one slot left the free
  set and came back. That is why the message exists: the install and
  the measurement are not the same party.
- **Authorization is SVC 244 all the way.** `AUTHORIZED BY SVC` on a
  stand whose library is not APF, and `__super(PSWKEY0)` then reached
  key 0 and wrote the table. No `IEAAPF00` entry, no IPL.
- **The invalid-SVC entry is a type-2 entry**, `0000CCC8 80000000`:
  byte +4 is X'80', and the `svclock` at +6 is zero, which is what the
  free test requires.

**The abend: a key-8 store into key-0 CSA.** `oftp_svc_stop()` cleared
`OFTP_ANC_ACTIVE` as its first act, with no `__super` bracket, and the
anchor is key-0 CSA while OFTPD runs key 8. The identical store inside
`oftp_svc_restore()` has the bracket -- which is why the recovery path
ran to completion and read 3 came back clean. **The one step whose
absence costs an IPL survived its own stop path abending**, because it
is the first act of `oftp_mon_abend()` and not something the stop path
reaches on its way through.

Fixed in `c138465` (`0720819` after the rebase onto `main`), and the
drain's read of `inflight` now takes the bracket too: subpool 241 is
*meant* to be non-fetch-protected, which would make a key-8 read legal,
but nothing has measured that here and the cost of being wrong is
another abend and another stand slot. It is now oftpd#15, with `TPROT`
as the instrument and the three ranges to probe.

**It informs piece two, it does not block it** -- an earlier wording
here said the slot thread "reads slots from key 8 continuously", which
is wrong about this design: the thread waits on the wake ECB and does
not poll (section 7's key/ECB rules), so the key-0 brackets cost one
per wake-up plus one per sweep tick, the order of magnitude piece one
already pays for its `inflight` read. Piece two can be written with
every CSA read bracketed; #15 decides whether the bracket comes off
afterwards.

The build ids in the two run tables below are what the running program
printed, i.e. the commits as they were on the branch. The branch was
rebase-merged, so those ids are not in `main`'s history -- do not
"correct" them, they are observations.

What the abend left on the stand is the documented behaviour of section
6 and not extra damage: the anchor and the module, about 10.4 KB of
CSA, stay until the next IPL. It is also the reason the first run
belonged on the experiment stand.

**No dump was read, and this paragraph has been wrong twice.** First it
claimed the procedure allocates none. It does: `//SYSUDUMP DD SYSOUT=*`
is the last line of `SYS2.PROCLIB(OFTPD)` on m38x, in the original and
in the test version alike (MBT, 2026-10-18). It then said the dump must
be missing somewhere in the recovery path and named the SDWA dump
options as the suspect. **That rests on nothing either**: the only thing
read on that stand was the Hercules console log, and a `SYSUDUMP` goes
to SYSOUT, not to the console -- the spool was never looked at. Since
the reset of 2026-10-18 the evidence is gone.

What is known is only that no dump was *read*. Whether one was written
is open, and oftpd#12 makes "it was written and nobody looked" the
likelier reading: on dev1 OFTPD leaves a `SYSUDUMP` at **every** end
(`S30A`, twelve STCs). The lesson that survives is the one that cost the
time: the fix below was reasoned out of the source rather than read out
of a dump, and the next abend gets its spool read first.

What the fix rested on instead: the absent `OFTP032I` placing the
fault before the read-back, the S0C4 being a protection exception, and
this being the only unbracketed store into CSA in the program. It made
a prediction -- the next stop prints `OFTP032I` and ends `OFTP003I`,
with no `OFTP098E` and no `IEF450I` -- and the run below is that
prediction tested, not a repetition hoping for a better outcome.

### The second run, same stand, after the one-line fix

| | |
|---|---|
| Read 1 (`JOB00640`) | `0000CCC8 80000000`, modal EP in **40** slots |
| `S OFTPD` | `OFTP000I … (bb17f61)`, `OFTP031I … 0000CCC8 80000000`, `OFTP030I … ANCHOR=00A93A88 MODULE=00A90188 (1144 BYTES), NOT ACTIVE` |
| Read 2 (`JOB00641`) | `00A90188 C0000000`, modal EP in **39** |
| `P OFTPD` | `OFTP002I`, `OFTP032I SVC 237 RESTORED, ENTRY READS 0000CCC8 80000000`, `OFTP003I OFTPD ENDED` -- **no `OFTP098E`, no `IEF450I`** |
| Read 3 (`JOB00642`) | `0000CCC8 80000000`, modal EP in **40** again, byte-identical to read 1 |

So **piece one is measured on MVS from both sides and in both
directions**: install and restore through the abend path in run 1
(`JOB00637`/`JOB00638`), install and restore through the clean stop in
run 2 (`JOB00641`/`JOB00642`). Every reading was taken by a party that
did not write the code, and `OFTP031I`/`OFTP032I` agree with it byte
for byte each time.

Two details worth keeping:

- **The addresses moved between runs** -- module `00A90608` → `00A90188`,
  anchor `00A90A88` → `00A93A88`, and in run 2 they are no longer
  adjacent. That is the 10.4 KB run 1 retained, sitting in the middle
  of subpool 241. A visible consequence of an abend path that frees
  nothing, and a reminder that these addresses are per-load.
- **Run 2 left nothing behind.** The clean stop reached the FREEMAINs,
  because it only frees once the drain and the read-back agree.
  `OFTP021W SPOOL WAS NOT CLOSED (OPEN RC 4), VERIFIED` at the start of
  run 2 is run 1's abend showing up in the spool -- oftpd#2 getting an
  unplanned live test, and passing it.

Still to come: the slot thread, the wake ECB, the SSCT, the liveness
check and the sweep, then the probe `test/mvs/tstsvc.c`. None of it can
be exercised off MVS. Piece one is provable by reading the SVC entry
back before `S OFTPD`, after it and after `P OFTPD` -- section 3's
chain, through HTTPD's `/.dm` on dev1 or a BREXX `STORAGE()` job on an
M38 stand; there is no `svcdm.py` (MVS test plan, below). OFTPD now
prints the same evidence itself: `OFTP031I` names the 8 bytes found
before the write and `OFTP032I` the 8 read back after the restore, so
the console and the job cross-check each other.

### The mirror check cannot be a host offsetof() test

`slot.ascb`, `slot.rplyptr`'s neighbours and `anchor.ascb`/`ecb` are
pointers, so on a 64-bit host every field from `slot.ascb` on sits 4 or
more bytes further along than its EQU and `sizeof(OFTP_SLOT)` comes out
2376 instead of 2368 -- 24 of 28 offsets "fail" for reasons that have
nothing to do with the `.asm`. Measured 2026-10-17. So the check belongs
where only cc370 compiles, and it is in `oftp#svc.c`.

**It cannot be one compile-time assert, and the example this section
used to carry was wrong.** It read

```c
A(sl_reply, offsetof(OFTP_SLOT,reply)==320);    /* does not compile */
```

and that does not build: libc370's `offsetof` is the old
`(size_t)&(((x *)0)->y)` form (`include/stddef.h:48`), which is not an
integer constant expression, so a file-scope array size using it is a
variably modified type. The obvious escape is not there either --
**cc370 has no `__builtin_offsetof`**; it is a gcc 3.4.6 fork and the
builtin is absent (measured 2026-10-18: `warning: implicit declaration
of function '__builtin_offsetof'` and then a syntax error).

So the check is in two halves:

- **Compile time**, where `sizeof` makes it an integer constant
  expression: `sizeof(OFTP_SLOT) == 2368`,
  `sizeof(OFTP_ANCHOR) == 64 + 4 * 2368`, and the constants
  `OFTP_SVC_NSLOT`, `OFTP_SVC_REQMAX`, `OFTP_SVC_REPMAX`. Verified to
  bite: widening `slot.rsv` by four bytes gives
  `error: size of array 'oftp_svc_sizes' is negative` and `mbt build`
  exit 1 (2026-10-18).
- **Run time**, in `mirror_ok()`, called from `oftp_svc_start()` before
  anything is allocated or installed. It carries one row per EQU of the
  `.asm` and refuses the transport with `OFTP034E` naming the field, its
  offset and the expected one.

The run-time half sits there rather than in a test on purpose: the
install is what depends on the layout, so a field that moved stops the
install instead of corrupting a slot in CSA, whether or not anybody
remembered to run the test. Keep the EQUs, the header and that table in
one commit.

### Three things in the routine that are load-bearing and silent

Found by review on 2026-10-17, each one a bug that reached a clean
assembly first:

- **`aceeuser` is length-prefixed** -- byte 0 the length, the text from
  byte 1 (`char aceeuser[9]` at X'14'). An 8-byte `MVC` from +0 puts the
  length byte into `slot.user[0]`, shifts the name by one and drops its
  last character: it corrupts the one field the whole transport exists
  to establish. The routine blank-fills the field and then moves only
  the declared bytes from `+1`, clamped to 8, like libc370's consumers
  (`httpd/src/httpxauth.c`, and oftpd's own `src/mvs/oftp#cli.c`).
- **The branch POST needs R10** = the completion code. It is required
  input, but *not* because POST would otherwise leave the ECB
  unposted -- see the interface note below, which corrects the first
  version of this bullet.
- **The reply WAIT must not be a loop.** OFTPD stores the reply and the
  state *before* the POST, so a wake-up in any other state is a
  protocol error. Re-WAITing on the already-posted ECB returns at once:
  a tight spin in supervisor state, key 0, in the caller's address
  space, with no way to break in -- httpd#199 / mvsmf#298 one level
  further down. The routine answers `RCBAD` and releases the slot
  instead, as nsf370 bails to `WQUIES`.

And one rule the design states that is easy to half-keep: **no caller
storage is touched before the `MVCK`**, the eye catcher included. A
key-0 `CLC` on the caller's pointer is a 4-byte read oracle on any
address and an S0C4 in key 0 on a bad one; the eye catcher is checked
on the staged key-0 image, where it costs nothing.

### The branch-entry POST interface, from IBM's source

Read in `mvs38-ibmsrc` by the libc370 session on 2026-10-17, because
the first version of the R10 bullet above had the right fix and the
wrong reason. Primary source, not inferred from libc370's C:

- **POST sets the post bit itself.** Every path in
  `ibm/IEA/IEAVSY50.asm` does `N XRPTCODE,OFFWAIT` then
  `O XRPTCODE,ONCOMPL` before storing the ECB (lines 966, 1101, 1359,
  1386, and the cross-memory SRB path at 1498); `OFFWAIT DC XL4'7FFFFFFF'`
  and `ONCOMPL DC XL4'40000000'` at 2013. So libc370's
  `postcode | 0x40000000` in `@@xmpost.c` is redundant, and an omitted
  R10 does **not** leave the ECB unposted.
- **R10 is required all the same, for two other reasons.** The
  interface block at `IEAVSY50.asm:628-646`: bits 2-31 are the
  completion code whoever waits reads, and **the high bit requests the
  ECBKEY option, with the key taken from R0 byte 3** (notes 3 and 7;
  the XM path tests it at `ECBVALCK`, line 1660, `LTR`/`BNM`).
  libc370's `& 0x7FFFFFFF` is what rules that out. So the constant is
  exactly `X'40000000'`: completion code 0, ECBKEY off. Leaving a
  24-bit TCB address there posts the ECB with a garbage code and, by
  luck of bit 0 being off, no ECBKEY request.
- **R9 *and* R14 survive the call**, not R9 alone: exit notes at
  `IEAVSY50.asm:675-680`, "REGS 9,14 ARE SAVED OTHERS ARE LOST IF LOCAL
  LOCK IS NOT HELD". libc370's `@@xmpost.c` comment and nsf370's say
  only R9. The routine saves R14 in `RBEXSAVE` anyway, so this costs
  nothing -- but do not tighten it on the strength of those comments.

The same read settled `aceeuser`: the layout is IBM's, not a
convention of its consumers -- `macros/maclib/IHAACEE.asm:53-55` is
`ACEEUSER DS 0CL9` / `ACEEUSRL DS AL1` / `ACEEUSRI DS CL8`. libc370's
`ihaacee.h:22` collapses the three into `char aceeuser[9]`, which is
where the trap comes from -- **libc370#581 will show the sub-fields**
for `aceeuser` and `aceegrp` (the arrays stay, so this stays true of
any libc370 before it), and the routine's comment can name
`ACEEUSRL`/`ACEEUSRI` once it lands. `ACEEUSRI` is treated as blank-padded to 8
by IBM's own code (`ibm/IEF/IEFSMFIE.asm:395`, `HASPSSSM.asm:9055`) and
by RAKF (`ICHSFR00.hlasm:527` compares `=CL8'STC'`), so a plain 8-byte
move from `+1` would also have been correct; the length-clamped move is
correct either way. Not read: the RACINIT path that *builds* the ACEE,
so the padding is three consumers' worth of evidence, not the
producer's.

**The model is nsf370**, which solved exactly this problem and measured it
live on MVS/CE. Read, in this order:

- `nsf370/asm/nsfvsvc.asm` -- the routine to copy the *structure* of (not
  the code): entry register discipline (R4/R5 are destroyed, re-derive the
  TCB from `PSATOLD` and the SVRB from `TCBRBP`), the `CS` slot reservation, the
  save/restore of registers across the branch POST in the SVRB's
  `RBEXSAVE`, the `MVCK` copy in/out under the caller's key, the exit
  paths. OFTPD's routine is a thin text-in/text-out router -- a fraction of
  `nsfvsvc.asm`, which also carries ECHO/XFER/RQE verbs OFTPD does not
  need.
- `nsf370/src/nsfv.c` -- the STC side: `nsfv_anchor_alloc` (getmain SP 241
  under `__super(PSWKEY0)`), `nsfv_router_load` (`__loadhi`, then patch the
  anchor address into the routine's word at `epa + OFTP_SVC_ANCH_OFF`),
  `nsfv_svc_reserve`/`nsfv_svc_restore` (walk `CVT->cvtabend->scvtsvct`,
  verify the slot holds the modal invalid-SVC EP before reserving it, write
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

**MVS test plan**: section 9 step 5. Ask MBT for a slot first; read the
slot-237 entry three times -- before `S OFTPD`, after it, and after
`P OFTPD`. The third read must give back what the first one did: the
invalid-SVC entry point, `00CCC8` on MVS/CE and `017068` on TK5, and
**not** zero. If an abend path or a bad restore is suspected, do not
`S OFTPD` again -- report.

**There is no `svcdm.py`.** It appeared in this file and in `TODO.md`
as the tool that would prove the install, and it was never written
(MBT, 2026-10-18: nothing of that name under `~/repos/mvs` or
`~/.mbt`). The read is section 3's chain -- `CVT` -> `CVTABEND` X'C8'
-> `SCVTSVCT` X'84' -> `+237*8` -- through HTTPD's `/.dm` on dev1, or a
BREXX `STORAGE()` batch job on an M38 stand, as the 2026-10-14 table in
section 3 was taken. Name the job or the `/.dm` call in the notes, so
"free" carries its evidence rather than being asserted. And **re-read
at the start of the slot, before anything writes**: the 2026-10-14
reading is memory, not inventory.

**Which stand takes the first install is open** (MBT put it to the
maintainer, 2026-10-18). Writing into the SVC table of a running shared
system is not a deploy: a restore that does not take leaves SVC 237
pointing at CSA that OFTPD has freed, and that state lasts until the
next IPL. dev1 carries libc370, oftpd and mvsMF work all day, so a
stuck restore there sits under all of them; m38x (MVSCE-EXP) is the
experiment stand and is MVS/CE like dev1, so the same invalid-SVC entry
point and the same chain apply.

**On the earlier handoff**: this section used to say that a session could
not write or correct the routine, because taking an SVC table entry reads
as privilege escalation out of context. That was overcautious and is
withdrawn (2026-10-17). An SVC routine is MVS's own sanctioned way for an
unauthorized problem-state program to reach a service -- the platform's
syscall mechanism, which is the whole argument of section 1 -- the stands
are the maintainer's own emulated MVS 3.8j systems, and the review that
produced the three fixes above was work on identity handling and caller
isolation, i.e. it made the routine stricter. Write and review it like any
other module in the ecosystem.

## 11. Piece two: the slot thread, the SSCT and the sweep (2026-10-18)

Written on `issue-16-xmem-piece-two`, host build and 220/220 host
tests clean. **Nothing in this section has run on MVS.** What it
describes is code and the reasoning behind it; `test/mvs/tstsvc.c`
and its JCL are the measurement, and they need a stand and a slot.

### What was added

| Where | What |
|---|---|
| `src/mvs/oftp#svc.c` | `oftp_svc_activate` / `_deactivate`, `_take`, `_answer`, `_sweep`; the SSCT install and removal; the STOP posts in the drain and in the ESTAE |
| `src/mvs/oftp#svcc.c` | the client side: `ssct_find` -> anchor -> `EX`-of-`SVC 0` |
| `src/mvs/oftp#mon.c` | the slot thread, `put_slot`, the `OWNER=` substitution, `oftp_mon_svc_start` / `_stop` |
| `include/oftpsvc.h` | `OFTP_SVCJOB`, the `OFTP_SW_*` clocks, `oftp_svc_call` |
| `test/mvs/tstsvc.c`, `.jcl` | the end-to-end probe from an unauthorized program |

### The division is by storage key, and that is what makes #15 optional

Everything that touches the anchor lives in `oftp#svc.c` and runs
inside the key-0 bracket. Everything above it -- the slot thread, the
`ctl_*` handlers, the reply formatting -- works on an `OFTP_SVCJOB`
and a plain buffer in OFTPD's own key-8 storage. A slot is read once,
under the bracket, into private storage; the answer is written back
the same way.

Two things follow, and the second is the one worth stating:

- A request handler cannot cause a key-0 store by being wrong about a
  pointer. The handlers are the part of OFTPD that keeps changing.
- **Whether a key-8 read of subpool 241 is legal cannot affect this
  code's correctness.** Every read is already bracketed, so #15 can
  only ever let a bracket come off -- it can never force one on, and
  it never blocked writing this.

**The client is the exception, and it is unavoidable.** An
unauthorized program cannot bracket anything, so `oftp#svcc.c` does
two key-8 fetches of key-0 CSA: the SSCT chain, to find OFTPD, and the
anchor header, for the SVC number and the ACTIVE flag. What makes that
defensible rather than hopeful is one fact that was read rather than
assumed: **libc370's `ssct_find()` contains no MODESET** -- it takes
`CVTJESCT`, follows `JESSSCT` and walks `SSCTSCTA`, all in whatever
key its caller is in (`src/mvs/subsys/@@ssfind.c`). It is an ordinary
library function with no authorization requirement in its contract,
so libc370 expects a problem-state key-8 caller to read the SSCT
chain, and the SSCT chain is key-0 CSA.

That is the whole of the evidence. It says libc370 relies on the
fetch-protection bit being off for that storage, exactly as
`MVS-PROT-0001` requires for the fetch to be permitted; it does not
establish that subpool 241 generally is, and no program's behaviour
other than libc370's own source was examined. **#15 is the
measurement.**

### Who frees a slot owns its decrement -- read off the routine, not the prose

The exits in `asm/oftpsvc.asm` decide this, and they were read rather
than assumed:

| Exit | Slot | `inflight` | So OFTPD must |
|---|---|---|---|
| `RETREL` (served) | routine sets FREE | routine decrements | nothing |
| `FREEDEC` (`BADHDR`/`BADREPLY`/`BADSTATE`/`POSTBAD`) | routine sets FREE | routine decrements | nothing |
| `STOPDEC` (woken in STOP: `RCSTOP`, since #34) | routine sets FREE | routine decrements | **not free, not decrement** |
| `WAKEDOWN` (woken in another state with ACTIVE off) | routine sets FREE | routine decrements | **not free, not decrement** |
| `SWEPT` (its `CS` from DONE lost) | untouched | untouched | **free and decrement** -- the sweep took it |

So OFTPD decrements in exactly one place: after it has **won** a
`CS` that freed a slot. A second decrement takes the count below zero
-- the routine's `BCTR` loops have no floor -- and every later
`P OFTPD` drains forever.

That is also why the stop posts parked slots and frees nothing: every
wake-up of a parked slot takes `STOPDEC` and cleans up after itself.
Until #34 the routine tested `ANCFLAG` before the state, and that
discarded an answer: the stop clears ACTIVE before it joins the slot
thread, so a request being served at that moment was carried out,
answered DONE, and its caller told `OFTP_SVC_DOWN`. The state is read
first now: DONE delivers the answer whatever ACTIVE says, and STOP
returns `OFTP_SVC_STOPPED` (20), "OFTPD stopped before carrying out
the request", which the old code could not tell from "not running".

### Three clocks in the sweep, and `stamp` serves only one

`stamp` is OFTPD's *answer* time, so it cannot time a copy-out: a
swapped-out caller may wake five minutes after the answer and finish
in microseconds. Freeing that slot on age is a live corruption.

| State | Clock | Why |
|---|---|---|
| CLAIMED | 3 sweep passes, private per slot | the claim-to-PENDING window holds no WAIT |
| COPYOUT | 2 sweep passes, private per slot | the routine says when it woke; a copy-out is microseconds |
| DONE | `stamp`, 300 s | the purged caller (`S322`) that will never write COPYOUT -- and nothing else |

**The passes are the clock, so a pass has to be worth about a second
whatever the traffic is.** The slot thread wakes on every POST, but it
sweeps only when `time()` has moved on. Without that, three passes
could be three milliseconds under load, and a caller merely preempted
inside the CLAIMED window would lose its slot.

### The wake ECB, and the one ordering that loses a request

The thread zeroes `svcwake` **before** the slot scan, never after. A
POST landing between the two is found by the scan; one landing after
it leaves the ECB posted and the next wait returns at once. The other
order drops a wake-up and nothing notices -- the caller waits until
the stop.

`ecb_timed_wait(&m->svcwake, 100, 0)` is one second. libc370 issues
`TTIMER CANCEL` after the WAIT, so a real POST leaves no armed timer
behind; and a negative rc means STIMER REAL failed and **nothing
waited** (libc370 #94), so that case falls back to the untimed
`ecb_wait` rather than spinning.

### Ordering at start and stop

Start: publish the wake ECB -> install the module and reserve the SVC
entry -> start the slot thread -> install the SSCT -> set ACTIVE.
`oftp_svc_activate()` does the last two, and from the instruction
after the flag goes on any program on the system can park a request.
A stale `OFT1` found at start is unchained by name (`ssct_remove`
never dereferences `SSCTSUSE`) and **not** freed, and the anchor it
names is not touched at all.

Stop: ACTIVE off and the slot thread joined (`oftp_mon_svc_stop`) ->
SSCT unchained -> drain, posting parked slots **each pass** -> restore
the SVC entry -> read it back -> free. The thread has to be joined
before the anchor is freed or it reads freed CSA, which is why the
stop is two calls in `oftpd.c` and not one.

**The STOP post is inside the drain loop, not once before it.**
Clearing ACTIVE does not stop a caller already past the flag test: it
goes on to claim, publish and WAIT. Posting once misses every one of
those, and each costs the drain its ten seconds and the caller its
session.

ESTAE: `restore_locked()` stays first -- it is the step that costs an
IPL -- then ACTIVE off, then the STOP posts, then percolate. A parked
caller WAITs in key 0 in its own address space, so an OFTPD that dies
without waking it leaves a task that cannot even be cancelled.

### Open after this piece

- **Nothing has run on MVS.** `test/mvs/tstsvc.c` is the probe and it
  needs a stand with OFTPD up and `SVC=` set.
- **#15 has a program now too** (`test/mvs/tstprot.c`), and it turned
  up a fact that changes how `TPROT` can ever be used here:
  **`TPROT` is privileged.** Hercules checks it with `PRIV_CHECK` in
  its `E501` handler (`control.c`, `test_protection`), so a
  problem-state caller gets S0C2. Three consequences. The probe job
  needs an **APF-authorized STEPLIB** — it checks `__isauth()` and
  fails rather than skipping, because a job that passes because it
  was not allowed to look is worse than no job. It needs no OFTPD,
  because with authority it can do #15's original recipe — its own
  `getmain(n, 241)` and `__loadhi()` — and probes OFTPD's live anchor
  only as a confirmation when one is up. And, the part that is a
  design fact rather than a test detail: **an unauthorized caller can
  never check its own access**, so the service has to do every check
  on the caller's behalf, which is what `TPRNG` in the routine
  already is.
- **The SSCT block is unchained and never freed**, ours included: 24
  bytes of CSA per start/stop. The chain walk in another address space
  is unserialised on MVS, so freeing it has a window that unchaining
  does not, and 24 bytes is the cheaper side. nsf370 took the same
  decision (ADR-0048 5.2). Revisit only with a reason.
- **`OWNER=` substitution is untested against a real ACEE.** The
  routine reads the userid length-prefixed and correctly
  (`ACEEUSER` as the length, text from +1 -- KB `ECO-0008`), and the
  thread drops any caller-supplied `OWNER=` and appends the slot's.
  What no test shows yet is a *second* userid seeing only its own
  transfers, because the reply of `STATUS` carries no userid to read
  it off. `LIST OWNER=` from two ids on a stand is the measurement.
  *Since #22 the substitution is gone (section 13): the userid travels
  beside the line, and two owners are measured there -- two RAKF
  userids still are not.*
- **A caller whose userid is blank gets no `OWNER=` at all**, which is
  the pre-SVC behaviour and no worse than the TCP port. It arises only
  where the caller's address space has no ACEE.
- **`USER=` is left to the caller on purpose**, and that needs saying
  before somebody "fixes" it. It is not an identity on this system:
  `ctl_send()` puts it in the spool record's `user`, which travels in
  the OFTP SFID user-data field (`oftp#cmd.c`, +48/+52) as free-form
  text agreed between partners. `OWNER=` is the identity and it is
  the one substituted.
- **The `552` path is unreachable with today's handlers**, and the
  bound is structural: every reply line is `snprintf`ed into a
  `char out[N]` local and the largest `N` in `oftp#mon.c` is 200
  (`ctl_list`, `ctl_show`), i.e. 201 bytes with the newline against
  an `OFTP_SVC_REPMIN` of 256. A handler that grows its buffer past
  255 breaks it, which is why the branch exists and says so rather
  than looping.
- **A clock set back parks the DONE sweep** until wall time catches
  up with the stamps already written: the age test is
  `now > stamp && now - stamp > OFTP_SW_DONE_SEC`, deliberately, so
  that unsigned arithmetic cannot turn a stamp in the future into a
  huge age and free a live slot. The cost is that an operator
  `SET CLOCK` backwards suspends that one case. Harmless -- the slot
  sits -- but it is an hour of diagnosis if it is not written down.
- `CONTROLPORT` still defaults on. Turning it off is the transition
  step (section 7) and deliberately not part of this piece.

## 12. Measured: both halves, in a throwaway container (2026-10-18)

Run in a disposable MVS/CE instance on the mvsdev Docker host
(`mvsce-target`, image `ghcr.io/mvslovers/mvsce-builder`
`sha256:8b826905…`, Hercules `a742398074`), not on a shared stand.
Instance `t6b7813`, torn down afterwards. OFTPD build `2af8b3f`.

**Everything below is MVS/CE.** TK5 is unverified for all of it, and
the storage-key granularity is *not* settled by this run -- see below.

### What had to be learnt before anything could be measured

**APF needs the library AND `AC(1)` on the module, and a test module
is `AC(0)`.** `SYS2.LINKLIB` is in `IEAAPF00` as `SYS2.LINKLIB MVS000`
and the data set really is on `MVS000` -- both halves of the pairing
checked, because an entry whose volume does not match authorizes
nothing. TSTPROT run from there still answered `__isauth() == 0`. The
missing half was the link attribute; mbt takes `ac` on a `[test.*]`
just as on a `[module.*]`. Its own guard is what caught this: the job
fails when it is not authorized rather than measuring nothing.

### #15: a key-8 fetch of subpool 241 is legal, a store is not

| Probed | key 8 answers |
|---|---|
| ordinary key-8 heap (the control) | **CC 0** store+fetch |
| `getmain(9536, 241)` at `00A90A88` | **CC 1** fetch only |
| `__loadhi(OFTPSVC)` at `009F6008` | **CC 1** fetch only |

The control answering CC 0 is what makes the other two readable; a
control that does not invalidates the run.

So **the client in `oftp#svcc.c` is allowed to read the anchor**, and
the assumption piece two shipped on is now a measurement. A key-8
*store* is refused, which is the other half one would want: an
unauthorized caller cannot corrupt the anchor. The brackets on
OFTPD's side can come off the read path if that is ever worth doing;
nothing requires it, and they cost one MODESET per wake-up.

### The granularity is NOT settled, and the first answer was an artefact

The scan looks for a storage-key boundary at an odd 2 K, which 4 K
keys cannot produce. **Its first run reported "no 2K-only boundary"
and that was a property of my grid, not of the machine**: the scan
started at `base - 32K` with `base` an arbitrary `getmain()` address,
so every probe sat at that address modulo 2048 -- `0x288` -- and the
test for `0x800` could not be true for any input whatsoever. The
assertion above it passed while this was so. Fixed by aligning the
grid to the step.

Aligned, the transitions are clean and all 4 K aligned:

```
00A8D000  1 fetch only -> 2 no access
00A90000  2 no access  -> 1 fetch only
00A93000  1 fetch only -> 3 unavailable
```

**That still settles nothing**, and the test now says so instead of
concluding 4 K. MVS hands out storage in pages, so it never creates a
boundary at an odd 2 K even on hardware whose keys are 2 K: the
absence is what *both* candidates give. A positive would have been
decisive; an absence is not. Settling it needs `SSK` on one half of a
page and `ISK` on the other, which no design currently needs --
`REPMAX` is 2048 and the two-ended `TPROT` check is sound under
either value.

### #16: the transport carries a request, 18/18

The SVC table entry, read three times by a **second party** (a BREXX
`STORAGE()` job walking `CVT` → `CVTABEND` → `SCVTSVCT` → `+237*8`),
each reading agreeing with OFTPD's own message:

| When | `SVC 237` | OFTPD said |
|---|---|---|
| before the start | `0000CCC8 80000000` | `OFTP031I … FOUND 0000CCC8 80000000` |
| transport active | `00A90608 C0000000` | `OFTP030I … MODULE=00A90608` |
| after `P OFTPD` | `0000CCC8 80000000` | `OFTP032I … RESTORED` |

`C0` at +4 is SVCTYPE3 with `svcapf` off, as `reserve()` writes it.
SVC 235, 236, 238 and 239 were untouched throughout.

The console, start to end, with **no sweep message of any kind** --
no `OFTP060W`, `061W`, `064W` or `065E`:

```
OFTP000I oftpd 0.1.0-dev (2af8b3f) STARTING
OFTP004I AUTHORIZED BY LIBRARY
OFTP031I SVC 237 ENTRY FOUND 0000CCC8 80000000
OFTP030I CROSS MEMORY ON SVC 237, ANCHOR=00A90A88 MODULE=00A90608 (1144 BYTES), NOT ACTIVE
OFTP039I CROSS MEMORY ACTIVE ON SVC 237, SUBSYSTEM OFT1
OFTP001I OFTPD READY
OFTP002I OFTPD STOPPING
OFTP032I SVC 237 RESTORED, ENTRY READS 0000CCC8 80000000
OFTP003I OFTPD ENDED
```

What the client job showed, from a **STEPLIB that is not
APF-authorized** -- the property the whole transport exists for, and
now asserted by the job itself rather than assumed:

- `STATUS` round-trips: 304 bytes, 5 rows, ending `200 status`.
- A reply buffer under `REPMIN` is refused without the SVC being
  issued at all.
- A request carrying `OWNER=` is still answered, with the keyword
  substituted.
- An unknown request comes back as `500`, so the transport carries a
  refusal as reliably as an answer.
- **Paging works and every row crosses exactly once**: `STATUS` into
  a 256-byte buffer came back as **5 rows in 2 pieces**. This had to
  be forced -- `LIST` on a fresh spool returns 0 rows, so the first
  green run left the resume logic, the most intricate part of piece
  two, completely untouched. A test that pages only when the system
  happens to hold data is not a test of paging.
- **Negative control after the stop**: the same job now fails at
  "subsystem OFT1 is installed". The SSCT really is unchained at
  stop, and discovery really does go through it.

### What this run does not show

- **TK5.** MVS/CE only, including the `00CCC8` invalid-SVC entry point.
  Since run in a TK5 container, `TSTSVC` 34/34 with TK5's `017068`
  (section 13); `TSTPROT` (#15) was not run there.
- **The granularity**, as above.
- **An abend with slots in flight.** The sweep never ran, so its state
  table is still unexercised: no slot was ever CLAIMED or COPYOUT when
  a sweep looked. That needs a deliberately faulting caller.
- **Two userids.** `OWNER=` substitution was exercised but nothing
  reads the resulting userid back, so "a second user sees only its
  own transfers" is still untested (`LIST OWNER=` from two ids).
  Section 13 measures two owners with one caller, and in the TK5
  container two RAKF userids.
- The deploy put OFTPD and OFTPSVC in `SYS2.LINKLIB`, which is in the
  linklist, so the modules had linklist reach. Harmless in a throwaway
  and **not a recipe for a stand**, where they belong in the product's
  own library.

## 13. The utilities over the SVC, and who may do what (2026-10-18)

mvslovers/oftpd#22. `OFTP` and the dialog use the transport; OFTPD
decides what the caller may do. Measured in two throwaway MVS/CE
containers on mvsdev, one after the other, then in a TK5 container
(the batch utility and `TSTSVC`, not the dialog), and last on the TK5
stand m38b as a test instance.

### What changed

- **The client** (`oftp#cli.c`): no host means the SVC. A long answer
  is asked for again with the resume index until a piece ends `OK`; a
  piece ending `MORE` carries rows only, because `put_slot()` drops the
  final line once the buffer is full. `BUSY` is RC 4, `DOWN` RC 12. What
  a piece of lines means is decided once, for both transports, in the
  portable `oftp#rep.c` (`oftp_cli_rc()`, `oftp_cli_piece()`), which is
  what the host test `TSTREP` checks.
- **A request over 255 bytes is refused, not cut** (`oftp#svcc.c`). The
  client used to shorten it to `OFTP_SVC_REQMAX` while `OFTP` builds
  requests of up to 300: what was left would have been another request,
  and OFTPD would have answered that one. `TSTSVC` step 2b checks it;
  run in the TK5 container (below), not in the MVS/CE ones -- they ran
  `OFTP`, not `TSTSVC`, and no command there was long enough to reach
  the limit.
- **The userid goes beside the line, not in it.** Piece two appended
  `OWNER=<slot userid>` to every request. That had two consequences
  nobody had chosen:
  - `LIST` was always filtered, so `MINE` changed nothing, and nobody --
    an operator included -- saw anyone else's transfers;
  - `SHOW`, `DELETE`, `HOLD`, `RELEASE` and `RETRY` checked no owner at
    all.

  Now `svc_line()` only drops `OWNER=`, and the slot's userid, `svc` and
  `admin` travel in the `CTLOUT` the handlers already get.
- **The rule** (`ctl_may()`): an administrator -- a userid in `ADMIN=`
  -- sees and changes every transfer, and has `CALL` and `REFRESH`;
  anyone else only the transfers they queued (`506 not your transfer`,
  `507 needs ADMIN= in OFTPPRM`, both RC 8). `STATUS` and `PARTNERS` are
  open. An empty userid owns nothing, so a received file, which has no
  owner, is an administrator's. `LIST MINE` sends `MINE=Y`, which the SVC
  needs, and `OWNER=` as well, which only the control port reads.
- **The TCP port is unchanged**: it believes `OWNER=`, treats it as
  `MINE` in `LIST` as a 0.1.0 client expects (read off the code, not
  run: the measured `LIST PORT(3306)` carried no `OWNER=`), and may do
  everything. It
  is the development port without security, and it goes off by default
  in 0.2.0.
- The console names who changed a transfer: `OFTP231I … DELETED BY
  IBMUSER`, `OFTP232I … HOLD BY …`; over TCP still `BY CONTROL PORT`.

### Measured

OFTPD from `SYS2.LINKLIB` (APF, `OFTP004I AUTHORIZED BY LIBRARY`),
`SVC=237`, `OFTP039I CROSS MEMORY ACTIVE ON SVC 237`. `OFTP` from
`IBMUSER.OFTP.LOAD`, which is not in `IEAAPF00` (read: `SYS1.VTAMLIB`,
`SYS1.INDMAC`, `SYS2.LINKLIB`, `SYSGEN.NJE38.AUTHLIB`, `SYSC.LINKLIB`).
One batch step per command, so each has its own condition code. The
second identity is a transfer queued over TCP with `OWNER=OTHER`; no
second RAKF userid was used, so "another userid" here is another
*owner*, and the caller on the SVC is always IBMUSER.

`ADMIN=NOBODY1`, IBMUSER an ordinary user (JOB00024):

| Command | CC | Answer |
|---|---|---|
| `SEND … VFN(MINE.FILE) HOLD` | 0 | `T0000002 queued and held` |
| `SEND … HOLD OWNER(O…` (cut at column 72) | 0 | `T0000003`, owner IBMUSER |
| `LIST` | 0 | T0000002, T0000003; not OTHER's T0000001 |
| `LIST MINE` | 0 | the same two |
| `SHOW T0000001` | 8 | `not your transfer` |
| `DELETE T0000001` | 8 | `not your transfer` |
| `CALL PEER` | 8 | `needs ADMIN= in OFTPPRM` |
| `PARTNERS` | 0 | 1 partner |
| `DELETE T0000002` | 0 | deleted; console `OFTP231I T0000002 DELETED BY IBMUSER` |
| `LIST PORT(3306)` | 0 | over TCP: T0000001 and T0000003, everything |

Restarted with `ADMIN=IBMUSER` (JOB00025):

| Command | CC | Answer |
|---|---|---|
| `LIST` | 0 | T0000001 (OTHER's) and T0000003 |
| `LIST MINE` | 0 | T0000003 only |
| `SHOW T0000001` | 0 | `owner OTHER` |
| `SEND … HOLD -` / `OWNER(OTHER)` (continued, whole) | 0 | `T0000004`, `SHOW`: owner IBMUSER |
| `DELETE T0000001` | 0 | deleted; console `OFTP231I T0000001 DELETED BY IBMUSER` |
| `CALL PEER` | 0 | `call of PEER requested`, `OFTP206I CALLING PEER` |

**Paging through the client** (JOB00026): 42 transfers, `LIST` and `LIST
MINE` each 42 rows and about 3.4 KB through a 2048-byte reply buffer --
so at least two pieces -- with no row twice. The number of pieces is not
visible from outside; that each crosses exactly once is `TSTSVC` step 5b.

**Negative control** after `P OFTPD` (JOB00030): `LIST` CC 12,
`OFTP010E OFTPD not reached: not running, or without SVC=`.

**The dialog**, second container, with `ADMIN=IBMUSER` -- which is why
it sees OTHER's transfer at all: started from the More menu as
`CMD(OFTPI) NEWAPPL(OFTP)`, panels in `SYSGEN.ISPF.PLIB`, the module in
`SYSGEN.REVIEW.LOAD` (`SYSGEN.ISPF.LLIB` was full, D37). From a fresh
profile, so with no host: the menu says `OFTPD reached by cross memory
(SVC)`, the monitor shows `STATUS`, a held send is queued
(`T0000002 queued and held`, owner IBMUSER read back over TCP), the
transfer list shows OTHER's transfer and the new one (`2 transfers`), and
`Mine only Y` shows the new one alone (`1 transfers`).

It needs a screen of 43 rows, as `docs/installation.md` says: on 24 the
main menu's `DISPLAY` ends RC 20 with no message, `main`'s panel as much
as this one. `mbt test --tso` sizes the screen from `[tso.ispf]`, so a
script under another name gets 24 rows; `MBT_TARGET_TN3270_SIZE=80x43`
gives it 43.

### TK5, in a container

The same branch (`d04f23f`) in a throwaway container of the `tk5` image
(TK5 Update 5, minimal configuration; a local build on mvsdev, not yet
published), and not on a TK5 stand.

**Authorization and the slot.** OFTPD and OFTPSVC from
`HERC01.OFTPD.LOAD`, which is not in `IEAAPF00` -- as an SMP install on
TK5 would be, where no product library is -- so `OFTP004I AUTHORIZED BY
SVC` (RAKF's SVC 244). SVC 235-239, read by a BREXX `STORAGE()` job
(`CVT` → `CVTABEND` → `SCVTSVCT`), as OFTPD's messages say:

| When | `SVC 237` | OFTPD said |
|---|---|---|
| before the start (JOB00040) | `00017068 80000000` | `OFTP031I … FOUND 00017068 80000000` |
| transport active (JOB00048) | `00AD8608 C0000000` | `OFTP030I … MODULE=00AD8608`, then `OFTP039I` |
| after `P OFTPD` (job number not recorded) | `00017068 80000000` | `OFTP032I … RESTORED, ENTRY READS 00017068 80000000` |

235, 236, 238 and 239 read `00017068 80000000` throughout: `017068` is
TK5's invalid-SVC routine, as on m38b and m38r (section 3).

**Two RAKF userids.** `ADMIN=HERC01`; HERC02 is not in `ADMIN=`. (On
TK5 HERC02 is in RAKF's group ADMIN all the same, as HERC01 is; that
decides data set access, not OFTPD's rights. Corrected 2026-10-18: this
line used to call HERC02 an ordinary TSO user.)
`OFTP` from `HERC01.OFTP.LOAD`, not in `IEAAPF00`.

A trap first: **mvsMF rewrites the job card's `USER=` to the userid
that submitted the job** -- `USER=HERC02` came back as `NOTIFY=$MVSMF,
USER=HERC01,PASSWORD= BY MVSMF`, and OFTPD rightly took the job for
HERC01's (JOB00050, discarded). To run as HERC02 the job has to be
submitted through mvsMF *as* HERC02 (JOB00051).

HERC01, the administrator (JOB00049):

| Command | CC | Answer |
|---|---|---|
| `SEND … VFN(ADMIN.FILE) HOLD` | 0 | `T0000001 queued and held` |
| `SEND … HOLD -` / `OWNER(OTHER)` | 0 | `T0000002`, owner HERC01: `LIST MINE` below lists it |
| `LIST`, `LIST MINE` | 0 | both, 2 transfers |
| `PARTNERS` | 0 | 1 partner |

HERC02, not in `ADMIN=` (JOB00051); T0000003 belongs to HERC01:

| Command | CC | Answer |
|---|---|---|
| `SEND … VFN(USER.FILE) HOLD` | 0 | `T0000004`, owner HERC02 |
| `LIST`, `LIST MINE` | 0 | T0000004 only, `1 transfers` |
| `SHOW T0000003` | 8 | `not your transfer` |
| `DELETE T0000003` | 8 | `not your transfer` |
| `RELEASE T0000003` | 8 | `not your transfer` |
| `SHOW T0000004` | 0 | `owner HERC02` |
| `RELEASE T0000004` | 0 | queued; console `OFTP232I T0000004 RELEASE BY HERC02` |
| `CALL PEER` | 8 | `needs ADMIN= in OFTPPRM` |
| `PARTNERS` | 0 | 1 partner |

HERC01 again (JOB00053): `LIST` shows HERC02's T0000004 beside its own,
`SHOW T0000004` says `owner HERC02`, and `DELETE T0000004` deletes it,
console `OFTP231I T0000004 DELETED BY HERC01`. Every step CC 0.

**Paging** (JOB00054): 40 further sends and one `LIST`: 42 rows, 3318
bytes as printed and more on the wire (each row has its `100 ` there),
through the 2048-byte buffer, every number from T0000005 to
T0000044 there once and in order, and no sweep message on the console.

**`TSTSVC`** (`tstsvc.jcl`, from the `TESTLIB`, not authorized, JOB00057):
**34/34**, step 2b included. Step 2b proves the *client's* refusal: a
request over `REQMAX` gets `BADREQ` from `oftp_svc_call()` without
the SVC being issued, so the routine's own length check was not
exercised by any test at the time of this run. It is since (#55):
`TSTSVC` step 2c issues the SVC directly with declared lengths the
routine must refuse (`test/mvs/tstsvc.c`); measured on MVS/CE only.

**Negative control** after `P OFTPD` (JOB00059): `LIST` CC 12,
`OFTP010E OFTPD not reached: not running, or without SVC=`.

**Issue #12 on TK5 too.** With the procedure's `SYSOUT=*`, TK5 prints and
purges the STC output, and the `$HASP150` line was the only sign:
14,524 lines. With `SYSOUT=H` (STC00043, idle, started and stopped):
a `SYSUDUMP` of 14,046 lines, `S30A` in a subtask running `CTHREAD`,
PSW `075C2000 00F1BB0C`, step `COND CODE 0000`, nothing on the console.
That is the dump of mvslovers/oftpd#12 on MVS/CE, so not new and not
TK5's.

**`SEND` and the caller's authority** stayed out of reach here: HERC02
may read `HERC01.OFTP.TESTA` itself (IEBGENER as HERC02, JOB00052, CC 0),
as RAKF group ADMIN and through TK5's `DATASET * READ`, so sending it
shows nothing. Measured since with HERC03 (section 14).

### TK5 stand m38b

The first run of the transport on a stand, 2026-10-18 about 22:01-22:06
MVS time, in a slot from the MBT session (`internals/m38-changes.md`).
A test instance beside the stand's own OFTPD, which belongs to the test
network and was left running and untouched. The instance had names of its
own: PROC `OFTPDT`, `SYS2.PARMLIB(OFTPPRMT)` and `(OFTPPTNT)`, all absent
before; the libraries `HERC01.OFTPDT.LOAD` and `HERC01.OFTPT.LOAD`, not
in `IEAAPF00`; spool `HERC01.OFTPDT.SPOOL`; `PORT=3337 CONTROLPORT=0
SVC=237 ADMIN=HERC01`.

| When | `SVC 237` | Read by | OFTPD said |
|---|---|---|---|
| before | `00017068 80000000` | JOB02714 | `OFTP031I … FOUND 00017068 80000000` |
| active | `00B5B608 C0000000` | JOB02720 | `OFTP030I … MODULE=00B5B608`, `OFTP039I` |
| after `P OFTPDT` | `00017068 80000000` | JOB02725 | `OFTP032I … RESTORED, ENTRY READS 00017068 80000000` |

The reader also counts the most frequent entry point of 200-255:
`00017068` 37 of 56 before and after, 36 while 237 was taken. The 19
numbers in use are the ones listed for m38b in section 3; 235, 236, 238
and 239 stayed `00017068 80000000`.

STC00109: `OFTP004I AUTHORIZED BY SVC`. Client job JOB02721 (HERC01,
one step per command, all CC 0):
- `SEND … HOLD` gave T0000001;
- `SEND … HOLD OWNER(OTHER)` gave T0000002, and `SHOW` names its owner as
  HERC01;
- `LIST` and `LIST MINE` each listed 2 transfers;
- `PARTNERS` answered;
- `DELETE T0000002` worked, and `LIST` then listed 1.

`TSTSVC` JOB02724 passed **20/20**. That is fewer checks than the
container's 34 because the spool held one transfer, so `LIST` paged in
fewer pieces. The forced paging of `STATUS` into 256 bytes is among
the 20. The negative control JOB02726 ended `LIST` CC 12, `OFTP010E`. The
#12 dump was there too: `SYSUDUMP` 14,060 lines, `S30A`, PSW `075C2000
00F1BB0C`.

Removed afterwards (OFTPDEL JOB02727, IDCAMS CC 0): the three members,
the cluster, the test data and both libraries; the job outputs were
purged. `D A,L` showed the stand's OFTPD only. The clean stop frees
module, slot pool and anchor, and no `CSA RETAINED` message appeared,
so no CSA should be left; that was not checked with a CSA map. Left
on the stand: `HERC01.OFTPD.V0R1M0D.TESTLIB` from `mbt test --mvs`.

### What this does not show

- **The dialog on TK5**, in the container or on a stand.
- **Two RAKF userids on a stand**: m38b ran HERC01 only. The container
  covered HERC02.
- **A caller without an ACEE**, which owns nothing by this rule.
- ~~**`SEND` reads the data set with OFTPD's authority, not the
  caller's.**~~ Checked at queue time since section 14: over the SVC,
  OFTPD asks RACHECK whether the caller may read the data set.

## 14. A send needs the caller's READ (2026-10-18)

`design.md` 7.1 as written: OFTPD reads a data set it sends with its own
authority, so a send over the SVC -- where the userid is real -- is
checked when it is queued (`src/mvs/oftp#sec.c`):

- `racf_login(user, NULL)`: RACINIT `ENVIR=CREATE,PASSCHK=NO`, which
  OFTPD may issue because it is authorized;
- `racf_auth(acee, "DATASET", dsn, READ)` on the data set (a member's
  library), RC 0 or 4 allowed, as OPEN itself would proceed;
- `racf_logout()`.

Every caller is checked, an `ADMIN=` userid as well: `ADMIN=` is about
transfers, not data sets. The answer is `509 not authorized: <user> may
not read <dsn>` (RC 8) and `OFTP238W` on the console, and it comes before
the existence check, so a data set the caller may not read answers the
same whether it exists or not. A caller without a userid is refused on a
system with a security product; without one (`CVTRAC` 0, or RACINIT
answering 32) there is nothing to check. The TCP port is unchanged.

Measured in a throwaway TK5 container (`tk5:target`), OFTPD authorized
by SVC 244, with a profile of the container's own in
`SYS1.SECURE.CNTL(PROFILES)` reloaded by `S RAKF` (`RAKFPROF7`):
`DATASET HERC01.OFTPSEC.*` NONE, group ADMIN ALTER. HERC03 is not in
ADMIN; HERC01 and HERC02 are.

| Who | What | Result |
|---|---|---|
| HERC03, IEBGENER (the control) | read `HERC01.OFTPSEC.DATA` | S913-38, `RAKF0005 INVALID ATTEMPT` (JOB00048) |
| HERC03, `OFTP SEND` | `HERC01.OFTPSEC.DATA` | RC 8, `not authorized: HERC03 may not read HERC01.OFTPSEC.DATA`; `OFTP238W` (JOB00049) |
| HERC03, `OFTP SEND` | `HERC01.OFTPSEC.NOSUCH` | RC 8, the same answer, not `data set not found` |
| HERC03, `LIST` | | `0 transfers` |
| HERC01, `OFTP SEND` | `HERC01.OFTPSEC.DATA` | queued (JOB00047) |
| HERC01 and HERC02, `OFTP SEND` | `HERC01.OFTP.TESTA` | queued (JOB00042, 00043) |
| HERC02, `OFTP SEND` | `HERC01.NOSUCH.DATA` | `data set not found` |

The check writes nothing to the console: RACINIT and the RACHECK
(`LOG=NONE`) left no RAKF message for OFTPD's checks, only for the
IEBGENER control.

Not shown: a caller without an ACEE, and a system without a security
product -- both are read off the code.


## 15. The failure cases, measured (2026-10-19, #34)

Throwaway MVS/CE containers, the build of #33 (`293706f`) and, for the
cases that need a parked caller, a test build with `-DOFTP_DEBUG_SLOW`:
a request carrying `SLOW=n` holds the slot thread n seconds before it
is served, and a stop cuts the wait short and leaves that request and
every one behind it unserved (`src/mvs/oftp#mon.c`, never shipped).
Five initiators for class A (`$SI4-6`), so that five clients run at
once.

**Instruments, both independent of OFTPD.** The SVC table entry is read
by Hercules' `r` over its web console: CVT at X'10', SCVT at CVT+X'C8',
SVC table at SCVT+X'84', SVC 237 at +237*8. The anchor and its slots
are read with Hercules' `v` at the address `OFTP030I` gives: `inflight`
+X'1C', `full` +X'24', `served` +X'28', slot i's state at
+X'40'+i*X'940'. The image carries no BREXX.

**1. A second `S OFTPD`.** A plain second start waits at allocation for
the spool (`IEF863I`, `IEF099I WAITING FOR DATA SETS`), before any OFTPD
code runs. One with its own spool and port (`M=OFTPPRM1,S=...`) and
also `SVC=237` says `OFTP033E SVC 237 NOT FREE (EP 00A6F5C8 LOCK 00,
UNUSED EP 0000CCC8), NO CROSS MEMORY` and runs on without the
transport. Requests went to the first, and its `P` left the entry
alone: no `OFTP032I`, the table still held the first one's routine.
The first's `P` restored it; a fresh `S`/`P` took it and gave it back
again.

**2. `P OFTPD` with clients parked.** One `LIST SLOW(120)` and two
plain `LIST`s behind it, three slots PENDING. `P OFTPD`: all three jobs
ended within the second, CC 12, `OFTP010E OFTPD not reached: not
running, or without SVC=`; the stop took two seconds, and SVC 237 read
back free. The text is not quite right for them: their request had
arrived, and the routine cannot tell its caller "stopped" from "not
there" because both are `OFTP_SVC_DOWN`.

**3. A client cancelled while parked.** `C OFTPB` while its `LIST` was
PENDING behind a `SLOW(30)`: S222. Its slot was answered when the slot
thread reached it and went to DONE, because the owner's address space
is the initiator's and outlives the job; `inflight` stayed at 1, the
cancelled caller's. 300 seconds after the answer `OFTP061W CROSS MEMORY
FREED SLOTS X'02' UNCOLLECTED FOR 300 SECONDS`, and `inflight` went to
0. **So each batch caller cancelled while parked takes one of the four
slots for five minutes**; four of them within five minutes leave the
pool full. A cancelled TSO user, whose address space goes away, is the
liveness check's case and is not measured.

**4. A full pool.** `LIST SLOW(40)` and three `LIST`s: four slots
PENDING, `inflight` 4. A fifth `LIST` got CC 4, `OFTPD busy, try again`
(`OFTP_SVC_BUSY`; not `421`, which #1's plan said), and `full` went to
1. When the slow one ended, the other three each got one complete
answer, `served` 4, every slot FREE, `inflight` 0.

**5. A caller faulting inside the routine.** It cannot be provoked from
a caller any more: `TPRNG` (section 6) turns a pointer the caller may
not store into into a return code. `TSTSVC` step 7 measures that: a
reply buffer in low core, eight times (twice the pool), each refused
with `OFTP_SVC_BADREQ` and nothing written, then `STATUS` answered and
`inflight` 0 -- 24/24. The window left, a second task freeing the
storage between the `TPROT` and the `MVCK`, was not provoked. The sweep
was exercised instead by writing the states a fault would leave into
the anchor with Hercules' `v ...=`:

| Written | Swept | `inflight` after |
|---|---|---|
| slot 3 CLAIMED, `inflight` 1 | `OFTP060W ... SLOTS X'08' ...` within 5 s | 0 |
| slot 2 COPYOUT, `inflight` 1 | `OFTP060W ... SLOTS X'04' ...` within 4 s | 0 |
| slot 1 CLAIMED, `inflight` left at 0 | `OFTP060W ... SLOTS X'02' ...` | 0, not X'FFFFFFFF' |

A CLAIMED slot with `inflight` 1 written just before `P OFTPD`, so that
the sweep had no time: `OFTP032I SVC 237 RESTORED` first, ten seconds
later `OFTP037W CROSS MEMORY DRAIN TIMED OUT, 1 IN FLIGHT, CSA
RETAINED`, `OFTP003I`. The next `S OFTPD` built a new anchor at another
address, took the entry, served a `LIST` and gave the entry back at its
stop; the old anchor and module stay in CSA until the IPL, as section 6
says.

Not covered: TK5, a cancelled TSO user, and the race between `TPROT`
and `MVCK`.

### The stop's answers, corrected (same day)

Case 2 left one thing wrong, and looking at it found a second. The
routine tested `ACTIVE` before the slot state when it woke. A stop
clears `ACTIVE` first and joins the slot thread after, so:

- a slot the thread never took is posted STOP, and its caller was told
  `OFTP_SVC_DOWN`, "not reached", although its request had arrived;
- a request the thread **was serving** was carried out and answered
  DONE, and its caller, waking to `ACTIVE` off, was told "not reached"
  as well.

The routine now reads the state first. DONE delivers the answer
whatever `ACTIVE` says (the anchor stays while the caller is in
flight); STOP returns the new `OFTP_SVC_STOPPED` (20), which the client
prints as `OFTPD stopped before carrying out the request`, RC 12;
another state with `ACTIVE` off is still `OFTP_SVC_DOWN` (`WAKEDOWN`).
`OFTPSVC` grows from 1144 to 1152 bytes. The debug build gained
`LATE=n`: hold a request n seconds whatever happens, then serve it.

Measured in a throwaway MVS/CE container, the anchor read with
Hercules' `v`:

| Run | Routine | Result |
|---|---|---|
| `LIST SLOW(60)` + two `LIST`s, three slots PENDING, `P OFTPD` | new | all three CC 12, `OFTPD stopped before carrying out the request`; entry restored |
| `SEND ... HOLD LATE(15)` + a `LIST` behind it, `P OFTPD` during the hold | new | anchor `flags 00000000` while both were still in flight; then `OFTP230I T0000001 QUEUED`, the `SEND` job CC 0 with `T0000001 queued and held`, the `LIST` CC 0 showing it |
| the same `SEND ... LATE(15)`, `P OFTPD` during the hold | **from `main`**, OFTPD unchanged | `OFTP230I T0000002 QUEUED` on the console, and the job CC 12 `OFTPD not reached: not running, or without SVC=` |
| `TSTSVC` | new | 24/24; SVC 237 restored at the stop |

The third row is the control: the same OFTPD with only the routine
from `main` reproduces the discarded answer, a `SEND` queued while its
job ended RC 12.


## 16. A cancelled batch caller, told apart by its job (2026-10-19, #36)

Section 15 case 3 found a batch caller cancelled while parked holding
its slot for the sweep's 300 s: its address space is the initiator's
and outlives the job, so the ASCB alone says "alive". #36 measured what
common storage offers instead -- no field identifies a job uniquely,
`ASCBJBNI` (ASCB+X'AC') points at `CHKEY` of the job's own CSCB while a
job runs and is 0 between jobs, and a job of the same name resubmitted
into the same initiator gets the same CSCB, name and step -- and the
maintainer chose to use what there is (option 1 there).

**What changed.** `OFTPSVC` records at claim `ASCBJBNI` (`SLJBNI`,
+X'38') and 16 bytes from the CSCB: `CHKEY` and `CHSTEP`, 56 bytes on
(`SLJOB`, +X'40'). The slot grew to 2384 bytes, the request text and
the reply moved by 16, the routine's request-header EQUs are relative
to `SLREQ` now, and the anchor is version 2 (`OFTP_ANC_VER`; the
request block stays 1). `caller_gone()` in `oftp#svc.c` compares: no
ASCB or another at the ASID, `ASCBJBNI` 0 or another CSCB, another
jobname or step -- gone. The CSCB is read only while `ASCBJBNI` still
points at it. It decides in three places:

- **take**: a gone caller's request is not carried out; the slot is
  freed, `OFTP066W ... THE OWNER OF SLOT n IS GONE, REQUEST NOT CARRIED
  OUT`;
- **answer**: as before, `OFTP064W ... ANSWER DISCARDED`;
- **sweep**: a DONE slot whose caller is gone is freed at once,
  `OFTP067W CROSS MEMORY FREED SLOTS X'nn', THEIR CALLER IS GONE`; the
  300 s stay for what `caller_gone()` cannot see.

**And a second fault, found by measuring the first.** The ECB of a
cancelled caller's slot still names that caller's SVRB, and a job
resubmitted into the same initiator can get an SVRB at the same
address. OFTPD's POST of the dead caller's ECB then ended the live
caller's wait while its own slot was still PENDING; the routine called
that a protocol error, and the job ended RC 8 although its own request
was carried out (`OFTP230I T0000002 QUEUED`, then `OFTP064W` for its
slot). The routine now waits again until its own ECB is posted. The
first version of that re-issued the WAIT on an ECB still carrying its
wait bit, and every try ended S301 (five of five); the ECB goes back to
0 by `CS` against the value it holds first, so a POST landing in
between is seen.

**Measured** in a throwaway MVS/CE container with the `SLOW=n` debug
build, slot fields read with Hercules' `v`:

| Run | Result |
|---|---|
| `OFTPB` (`SEND`) parked behind `LIST SLOW(30)`, `C OFTPB`, initiator idle after | slot showed `jbni 00FE1740 job OFTPB S01`; `ASCBJBNI` 0 after the cancel; `OFTP066W`, the `SEND` not carried out (`0 transfers`), slot free, `inflight` 0 |
| as above, a new `OFTPB` submitted 2 s after the cancel, landing on another CSCB (`00FE1690` -> `00FE17C0`) | old: `OFTP066W`, not carried out; new: CC 0, its own `SEND` queued |
| the same, landing on the same CSCB (five tries, `R6`-`R10`) | old: carried out (`R*.OLD QUEUED`) -- the case #36 said would not be told apart; new: CC 0 with its own answer every time, despite the POST meant for the old slot; old slot freed by `OFTP067W` one second after the new job ended; `inflight` 0 |
| the same before the re-wait (`R1`-`R5` with the first version, and the run before it) | first version: S301 five of five; without it: the new job RC 8, its `SEND` queued and its answer discarded |
| `TSTSVC` | 31/31 (more `LIST` pieces: 23 transfers in the spool) |
| `LIST SLOW(60)` + two `LIST`s, `P OFTPD` | all three CC 12, `OFTPD stopped before carrying out the request`; entry restored |

**Left open then:** a cancelled job's request was still carried out
when a job of the same name got the same CSCB in the same initiator
before the slot was served. Closed the same day, below.

### The JES job id and the claim sequence (option 2)

Option 2 in #36 named the caller's TCB. It was not used: a TCB address
is as reusable as the CSCB's, which is what the CSCB measurement
showed, and nothing about a TCB at the same address says it is another
task's. What is unique per job is its JES job id, `SSIBJBID` (TCB+X'B4'
-> JSCB, +X'13C' -> SSIB, +X'0C'), private storage the routine can read
because it runs in the caller's address space. It records it in the
slot (`SLJOBID`, +X'50') and numbers the claim from a 24-bit sequence
in the anchor (`ANCCLAIM`, +X'34'; `SLCLAIM`, +X'3C'). One address
space runs one job at a time, so `caller_gone()` reads **a later claim
from the same ASCB under another job id** as the end of the earlier
claim's job.

Nothing writes into another caller's slot. A routine that marked other
slots "gone" could hit one that was freed and claimed again in
between, and that caller would never be served; reading instead, the
worst a slot caught mid-claim can do is fail to say "gone". The
routine clears the fields, stores the id, takes the number with `CS`
and stores it last; OFTPD reads the number first and skips 0, which is
a claim being made. Slot 2400 bytes, anchor version 3, `OFTPSVC` 1280.

| Run | Result |
|---|---|
| cancel and resubmit `OFTPB` into the same initiator, five tries (`Q1`-`Q5`) and a sixth with the slots read during it | old and new slot: same ASID `0004`, same CSCB `00FE0118`, `OFTPB S01` -- only the job id (`JOB 39`, `JOB 40`) and the claim (17, 18) differ; every time `OFTP066W` for the old slot and no `Q*.OLD` queued, the new job CC 0 with its own `SEND`; `inflight` 0 after |
| a job whose two steps each `SEND`, then `LIST` | CC 0, both queued: one job id across its steps |
| `TSTSVC` | 26/26 |
| `LIST SLOW(30)` + `LIST`, `P OFTPD` | both CC 12, `OFTPD stopped before carrying out the request` |

**Left open:** the request of a caller cancelled before any later claim
from its address space, while a job of the same name holds the same
CSCB, is still carried out -- the next claim is what reveals it. TK5
and a cancelled TSO user are not measured.
