# oftpd — design

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
- `OFTP SEND` and `OFTP LIST` (then over a TCP control port without
  security; over OFTPD's SVC since 2026-10-18, the port removed on
  2026-10-19, section 7).
- `F OFTPD,D ...`, `F OFTPD,CALL`, `P OFTPD`.

**Partners behind a partner** (*decided* 2026-10-12, stage 1 of routing):
a partner with `VIA=name` is reached through the direct partner `name`
-- the session goes there, `SFIDDEST` carries the final recipient, its
EERP comes back the same way and is matched by `EERPORIG` too. A file for
an Odette ID that is not ours is refused (SFNA 02) -- unless OFTPD is a
node.

**OFTPD as a node** (stage 2, `RELAY=Y`, 2026-10-12): a file for the
Odette ID of one of our partners (direct or `VIA`) is taken, stored
unchanged (a T file as its byte stream, format U, never translated) under
`TMPPREFIX.Tnnnnnnn`, answered with EFPA and **no EERP**, and queued as a
send to the next hop with its identity intact (name, date, time and the
original `SFIDORIG`). The final recipient's EERP or NERP is matched to
that send and queued as a response record for the partner the file came
from, which `next_erp` sends like any EERP; the stored copy is then
deleted. A relayed file refused for good by the next hop makes OFTPD
create the NERP itself (creator: our Odette ID). A file whose route would
lead back to the partner it came from, or with no route, is refused with
SFNA 02. A relayed file that reaches `MAXRETRY` is given up the same
way: NERP to the partner it came from (its last SFNA reason, else 99),
the stored copy deleted (3.3). Not yet: forwarding an EERP that matches
no relayed send, loop detection beyond the one hop back.

**Not in the MVP** (all planned): restart, buffer compression, NERP
generation beyond a relay that gives up (received NERPs are recorded),
post-processing after receipt, configurable naming of received data sets,
partner maintenance by command or ISPF, virtual local Odette IDs, routing
beyond one hop, cross-memory transport.

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
         OFTP ...       ISPF dialog              (OFTP 1.3 / 1.4 / 2.0)
               \          /                               |
          SVC 237 (cross memory)                    port 3305 (TCP)
                  \    /                                  |
   +-----------------------------------------------------------------+
   | OFTPD (started task, APF, AC(1))                                |
   |                                                                 |
   |  main task ---- CIB polling (MODIFY / STOP), startup, shutdown  |
   |  slot thread ------> request handlers (SVC callers)             |
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
port, the SVC number, data set names of spool and partner table, `MAXSESS`,
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
- **Disposition** (sends): `KEEP` the source (the default), `DELETE` once
  the EERP has arrived, never earlier (built 2026-10-18: `DISP=DELETE`,
  over the SVC only, never for a member, the caller checked for ALTER
  when it is queued; a NERP keeps the source), and *later* `COPY` (a
  snapshot taken when the send is queued, so the source may change
  afterwards). rvs also renamed the source when queueing a DELETE; OFTPD
  does not, so a source changed before the EERP is deleted as it is.
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
   |       (retry later)           FAILED        (local error)
   |
 HELD (operator or user)
```

- `SENT` means EFPA received: the partner has the file, the end-to-end
  answer is outstanding. `DELIVERED` is the EERP. The two are never
  folded into one state.
- `WAITING` is the **system's** hold: after a broken connection, SFNA
  retry `Y`, or a partner that was not reached, with a next-attempt time
  that grows with the attempt count (`RETRY` times the attempts, at most
  a day). It clears itself. `HELD` is the **operator's or user's** hold and
  only a person releases it.
- **Retry limit** (2026-10-13, after rvsMVS `MAXRETRY`/`TRANSHLD=YES`):
  after `MAXRETRY` attempts (OFTPPRM, default 10, 0 = no limit) our own
  file goes to `HELD` -- not `FAILED`, so a person decides, and `RELEASE`
  or `RETRY` start the count again. A relayed file is given up instead:
  `REJECTED`, a NERP to the partner it came from, the copy deleted; a node
  has no one to release it, and the originator has to learn it (a level
  1.3 originator cannot: the engine drops the NERP). An attempt is a file
  opened in a session (as rvs counts) **and** a call that did not reach
  the partner -- unlike rvs, whose dial retries are counted apart, so that
  a dead next hop ends in a NERP too. A partner not reached, or a call that
  ended in an error before any file (a wrong password, say: not counted),
  is not called again for `RETRY` seconds (CALL overrides); a partner
  that answered ESID 08 after a short pause instead (section 6.1). A file for a partner we do
  not call (no `HOST`) waits for the partner's call and is never counted.
  A stop of the monitor during a transfer does not count. Later, from
  rvs: per-send `MAXRETRY`, a deadline (`ENDDATE`/`ENDTIME`) and a job
  when either fires (`ERRJOB`, with post-processing). An abend during a transfer leaves it `HELD`,
  not `WAITING`, so a failure that repeats does not loop.
- **When a send may start** (2026-10-18, after rvs `STRTDATE`, `STRTTIME`,
  `SENDWIN`): `tstart` (not before) and `winfrom`/`winto` (a daily
  window) in the record; `send_due()` honours both, for the first attempt
  and every retry. The rules are portable (`src/oftp#sch.c`, `TSTSCH`).
  libc370's `mktime()` reads a `struct tm` as UTC (libc370#392), so the
  start is computed as now plus a difference of two `mktime()` results.
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

### 3.3.1 Housekeeping

From rvsMVS's cleanup job, which deletes finished entries after a number
of days and leaves pending ones alone (2026-10-18). A transfer is
**finished** when nothing more happens to it by itself: `DELIVERED`,
`CONFIRMED`, `REJECTED`, `FAILED`. `HELD`, `WAITING`, `SENT` and
`RECEIVED` are not, whatever their age. Its age is its **last change**,
the latest of the queue, attempt and end times, because not every way to
an end sets the end time.

- `KEEPDAYS=n` in OFTPPRM (default 7, 0 = for ever): the scheduler deletes
  finished transfers older than that once a day, the first time at its
  first tick after the start.
- `PURGE [DAYS=n]` (utility, administrators only) and `F OFTPD,PURGE [n]`
  do it now; `DAYS=0` takes every finished transfer.
- The scan and every delete are **one request** of the spool thread
  (`oftp_spl_purge()`), so a `RETRY` of a finished send cannot slip in
  between the look and the delete. The rule itself is portable
  (`src/oftp#prg.c`) and host-tested.
- Only spool records go; data sets, received or sent, are never touched.

Measured (2026-10-20, #59, a throwaway MVS/CE container, `KEEPDAYS=7`
by default, `test/mvs/tstkeep.c` and `.jcl`): the daily run, not
`PURGE`, deletes what is old and finished. The age is produced by
writing it: with OFTPD stopped, `TSTKEEP` (PARM='SEED') adds nine
transfers to the spool with `tqueued`, `tlast` and `tdone` set 1 to 60
days back; then OFTPD is started and nothing else is done. Four finished
ones older than 7 days (CONFIRMED 30, DELIVERED 10, REJECTED 20, FAILED
60 days) and five that must stay (CONFIRMED 1 day, DELIVERED 6 days, and
HELD, SENT, QUEUED at 30 days) were seeded. The first tick, ten seconds
after `OFTP001I OFTPD READY`, logged `OFTP235I 4 FINISHED TRANSFERS OLDER
THAN 7 DAYS DELETED BY KEEPDAYS`, and `TSTKEEP` (PARM='CHECK') then found
the four gone and the five there (10/10); the same check before the start
failed on exactly the four (6/10). A first start on an empty spool logs
nothing: the daily run is silent when it deletes none. Not observed: the
second run 24 hours later, whose only part not shared with the first is
`now - tpurge >= 86400`.

Later, from rvs: a purge by partner, name or time range, and a backup of
the spool (a REPRO job in samplib).

### 3.4 Concurrency

*Proposed*: **one spool server thread owns both VSAM handles**. Session and
control workers send it requests (read, write, update, delete, next
transfer for partner X) through a queue and wait on an ECB. Reason: a
libc370 `VSFILE` carries one ACB and one RPL, so concurrent requests from
several threads through one handle are unsafe whatever the SHAREOPTIONS
say; and whether an ACB opened by one TCB can be used from another has not
been measured. A single owner sidesteps both and makes every spool change
a sequence that is easy to log.

Measured on MVS (CLAUDE.md, `test/mvs/tstksds.c`): inserts need libc370
2.10.1, whose `vswrite()` on a dynamic handle is a direct PUT; existence
is tested with a POINT (`vssteq()`), which reports "not found" as reason
16; a never-loaded cluster gets its control record by a sequential load.

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
two oftpd systems through `SFIDUSER` or the 2.0 description, *later*.

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

### 6.1 Sessions per partner, and two partners calling each other

*Decided* 2026-10-18 (#18). RFC 5024 limits neither the sessions to one
partner nor says what happens when two stations call each other at the
same moment. rvsMVS has a limit per station (`TCPSESS`, 1 to 99, default
1) under a global one (`TCPMAX`), and a call beyond the station's limit
ends with ESID 08 (DF9429E). OFTPD follows that:

- **`SESSIONS=n` per partner, default 1**, under `MAXSESS`. A call from a
  partner at its limit ends with **ESID 08**, "resources not available,
  retry later", not ESID 03: the caller is known, it is only busy.
  `MAXSESS` reached keeps the silent close (`OFTP204W`): an ESID needs
  the SSRM/SSID exchange, and that costs a session subtask exactly when
  the server has none to spare.
- **Work is distributed.** While more files for a partner wait than
  our calls still starting up will take, the scheduler opens another
  session to it, up to `SESSIONS`. EERPs and NERPs alone open one only
  when no session with the partner runs, since a running one sends
  them at its next turn. Each session claims what it sends:
  - **a file** by reading its record again under the monitor lock and
    writing it back `ACTIVE` while it is still due; the data set is
    opened after the lock. A session that ends before the file is done
    gives it back as before (`WAITING`, or an attempt that failed).
  - **an EERP or NERP** in memory, in its session slot, after reading
    the record again under the lock; the record stays `RECEIVED` until
    the EERP is sent. The claim goes with the session's slot, so an
    end or an abend gives it back, and it does not outlive the run. A
    spool state would: `ACTIVE` on a receive record means a partial
    receive, which the next start discards.
  - A partner not reached while another session with it runs leaves
    the files alone (that session takes them) and only waits `RETRY`
    before the next call.
- **Two partners calling each other.** Both mark the other busy before
  the call, so each refuses the other's call. Measured between dev1 and
  m38b on 2026-10-18 with ESID 03 and `RETRY` for both: the two waited
  the same time from the same second and collided again until timing
  jitter let one call through, which is how an EERP stayed held at m38b
  on 2026-10-13 (`internals/testnet.md`, Results). Now a call answered
  with ESID 08 before any file is made again after a pause of 5 to 60
  seconds taken from the low bits of the TOD clock (`OFTP218I`), and the
  scheduler wakes at the second it is due instead of the next 10-second
  tick, so the two sides drift apart. With `SESSIONS=1` that is what
  resolves a collision; with more, both calls simply go through. An
  ESID 03 keeps `RETRY`: it can be a real configuration error, and a
  partner on an older level answering 03 for busy is resolved by jitter,
  as before.
- **`MODE=S|R|B`** is the RFC's SSIDSR and rvs `DIRECTN`; EERPs and
  NERPs go both ways whatever the mode, as in rvs. rvs `DIRECTN=P` ("pay
  only for send data": send only when calling, both when called) exists
  because the caller paid for the line, which TCP/IP does not do: not
  adopted. Neither are a `RETRY` per partner, call windows (`DIALOPTS`)
  or a switch against calling for an EERP alone (`EERPAD`; rvs itself
  recommends calling).

Measured for the first step (ESID 08 and the pause, `SESSIONS=1` only)
on 2026-10-18 in two MVS/CE containers (`mvsce-target`,
image `mvsce-builder@sha256:8b8269051cea`), each running OFTPD
`bb952b3` (on `main` as `a58a3c8`, the same `src/` and `include/`) with the other as its only partner, calling each other over
the Docker bridge. Seven trials, each a held file on both sides, both
`CALL`ed at the same moment, the files released 3 s later:

| Trials | What happened | Both files delivered, EERP back |
|---|---|---|
| 1, 2 | half collision: one side refused the other with ESID 08 and paused (21, 19 s); its own call went through | after 6 s |
| 3-7 | full collision: both refused with ESID 08, pauses 56/34, 30/46, 50/42, 40/14, 26/48 s | after 17-46 s |

In every full collision the side with the shorter pause called at the
second it was due, and that one session carried both files and both
EERPs (`FILES 1 OUT 1 IN`); the other side never had to call.

`SESSIONS` and the claims, measured the same day in two fresh
containers (image `mvsce-builder@sha256:caf9a92f1210`), OFTPD
`1367fca` (on `main` as `4f9d639`, the same tree; built before the
commit, so the banner says `54c6538-dirty`), `SESSIONS=2` on both
sides. Each file was queued held and released through the control
port, one release after another:

| Trial | Work | Sessions | Each file sent / received / EERP |
|---|---|---|---|
| 3 | 20 files A to B | A called B twice, 12 and 8 files, the two running together from 16:20:25 to 16:20:29 | once each (20 `OFTP211I`, 20 `OFTP222I`, 20 `OFTP213I`, 20 different keys) |
| 4 | 12 files each way at the same moment | B called A twice; A's call was the third and got ESID 08 (`OFTP202W ... 2 SESSIONS RUNNING`); B's two sessions carried all 24 (`FILES 8 OUT 6 IN`, `4 OUT 6 IN`) | once each |

No `OFTP214W` (an EERP for a file not known) and no file refused as
one already received. Then `SESSIONS` taken out on both sides (default 1),
`F OFTPD,REFRESH`, and the collision trial again: two half collisions,
delivered after 6 and 5 s, and one full collision with pauses 39 and
15 s, delivered after 20 s by one session (`FILES 1 OUT 1 IN`), as
before.

Two things the first runs showed, fixed before the trials above: the
scheduler cleared its wake ECB before waiting, so a `RELEASE` that came
while it was connecting waited for the next 10-second tick (five
files released in the second of the call all went in that one session);
and a side that had received files opened a second session for the
EERPs while the first still ran, which ended with nothing to send. Not covered by
a trial there: a session that ends, or abends, while it holds a claim.
The EERP claim has been measured since (below); a send claim has not.

A session cut while it holds an EERP claim, measured on 2026-10-20 (#57)
in a throwaway MVS/CE container, OFTPD `90f6008` (the same `src/` as
`main` at `9e90051`), with `host/oftph -X` as the partner: it drops the
line when the first EERP arrives, before the RTR. The claim is
`m->erpheld[slot]`, in the monitor's memory only; the spool record stays
RECEIVED until the RTR has come.

| Step | What happened |
|---|---|
| 1. partner sends a file, cuts at the EERP | `OFTP222I` once; `SEND EERP`, then `connection lost`, session ended with ESID 99; record `R R` (received, EERP owed); `D S`: no session |
| 2. the monitor's next call to the partner (30 s, RETRY) | `SEND EERP`, `RECV RTR`, `OFTP203I` ESID 00; record `R C` (confirmed) |
| 3. another session of the partner, then a call by the monitor | no EERP, no file; the monitor did not call within 75 s |
| 4. two files in one session, cut at the first EERP, then cut again at the same EERP in the next session, then the monitor's call | records `R R`, `R R` after both cuts; the first EERP offered again in session two (the partner had it once already, no RTR: at-least-once); the monitor's call sends the two EERPs once each, each with its RTR, both `R C` |

The partner's data sets: `HELLO`, `HELLO2`, `HELLO3` once each under the
input prefix; `OFTP222I` once per file over the whole run. An EERP that
was sent and whose RTR was lost is sent again; that is the protocol's,
not a double delivery of the file. Not measured: an abend of the session
(the cleanup that frees the claim is the same one) and a cut between
claiming the record and sending the EERP. The engine side is checked on
the host by `TSTSESS` (`cut_scenario`): `erp_sent` does not run without
the RTR.

## 7. How the utilities reach OFTPD

*Decided* (2026-10-12): **the target is cross-memory** (an SVC or subsystem
interface, like ufsd's), because only there does OFTPD see the caller's
real identity. During development the transport was a TCP control port
with no security, the cheaper start, which let a PC drive the monitor in
tests. Both carried the same requests and answers, so the request layer
does not depend on the transport: the handlers write lines through an
output function, and keep no per-caller state beyond the request.

*Decided* (2026-10-14): the cross-memory transport is **a private SVC**
that OFTPD installs at start, not the subsystem interface, because an
SSI caller must be authorized and the utilities are not. The design is
in `internals/xmem.md`.

*Decided* (2026-10-19, #32): **the control port is removed**, code and
documentation. Its sockets were the Hercules host's (DYN75), so any
process on the PC running Hercules could queue, list and change every
transfer, and a `SEND` over it read the data set with OFTPD's authority.
Another transport -- an HTTP API, say -- can be added beside the SVC
through the same output function, and it has to bring a real identity
with it.

Proposed protocol: line-based text, one request per line, numbered
responses in the FTP manner (`2xx` done, `4xx` try later, `5xx` refused),
multi-line output for lists. Verbs mirror the utility: `SEND`, `LIST`,
`SHOW`, `HOLD`, `RELEASE`, `DELETE`, `RETRY`, `CALL`, `PARTNER
ADD|CHANGE|DELETE|LIST`, `STATUS`.

### 7.1 Who is asking

OFTPD acts on the requester's behalf: it reads the data set to be sent and
lets the requester see and change their transfers. Over the SVC the
caller's ACEE is real: the routine reads the userid, and the client
cannot say one (`internals/xmem.md` section 13). The control port could
not tell who was calling and believed the userid the client sent, which
is why it is gone (7, above).

**Built** (2026-10-18, `src/mvs/oftp#sec.c`, measured in
`internals/xmem.md` section 14). **The check itself**: OFTPD builds an ACEE
for the requester's userid without a password (RACINIT, possible because
OFTPD is APF) and checks READ on the data set to be sent — and for
post-processing, on the job library. Under RAKF a missing profile means
"allowed", so the check alone does not protect; the installation must
define the profiles, and the documentation says so. A user exit may
replace the check *later*; whatever is shipped **refuses by default**.

**With OFTPD down, nothing can be queued.** That is intended: OFTPD is
the only writer of the spool. If it bites, a drop data set that OFTPD
reads at startup can be added later.

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

`HOLD`/`RELEASE` of a partner (2026-10-18, after rvsMVS `H-ALL`, `S-ALL`,
`S-sid[,name]`): every send to it -- `ALL`, or `PARTNER=*` from the
utility, for every partner -- goes from `QUEUED` or `WAITING` to `HELD`,
or from `HELD` back to `QUEUED` with its attempts reset; a name or a
`prefix*` narrows it (utility only). From the utility it takes only what
the caller may change (an administrator: everything). Each change is a
conditional put (`oftp_spl_putif()`): a send a session claimed in the
meantime keeps its new state. The single-key `HOLD`, `RELEASE` and
`RETRY` use the same put now and answer `450` instead of overwriting.

`TRACE` (#48): the session engine asks a switch before every command it
sends or receives (`OFTP_SESS.traced`), so the operator's change reaches
sessions already running. The monitor keeps the state in its block -- one
flag for all sessions, one per partner, same index as the partner table --
and the line goes out as `OFTP290P partner: TRACE SEND|RECV CMD len: fields`.
The fields are decoded for SSID (level, ID, buffer, mode, credit; the
password is never formatted), SFID, SFPA, SFNA/EFNA/ESID, EFID, EFPA and
EERP/NERP; DATA buffers are counted and reported as one line per run.
The responder learns the partner from the SSID, so with a trace for one
partner its SSRM is not traced and its SSID is logged once the partner is
known. `TRACE OFF partner` while all sessions are traced is refused.

Stopping a session or OFTPD comes in three strengths: **graceful** (at the
next file boundary), **cancel** (now; the transfer goes to `WAITING`) and
**break** (now, and the transfer goes to `HELD`).

**Messages**: `OFTPnnnnS`, S one of `I W E` and `P` for messages that go
to the log only and never to the console. Number ranges per component
(monitor, session, spool, requests, utility). Every session message
starts with the partner name, so a log of several sessions can be read.
The log is a SYSOUT data set; the console sees only what an operator must
act on.

How it is built (#47, `src/oftp#log.c`, `src/mvs/oftp#mon.c`):

- **Routing is one function** (`oftp_log_route()`), host-tested with a
  fake data set and console (`test/tstlog.c`); the MVS side only supplies
  `rwrite()` and `wto()`. The class is read from the message text itself,
  the letter after the three digits, so a call site cannot disagree with
  the text. `P` goes to the log, the rest to both.
- **The log is `DD OFTPLOG`**, opened with `ropen()` as RECFM=F (the
  record is padded; a SYSOUT data set with a variable format is refused
  with `OFTP022W`). The record is `YYYY-MM-DD HH:MM:SS ` and the
  message; a message is at most 123 characters (a WTO line), so the
  record is 143. The cut is explicit: `vsnprintf()` into a 124 byte
  buffer, and a line that did not fit ends in `>`; the partner name is
  first, so it is never what is cut. A record length below 143 cuts the
  same way. Nothing is wrapped: one message, one record.
- **No writable static.** The `OFTP_LOG` state is in the `OFTP_MON` block
  and found through `grtapp2` (FTPD's way); the GRT is the process's, so
  every session subtask sees it. Before `oftp_mon_logopen()` and after
  `oftp_mon_logclose()` the slot is empty and a message goes to the
  console.
- **Serialized with `lock()`** on the log block: sessions are subtasks,
  and two `rwrite()` calls on one RFILE must not overlap. The ESTAE
  (`OFTP098E`) uses `trylock()`: the abend may have come inside a log
  write of the same task, where waiting for the lock would wait for
  itself. When the lock is not free the line goes to the console.
- **Failure is loud, once.** No DD, or an open that fails: `OFTP022W`,
  everything on the console, class `P` too, so nothing is lost. A write or
  close that fails: `OFTP023E`, once, and the log is given up for the rest
  of the run. Open, every write and the close are checked.
- **Out of scope**: a log viewer, switching to a new data set, size
  limits.
- **Class changes keep the number and change the letter** (`OFTP211I` is
  now `OFTP211P`); a number is never reused for something else.
  `OFTP215`, `OFTP216` and `OFTP233` exist with two letters because the
  two messages of one number differ in who needs to see them. The list is
  `docs/messages.md`.

## 9. Phases

1. Codec and session engine on the host (tests against the RFC examples),
   then OFTPD with spool, partner table, inbound and outbound sessions,
   a TCP control port (removed 2026-10-19) and `OFTP`; interop with
   mendelson OFTP2 CE at level 5.
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
- ~~Q7~~ Who is asking: *decided* — cross-memory through OFTPD's SVC,
  the control port removed (section 7).
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
