# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Partner maintenance at run time: `OFTP PARTNER ADD|CHANGE|ENABLE|DISABLE|
  DELETE|SHOW|LIST` from TSO and batch, and the same in the ISPF dialog
  (option 3), for the userids named in `ADMIN=`. No parmlib edit and no
  restart; the partners are kept in the spool and survive a restart. A
  partner of the same name there wins over `OFTPPTN`, and `REFRESH` reads
  `OFTPPTN` only. A partner that has transfers in the spool is never
  deleted: it is disabled and the delete is refused. The spool holds the
  partner passwords in clear text, like `OFTPPTN`: protect it the same
  way. OFTPD does not start when it cannot read the partner records
  (`OFTP019E`). See `docs/partners.md`.
- Project skeleton: the monitor `OFTPD` and the utility `OFTP` as stubs.
- OFTPD writes its messages to a SYSOUT data set, `DD OFTPLOG` of the
  started task, each line with its date and time. A new message class `P`
  goes to the log only: every single transfer, session start and end, and
  the start-up detail. The console receives only what an operator must act
  on. Every session message starts with the partner name. Without the DD,
  or if a write fails, everything goes to the console and OFTPD says so
  once (`OFTP022W`, `OFTP023E`). `docs/messages.md` lists every message with
  its destination.

### Changed

- Messages that moved to the log only changed their class letter (for
  example `OFTP211I` is now `OFTP211P`) and, where the key came first,
  now begin with the partner name.
