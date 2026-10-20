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
  old rules when the new member has an error. The sample procedure names
  the member in a mandatory `OFTPRCV` DD: copy `OFTPRCV0` to the parmlib
  before using it. `%VFN` and `%Q1`..`%Q9` in a pattern carry partner-chosen
  text into the name, so start patterns with a fixed qualifier.
- Project skeleton: the monitor `OFTPD` and the utility `OFTP` as stubs.
