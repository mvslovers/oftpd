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
  deleted: it is disabled and the delete is refused. See
  `docs/partners.md`.
- Project skeleton: the monitor `OFTPD` and the utility `OFTP` as stubs.
