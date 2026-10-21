# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Receive rules: an optional parmlib member (`OFTPRCV0`, DD `OFTPRCV`) decides
  where an incoming file goes -- the data set name from patterns with the
  partner, the file name parts and the arrival time, an existing data set
  refused or replaced (`REPLACE=Y`, the old data set is deleted only after
  the new file has been received completely), and unit, volume, space and
  translation per rule. The first matching rule wins; without a rule files are
  stored as before. `F OFTPD,REFRESH` reads the member again and keeps the
  old rules when the new member has an error. OFTPD does not need the
  `OFTPRCV` DD (without it there are no rules), but the sample procedure
  names the member in it: copy `OFTPRCV0` to the parmlib before using the
  procedure, or delete the DD. `%VFN`, `%Q1`..`%Q9` and `%CTR` carry partner-chosen
  text into the name, so a pattern with one of them in its first qualifier
  is refused when the member is read, as is a pattern that can name a data
  set under `TMPPREFIX`. `REPLACE=Y` replaces a plain sequential data set
  only; a PDS or VSAM name is refused and left alone. If a stop between the
  two renames of a replace leaves the old data set only under its backup
  name, the next start renames it back.
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
- A job after receipt: a receive rule with `JOB=pds(member)` makes OFTPD
  read that member once the file is under its final name, put `?PFILE`
  (the data set), `?VFILE` (the virtual file name), `?ID` (the partner) and
  `?KEY` (the transfer) into it and give it to the internal reader of the
  started task; the job runs as the default user of the security product
  (RAKF: `PROD`), not as the started task, unless the member has a `USER=`
  of its own. The EERP does not wait for the job. The transfer record
  (`OFTP SHOW`) says whether the job was submitted, with its JES job id, or
  why not; a line that grows past column 71 (a JCL statement) or 80 after
  the substitution, a member that is missing or empty, or a virtual file
  name with a character JCL must not get from a partner leaves the job
  unsubmitted and the file received (`OFTP247W`). So does a member that
  has a different number of records the second time it is read. The
  reason comes first in `OFTP246P` / `OFTP247W`, so the console and log
  cut cannot remove it.
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
