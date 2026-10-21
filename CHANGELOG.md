# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - not yet released

The first final release. OFTPD is an implementation of OFTP, the ODETTE File
Transfer Protocol, for MVS 3.8j: a monitor that runs as a started task, the
`OFTP` command for TSO and batch, and an ISPF dialog. It speaks OFTP 2.0
with its security features off, and OFTP 1.4 and 1.3, over plain TCP/IP.
There is no TLS and no certificate handling. It is meant for hobbyist and
test systems, not for production use.

It replaces the prerelease 0.1.0-dev. The SMP function `TOFT100` deletes
`TOFT010`, so it installs over the prerelease; see "Upgrading" in
`docs/installation.md`.

### Added

- **Cross-memory transport.** The `OFTP` command and the ISPF dialog reach
  the monitor through an SVC of its own, so the monitor knows which job or
  TSO user is asking. `SVC=` in `OFTPPRM0` names the number (200 to 255,
  no default; OFTPD's own is 237). The monitor takes the SVC at start and
  gives it back at stop and in its recovery exit. The routine is the new
  load module `OFTPSVC`. The monitor must run authorized for this (from an
  APF-authorized library, or through SVC 244 where the security product
  permits it); without that OFTPD still starts and carries files, but the
  utilities and the dialog find no monitor (`OFTP005W`, `OFTP035W`).
- **Owners and administrators.** A transfer belongs to the userid that
  queued it. That userid sees and changes it. `ADMIN=` in `OFTPPRM0` names
  the userids that see and change every transfer, including received ones
  (which belong to nobody), and that may `CALL` a partner and `PURGE`.
  Anyone else gets `not your transfer`.
- **`SEND` checks the caller's authority.** The monitor asks the security
  product (RAKF) whether the userid that queued the transfer may read the
  data set, and refuses with `not authorized` when it may not, an
  administrator included.
- **Receive rules.** The new parmlib member `OFTPRCV0` (DD `OFTPRCV`) says,
  per partner and virtual file name, which data set a received file is
  written to: a name built from the virtual file name, the partner and the
  arrival time (`DSN=` with `%VFN`, `%Q1` to `%Q9`, `%ID`, `%YYYY` and more),
  `REPLACE=Y` to replace an existing sequential data set once the new file
  is complete and closed, and unit, volume, space and translation. The first
  matching rule decides; a member with an error is not used and the old
  rules stay. `F OFTPD,REFRESH` reads it again.
- **Partner maintenance at run time:** `OFTP PARTNER ADD|CHANGE|ENABLE|
  DISABLE|DELETE|SHOW|LIST` from TSO and batch, and the same in the ISPF
  dialog (option 3), for the userids named in `ADMIN=`. No parmlib edit and
  no restart; the partners are kept in the spool and survive a restart. A
  partner of the same name there wins over `OFTPPTN`, and `REFRESH` reads
  `OFTPPTN` only. A partner that has transfers in the spool is never
  deleted: it is disabled and the delete is refused. The spool holds the
  partner passwords in clear text, like `OFTPPTN`: protect it the same
  way. OFTPD does not start when it cannot read the partner records
  (`OFTP019E`). See `docs/partners.md`.
- **Operator log.** OFTPD writes its messages to a SYSOUT data set,
  `DD OFTPLOG` of the started task, each line with its date and time. A new
  message class `P` goes to the log only: every single transfer, session
  start and end, and the start-up detail. The console receives only what an
  operator must act on. Every session message starts with the partner name.
  Without the DD, or if a write fails, everything goes to the console and
  OFTPD says so once (`OFTP022W`, `OFTP023E`). `docs/messages.md` lists the
  messages with their destination.
- **`F OFTPD,TRACE ON|OFF [partner]`** logs the protocol commands of the
  sessions, running ones included, one `OFTP290P` line each; never a
  password and never file data.
- **Several sessions per partner.** `SESSIONS=n` (1 to 16, default 1) in the
  partner entry lets up to n sessions with a partner run at once. A call
  beyond the limit is refused with ESID 08, and a refused call is made again
  after a pause of 5 to 60 seconds that differs from one system to the
  next. Two systems that called each other at the same moment used to be
  refused by each other and retry in step.
  Each session claims the file or the EERP it sends, so two sessions with
  one partner never send the same one.
- **Spool housekeeping.** Finished transfers (delivered, confirmed, rejected,
  failed) are deleted from the spool once a day after `KEEPDAYS` days
  (default 7, `0` keeps them for ever); `OFTP PURGE [DAYS(n)]` and
  `F OFTPD,PURGE [n]` do it on request. Only spool entries go, never data
  sets.
- **`HOLD` and `RELEASE` by partner:** `HOLD|RELEASE PARTNER(name|*)
  [VFN(prefix*)]` from the utility and `F OFTPD,HOLD|RELEASE name|ALL` from
  the console.
- **`SEND ... DISP(DELETE)`** deletes the data set once the partner's EERP
  (end-to-end receipt) has arrived, never earlier; a refusal (NERP) keeps it.
  It needs ALTER on the data set and takes a whole data set, not a member.
- **`SEND ... STRTTIME(hh:mm) STRTDATE(*|*+n|ddd) SENDWIN(hh:mm-hh:mm)`**
  set the earliest start and a daily window in which a send may start,
  also across midnight.
- **Documentation:** `docs/installation.md`, `docs/messages.md` (the
  messages, their class and where they go) and `docs/partners.md`.

### Changed

- The messages that moved to the log only changed their class letter (for
  example `OFTP211I` is now `OFTP211P`) and, where the key came first,
  now begin with the partner name. A message number is never given to
  another message.
- A file sent by OFTPD now states its size, taken from the volume's table of
  contents. A sender that states no size gets 15 tracks of primary and 15 of
  secondary space on the receiving side, unless a receive rule gives `SPACE`.
- Passwords are kept out of more places: the echo of `SYSIN` masks every way
  `PASSWORD` can be written, an error text names the position of a bad word
  instead of the word, and the request and reply slots in common storage are
  erased as soon as they have been read.
- The build toolchain is libc370 2.24.1 and cc370 1.6.1.

### Removed

- The TCP control port and its keyword `CONTROLPORT=`. It had no security:
  whoever could reach the port could queue and list transfers. OFTPD reports
  the keyword as unknown (`OFTP010W`); add `SVC=` instead.

### Fixed

- A file of more than about 750 KB between two OFTPDs ended with `EFNA`
  reason 12, because the file description carried no usable size.
- A spool that an abend or a `CANCEL` had left open could not be opened
  again. OFTPD now runs `VERIFY` itself and says so (`OFTP021W`).
- A session that abended stayed counted as running: its partner was refused,
  its spool entry stayed active and `P OFTPD` never ended. The session is
  now cleaned up like one that ended (`OFTP209E`, `OFTP203P`), and its record
  buffer is freed.
- Every session subtask kept about 69 KB until the next restart; each is
  freed once it has ended.
- Every end of OFTPD wrote a system dump (`S30A` in VSAM's close), because
  the spool was closed by another task than the one that opened it.
- An abend in the spool thread left every later spool request waiting for
  good. The request now gets an error (`OFTP219E`) and the thread goes on.
- `P OFTPD` gave up waiting for a running session after 60 seconds. It now
  waits for the sessions and says `OFTP096W` every minute; a session that
  hangs is ended by `CANCEL`.
- A batch job cancelled while its request was parked kept its slot for 300
  seconds. It is recognised by its job now and its slot is freed.
- An answer given while OFTPD was stopping was thrown away. It is delivered,
  and a request that was never served is told so.

## [0.1.0-dev] - 2026-10-13

The first prerelease, installed through SMP as `TOFT010`.

- The monitor `OFTPD` (a started task), the `OFTP` command for TSO and batch
  (`SEND`, `LIST`, `SHOW`, `DELETE`, `HOLD`, `RELEASE`, `RETRY`, `CALL`,
  `PARTNERS`) and an ISPF dialog.
- OFTP 2.0 with its security features off, and OFTP 1.4 and 1.3. A file a
  partner wants signed or encrypted is refused.
- Sequential data sets and PDS members of fixed, variable and undefined
  record format, and text translated between EBCDIC and ASCII with CR-LF
  line ends.
- A VSAM spool that keeps every transfer, an EERP when the receiving side
  confirms, retries with growing waits up to a limit.
- Routing through intermediate nodes (`VIA=`, `RELAY=Y`), with receipts
  coming back the same way.
- Operator commands on the started task: list partners, the spool and the
  sessions, call a partner now, read the partner table again.
