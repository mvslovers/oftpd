# oftp370 — design

Status: **draft for discussion**, 2026-10-12. Everything here is a proposal
unless marked *decided*. The wire format is not repeated here; it is in
`protocol.md`, and the RFC state tables are the state machine. This file
covers what the RFC leaves to the implementation. Open questions are
numbered **Qn** and collected at the end.

## 0. The MVP

*Decided* (2026-10-12): get a minimal product working first; everything
else is added later. Sections 1–10 describe the full picture so that the
MVP does not block it; what the MVP leaves out is marked *later* there.

**In the MVP:**

- OFTPD accepts calls on port 3305 and calls partners itself (on
  `F OFTPD,CALL partner` and when a send is queued).
- Level negotiation 5, 4, 2/1 (the engine is one; the layouts are cheap).
  `SSIDAUTH=N`, `SSIDCMPR=N`, `SSIDREST=N`, `SSIDSPEC=N`. Mode both.
- Files F, V, U, T in both directions; T translated CP037 <-> ISO 8859-1.
- Received files are stored **under the virtual file name**, with a
  configurable prefix (5.2).
- Unsigned EERP, sent and received; the spool tracks it.
- The spool KSDS (section 3), owned by OFTPD.
- Partners in a parmlib member read at startup (section 4).
- `OFTP SEND` and `OFTP LIST` over the TCP control port, no security.
- `F OFTPD,D ...`, `F OFTPD,CALL`, `P OFTPD`.

**Not in the MVP** (all planned): restart, buffer compression, NERP
generation (received NERPs are recorded), post-processing after receipt,
configurable naming of received data sets, partner maintenance by command
or ISPF, virtual local Odette IDs, routing, cross-memory transport.

**Order of work:**

1. Codec, host-tested: STB framing, every command encoded and decoded per
   level, DATA subrecords, translation tables. Byte-for-byte against the
   RFC examples.
2. Session engine over an abstract transport, host-tested by running two
   engines against each other in one process.
3. A host-side driver for the engine, so the engine meets mendelson OFTP2
   CE before it ever runs on MVS — the fast loop.
4. On MVS: KSDS smoke test, data set reader/writer per format, then OFTPD
   and `OFTP`.

## 1. Overview

```
            TSO / batch / ISPF                        partners
         OFTP ...      (later) dialogs           (OFTP 1.3 / 1.4 / 2.0)
               \          /                               |
          control port (loopback TCP)               port 3305 (TCP)
                  \    /                                  |
   +-----------------------------------------------------------------+
   | OFTPD (started task, APF, AC(1))                                |
   |                                                                 |
   |  main task ---- CIB polling (MODIFY / STOP), startup, shutdown  |
   |  control listener --> control workers                           |
   |  OFTP listener ----> session workers (inbound)  \               |
   |  scheduler --------> session workers (outbound) -+-> spool      |
   |                                                  |   server     |
   |                                                  +-> partner    |
   |                                                       table     |
   +-----------------------------------------------------------------+
            |                    |                     |
     spool KSDS          partner KSDS          user data sets
   (OFTPD only)          (OFTPD only)    (read/written under the
                                          requester's identity)
```

*Decided*: OFTPD is the only program that opens the spool and the partner
table. Everything else is a client of OFTPD.

## 2. The monitor (OFTPD)

ftpd is the template for everything that is not OFTP: startup order
(`__gtcom()`/`__cibset()` before any thread, APF check, RACINIT for the
task's own identity, configuration from `DD:OFTPPRM`, single-instance ENQ),
console handling by polling the CIB once a second, the `cthread` worker
pool, the non-blocking `accept()` plus `ecb_timed_wait` on listeners (the
DYN75 `selectex` timeout is unreliable there), and `select()` with a short
timeout on connected sockets so a worker sees a shutdown request.

Threads:

| Thread | Job |
|---|---|
| main | console commands, start/stop of the others, statistics |
| OFTP listener | accepts on port 3305, hands each connection to a session worker |
| scheduler | decides which partner to call: work queued, partner not busy, call window open, retry interval elapsed |
| session workers | one OFTP session each, inbound or outbound; pool of `MAXSESS` |
| control listener + workers | requests from the utilities (section 7) |
| spool server | the only thread that issues VSAM requests (section 3.4) |

Configuration (`DD:OFTPPRM`, `key=value` like ftpd): local Odette ID, OFTP
port, control port, data set names of spool and partner table, `MAXSESS`,
timers, default SDEB and credit, inbound data set naming defaults, trace
level.

## 3. The spool

### 3.1 Data set

A KSDS, `SHAREOPTIONS(1)`, opened only by OFTPD. Key: 8 bytes.

| Key | Record |
|---|---|
| `X'00'` + 7 | control record: next transfer number, format version |
| `T` + 7 digits | a transfer, in either direction |
| `E` + 7 digits | an end-to-end response (EERP or NERP) still to be sent |

At startup OFTPD reads the spool once and keeps a small in-memory index
(key, partner, direction, state; about 40 bytes per entry), so the
scheduler never scans the data set. An alternate index is not needed for
the volumes expected here.

### 3.2 Transfer record

Grouped by purpose; field widths are fixed in the header file, not here.

- **Identity**: transfer number; direction (send / receive); partner name;
  the virtual file's identity as OFTP defines it — `SFIDDSN`, `SFIDDATE`,
  `SFIDTIME`, `SFIDORIG` — plus `SFIDDEST` and `SFIDUSER`.
- **File**: format F/V/U/T, LRECL, size estimate; local data set name and
  member; translation (none, or a table name); for received files the
  attributes the data set was created with.
- **State**: state (3.3); attempt count; time queued, last attempt, last
  change, next attempt; owning userid (who queued it or for whom it was
  received).
- **Outcome**: the partner's OFTP reason code (SFNA/EFNA/ESID/NERP) and
  text **as received**, kept apart from our own local error code. Mixing
  the two in one field makes "who refused" unanswerable afterwards.
- **Disposition** (sends): `KEEP` the source (MVP), *later* `DELETE` once
  the EERP has arrived (never earlier) and `COPY` (a snapshot taken when
  the send is queued, so the source may change afterwards).
- **Restart**: last confirmed position (records for F/V, 1K blocks for
  U/T) and whatever is needed to resume the local data set at that point
  (3.5).
- **EERP**: for sends, whether an EERP or NERP has arrived and when; for
  receives, the key of the `E` record that answers it.
- **Post-processing** (*later*, but the field exists from the start): state
  and return code of the job or exit run after a receive.

Besides the key, OFTPD indexes every transfer **by the virtual file's
identity** (originator, name, date, time with its counter) in memory, for
duplicate detection (SFNA 13) and for matching an incoming EERP to its
send. The key itself stays a plain sequence number and never changes
during a transfer's life. (A key built from the file identity and
timestamps is a known source of collisions in the same second.)

### 3.3 States

Send:

```
QUEUED --> ACTIVE --> SENT ------> DELIVERED     (EERP received)
   ^  ^       |         |
   |  |       v         +--------> REJECTED      (NERP received)
   |  +---- WAITING                REJECTED      (SFNA/EFNA, retry N)
   |       (retry later)           FAILED        (retries exhausted)
   |
 HELD (operator or user)
```

- `SENT` means EFPA received: the partner has the file, the end-to-end
  answer is outstanding. `DELIVERED` is the EERP. The two are never
  folded into one state.
- `WAITING` is the **system's** hold: after a broken connection, SFNA
  retry `Y` or EFNA, with a next-attempt time that grows with the attempt
  count. It clears itself. `HELD` is the **operator's or user's** hold and
  only a person releases it. An abend during a transfer leaves it `HELD`,
  not `WAITING`, so a failure that repeats does not loop.
- An explicit `RETRY` verb resends a `SENT` file (lost EERP) — knowingly,
  because the partner then sees the same virtual file twice.

Receive:

```
ACTIVE --> RECEIVED --> CONFIRMED      (EERP sent)
   |           |
   v           +------> REJECTED       (NERP sent, level 4+)
INTERRUPTED
```

- `RECEIVED`: the data set is closed without error and renamed to its
  final name (5.2), EFPA sent, `E` record written. The EERP is never sent
  for a file that is not yet under its final name.
- `INTERRUPTED` receives that are not resumed within a configurable time
  are cleaned up (data set deleted, record marked).

### 3.4 Concurrency

*Proposed*: **one spool server thread owns both VSAM handles**. Session and
control workers send it requests (read, write, update, delete, next
transfer for partner X) through a queue and wait on an ECB. Reason: a
libc370 `VSFILE` carries one ACB and one RPL, so concurrent requests from
several threads through one handle are unsafe whatever the SHAREOPTIONS
say; and whether an ACB opened by one TCB can be used from another has not
been measured. A single owner sidesteps both and makes every spool change
a sequence that is easy to log.

### 3.5 Restart

*Later* — the MVP offers and accepts `SSIDREST=N`. OFTP restart is
negotiated per session (`SSIDREST`) and per file (`SFIDREST` / `SFPAACNT`).

- **As sender**: honour the partner's `SFPAACNT` by skipping that many
  records (F/V) or 1K blocks (U/T) from the start of the data set. Cheap.
- **As receiver, phase 1**: answer `SFPAACNT=0` — start over. That is
  spec-legal and keeps interop with peers that offer restart. The hard part
  is cutting a partly written BSAM data set back to the last confirmed
  position: BSAM has no truncate. Later options: `NOTE`/`POINT` to the
  checkpoint and rewrite from there; or receive into a work data set and
  copy the confirmed part on resume. **Q1**.

## 4. The partner table

*Decided* for the MVP: **a parmlib member** (`DD:OFTPPTN`), one entry per
partner, read at startup and on `F OFTPD,REFRESH`. An entry with an error
disables that partner with a message; it does not stop OFTPD. A refresh
never deletes a partner that still has transfers in the spool. *Later*: maintenance by
command and ISPF (rvsMVS had both a table and commands); whether that
needs a KSDS is decided then. Each entry has an 8-character partner name,
the name operators and users type.

Attributes:

- **Identity**: partner name; partner's Odette ID; our Odette ID towards
  this partner (default: the local one); the password we expect; the
  password we send; `SSIDUSER` both ways.
- **Connection**: host and port; may we call (active), may it call
  (passive); an **enabled** flag (a disabled partner is refused and never
  called, its queue kept); the addresses a call from it may come from
  (MVP: checked if set); a session cap; call windows and retry interval
  and limit (*later*).
- **Protocol**: highest level to offer (5, 4, 2); mode (sender, receiver,
  both); SDEB and credit caps; buffer compression Y/N; restart Y/N.
- **Files**: default code page translation for T (and optionally F/V);
  naming pattern for received data sets; the userid under whose authority
  received files are written and post-processing runs; allowed virtual
  file names (optional).
- **Defaults the send command can override** — *later*.

## 5. Virtual files and data sets

### 5.1 Sending

| Data set | Virtual file | Notes |
|---|---|---|
| RECFM F/FB | `F`, LRECL = LRECL | |
| RECFM V/VB | `V`, LRECL = LRECL − 4 | |
| RECFM U | `U` | blocks sent as a byte stream |
| any, with `FORMAT(T)` | `T` | each record translated EBCDIC→ASCII, trailing blanks stripped (option), CR-LF appended |

Default format comes from RECFM; the user can force `T` or `U`. Binary
data in F/V/U is sent as is unless the user names a translation.

### 5.2 Receiving

| Virtual file | Data set created | Notes |
|---|---|---|
| `F` | RECFM FB, LRECL from `SFIDLREC` | |
| `V` | RECFM VB, LRECL = `SFIDLREC` + 4 | |
| `T` | RECFM VB, LRECL 255 (configurable) | split at CR-LF, ASCII→EBCDIC; a line longer than LRECL fails the file (EFNA 05) rather than being cut or wrapped silently |
| `U` | RECFM U, BLKSIZE configurable (default 6144) | the stream is cut into full blocks, the last one short, so the byte count survives exactly |

**What survives and what does not.** OFTP carries only the format letter
and the maximum record length, never the original RECFM or BLKSIZE. F and
V therefore come back as the original records (FB/VB, blocked as we
choose); U is a byte stream on the wire, so the original U *block*
boundaries are lost and only the bytes are kept. Recreating the sender's
exact layout would need the sender to say what it was — possible between
two oftp370 systems through `SFIDUSER` or the 2.0 description, *later*.

**Written under a temporary name, renamed at the end.** A file being
received goes to `<temp prefix>.T<transfer number>`; only after the CLOSE
succeeded is it renamed to its final name, and only then is EFPA sent.
A half-received file therefore never carries a name anyone might pick up.

**Data set name** (MVP): the virtual file name as the protocol gives it
(`SFIDDSN`), under a configurable prefix: `OFTP.IN` + `PAYROLL.DATA` →
`OFTP.IN.PAYROLL.DATA`. If that is not a valid data set name, SFNA 01
(invalid filename, retry N); if the data set exists, SFNA 13 (duplicate
file, retry Y). This is built as the **default receive rule**: *later*,
rules keyed by partner (or any) and virtual file name (with wildcards)
choose the name pattern, replace or not, translation, space and the
post-processing job — the MVP's prefix + name is simply the rule that
matches everything.

V files with an LRECL above 32752 are refused with SFNA 05 (no spanned
records in the MVP). *Later*: a naming pattern per partner with symbols such
as `&PARTNER`, `&DATE`, `&TIME`, `&SEQ` (rvsMVS could be configured so).

Before EFPA: every write and the CLOSE must have succeeded (a full volume
is only reported at CLOSE). Otherwise EFNA.

### 5.3 After receiving

*Later*, but planned for: both a JCL member submitted with symbols
substituted and a user exit, per partner or virtual file (rvsMVS had
both). The spool record keeps the state needed so that adding it does not
change the record layout: a post-processing state and return code.

## 6. Sessions and memory

Per session: one receive buffer and one send buffer of SDEB bytes, one
record buffer of up to the largest LRECL, the session state. **SDEB cap
4096 by default** (configurable, 128 to 99999): the partner proposes,
OFTP takes the lower value. With 4096 a session needs about 8 KB of
buffers plus up to 32 KB of record buffer, plus thread stack. `MAXSESS`
defaults to 4.

The credit window does not cost memory on the receiving side: each buffer
is processed as it arrives.

Timers are not in the RFC for TCP; proposed: response timeout 60 s,
connection idle 300 s, both configurable.

## 7. The control port

*Decided* (2026-10-12): **the target is cross-memory** (an SVC or subsystem
interface, like ufsd's), because only there does OFTPD see the caller's
real identity. **During development the transport is TCP**, with no
security: it is the cheaper start and lets a PC drive the monitor in
tests. Both carry the same requests and answers; only the transport
underneath changes. So the request layer must not depend on TCP (no
reliance on stream properties, no per-connection state beyond the
request), and nothing may assume that a TCP caller is trustworthy.

Note: the sockets are the Hercules host's (DYN75), so "loopback" is the
PC running Hercules, not MVS. Any process on that PC can reach the port.
Acceptable for development, not for anything else.

Proposed protocol: line-based text, one request per line, numbered
responses in the FTP manner (`2xx` done, `4xx` try later, `5xx` refused),
multi-line output for lists. Verbs mirror the utility: `SEND`, `LIST`,
`SHOW`, `HOLD`, `RELEASE`, `DELETE`, `RETRY`, `CALL`, `PARTNER
ADD|CHANGE|DELETE|LIST`, `STATUS`.

### 7.1 Who is asking

OFTPD acts on the requester's behalf: it reads the data set to be sent and
lets the requester see and change their transfers. Over TCP, **OFTPD
cannot tell who is calling** — a connection carries no identity. That is
why cross-memory is the target (7, above): there the caller's ACEE is
real.

Until then the client sends its userid and OFTPD believes it. That is a
development shortcut, not a security model, and the TCP transport is not
to be offered as one.

**The check itself** is the same for both transports: OFTPD builds an ACEE
for the requester's userid without a password (RACINIT, possible because
OFTPD is APF) and checks READ on the data set to be sent — and for
post-processing, on the job library. Under RAKF a missing profile means
"allowed", so the check alone does not protect; the installation must
define the profiles, and the documentation says so. A user exit may
replace the check *later*; whatever is shipped **refuses by default**.

**With OFTPD down, nothing can be queued.** That is intended: OFTPD is
the only writer of the spool. If it bites, a drop data set that OFTPD
reads at startup can be added later. A password check over TCP (RACINIT) can be added if
the TCP transport ever has to live longer than planned.

### 7.2 Utilities

`OFTP` is one load module for TSO, batch TMP and `EXEC PGM=OFTP,PARM=`:

```
OFTP SEND DSN('MY.DATA') PARTNER(MVS2) VFN(PAYROLL) FORMAT(T)
OFTP LIST [PARTNER(MVS2)] [STATUS(QUEUED)]
OFTP SHOW 0000042
OFTP HOLD|RELEASE|DELETE|RETRY 0000042
OFTP CALL MVS2
OFTP PARTNER LIST|ADD|CHANGE|DELETE ...
```

Batch: several commands from `SYSIN`. Output through `*PUTLINE` in TSO,
`SYSPRINT` otherwise. Operands are parsed by our own keyword parser (there
is no IKJPARS in the ecosystem).

## 8. Operator interface

```
F OFTPD,D SESSIONS | PARTNERS | QUEUE [partner]
F OFTPD,CALL partner
F OFTPD,STOP partner          (end that partner's sessions gracefully)
F OFTPD,HOLD|RELEASE partner
F OFTPD,REFRESH               (configuration, partner table if parmlib)
F OFTPD,TRACE ON|OFF [partner]
P OFTPD                       (stop accepting, end sessions at the next
                               file boundary, close the spool)
```

Stopping a session or OFTPD comes in three strengths: **graceful** (at the
next file boundary), **cancel** (now; the transfer goes to `WAITING`) and
**break** (now, and the transfer goes to `HELD`).

**Messages**: `OFTPnnnnS`, S one of `I W E` and `P` for messages that go
to the log only and never to the console. Number ranges per component
(monitor, session, spool, control port, utility). Every session message
starts with the partner name, so a log of several sessions can be read.
The log is a SYSOUT data set; the console sees only what an operator must
act on.

## 9. Phases

1. Codec and session engine on the host (tests against the RFC examples),
   then OFTPD with spool, partner table, inbound and outbound sessions,
   control port and `OFTP`; interop with mendelson OFTP2 CE at level 5.
2. Buffer compression, receiver-side restart, post-processing, levels 4
   and 2 against a 1.x peer.
3. Cross-memory transport for the utilities (the target, section 7).
4. Maybe: routing through OFTPD as a hub, CMS compression, ISPF dialogs.

## 10. Open questions

- **Q1** (*later*) Receiver restart: NOTE/POINT and rewrite, or work data
  set and copy?
- ~~Q2~~ Partner table: parmlib member for the MVP; commands/ISPF later.
- **Q3** (*later*) Which partner attributes may a single send override?
- ~~Q4~~ Received `U` → RECFM U, `T` → VB 255, long line fails (5.2).
- ~~Q5~~ Received data sets under the virtual file name (5.2); patterns
  later.
- ~~Q6~~ Post-processing: JCL submit and exit, later (5.3).
- ~~Q7~~ Control port identity: *decided* — cross-memory is the target,
  TCP without security during development (section 7).
- **Q8** (*later*) Virtual local Odette IDs (several per installation);
  the MVP has one.

## 11. Lessons from a mature product

The design was checked against the documentation and libraries of
rvsMVS 6.2 (the maintainer's former product), studied for concepts only.
Adopted above: separate system and operator holds, `SENT` apart from
`DELIVERED`, raw partner reason codes, receive under a temporary name,
duplicate detection by file identity, receive rules with the MVP's naming
as the default, disposition after EERP, a log-only message class, three
stop strengths, a partner enabled flag.

Deliberately not adopted:

- **Utilities writing the spool directly**, with the monitor rescanning it.
  It allowed queuing while the monitor was down, and cost two decades of
  contention fixes on a shared KSDS. OFTPD stays the only writer.
- **A spool key built from name and timestamps**; uniqueness from
  timestamps was a recurring source of bugs.
- **Checks that fail open**: an exit shipped as accept-all.
- **Many transports** (X.25, SNA, BSC, XOT, tape, RJE), several security
  and compression layers, a keyword matrix for every attribute.
- **A journal that stops the monitor when full.**
