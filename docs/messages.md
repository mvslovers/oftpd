# OFTPD messages

Every message of the started task `OFTPD` has an id of the form
`OFTPnnnC`: `OFTP`, three digits, and a class letter.

| Class | Meaning | Goes to |
|---|---|---|
| `I` | information | console and log |
| `W` | warning | console and log |
| `E` | error | console and log |
| `P` | protocol of normal work | log only, never the console |

The log is `DD OFTPLOG` of the procedure (`docs/installation.md`, "The
log"). Each log line has the date and time in front; the console line does
not. A line is at most 123 characters; a longer one is cut and ends in `>`.

If the log is not available -- the DD is missing, it will not open, or a
write to it fails -- everything goes to the console, class `P` included,
and `OFTP022W` or `OFTP023E` says so once.

**What reaches the console** is what an operator must act on or answers an
operator command: start and stop, errors and warnings, a partner that is not
authorized, held or given up, the output of `F OFTPD,D ...`. A single
transfer (sent, received, delivered, refused with a retry to come) is
`P`.

**Session messages begin with the partner's name**, then a colon. A call
that has not been matched to a partner yet (`OFTP204W`, `OFTP205P`) begins
with the caller's address instead. Where a spool key is shown (eight
characters, `T0000012`) it follows the name.

**A number keeps its meaning for ever.** A message that changes its class
changes its letter and keeps its number: `OFTP211I`, as it was in a
development version, is `OFTP211P` now. A number is never given to another
message. `215`, `216` and `233` exist with two letters because two
different messages share the number; each is listed.

## Start, stop and the log

| Id | Dest | Text and meaning |
|---|---|---|
| `OFTP000I` | Console + log | `<version> STARTING` |
| `OFTP001I` | Console + log | `OFTPD READY`: the monitor accepts calls and runs the scheduler |
| `OFTP002I` | Console + log | `OFTPD STOPPING` (`P OFTPD`) |
| `OFTP003I` | Console + log | `OFTPD ENDED` |
| `OFTP004P` | Log only | `AUTHORIZED BY LIBRARY` or `BY SVC`: how OFTPD got authorization |
| `OFTP005W` | Console + log | `NOT AUTHORIZED, RC=n, CONTINUING` |
| `OFTP006W` | Console + log | `NO ABEND RECOVERY (ESTAE FAILED)`: an abend of OFTPD will not close its sockets |
| `OFTP010W` | Console + log | `<member>: <line>`: a line of `OFTPPRM` or `OFTPPTN` that was not understood; the entry is skipped or disabled |
| `OFTP010W` | Console + log | for example `OFTPPTN: line 12: a word is longer than 99 characters`: such a word is refused, never cut. In `OFTPPTN` the partner entry being read is disabled; `OFTPPRM` has no partner entry. `OFTP` stops an operand that long earlier, with `OFTP003E` |
| `OFTP010W` | Console + log | for example `OFTPPTN: line 7: word 4 is not KEYWORD=VALUE`: the 4th word of line 7 has no `=`. Words are cut at blanks, tabs and commas and counted from the first on the line, so in `PARTNER=PW ODETTEID=O1 PASSWORD=ab cd` the word `cd` is word 4. The word is never shown, because it may be the rest of a password. OFTPD skips that word and takes the others on the line; the partner entry stays enabled |
| `OFTP011E` | Console + log | `DD OFTPPRM CANNOT BE OPENED`; OFTPD ends |
| `OFTP012E` | Console + log | `OFTPPRM HAS NO ODETTEID`; OFTPD ends |
| `OFTP013W` | Console + log | `DD OFTPPTN CANNOT BE OPENED, NO PARTNERS` |
| `OFTP014P` | Log only | `ODETTE ID <id>, n OF m PARTNERS ENABLED` |
| `OFTP015W` | Console + log | `PARTNER <name> NOT ADDED, TABLE FULL` |
| `OFTP016I` | Console + log | `PARTNERS RELOADED: n ENABLED, n NEW, n DISABLED`: the answer to `F OFTPD,REFRESH` |
| `OFTP017E` | Console + log | `DD OFTPPTN CANNOT BE READ` (`F OFTPD,REFRESH`) |
| `OFTP018I` | Console + log | `n PARTNERS KEPT BY COMMAND, n OVERRIDE OFTPPTN`: the partners added or changed by `PARTNER` commands (`docs/partners.md`), read from the spool at start |
| `OFTP018W` | Console + log | a partner record of the spool was not taken (`HAS NO NAME`, `<reason>, IGNORED`, `IS KEPT BY COMMAND ALREADY`), two partners have one Odette ID (the second is `DISABLED`), or `THE PARTNER RECORDS OF THE SPOOL CANNOT BE READ` |
| `OFTP019E` | Console + log | `PARTNER RECORDS OF THE SPOOL UNREADABLE, OFTPD NOT STARTED`: starting on the parmlib table alone would bring back a partner that was disabled or changed by command |
| `OFTP020E` | Console + log | `SPOOL NOT OPEN: <reason>`; OFTPD ends |
| `OFTP021W` | Console + log | `SPOOL WAS NOT CLOSED (OPEN RC n), VERIFIED`: OFTPD did not end cleanly last time; the spool was checked |
| `OFTP022W` | Console + log | `DD OFTPLOG CANNOT BE OPENED` or `IS NOT RECFM=F OR FB`: the messages go to the console |
| `OFTP023E` | Console + log | `LOG WRITE FAILED, ERRNO n` (once; from then on the messages go to the console) or `LOG CLOSE FAILED, ERRNO n` |
| `OFTP040P` | Log only | `RECOVERED n SENDS, DISCARDED n PARTIAL RECEIVES`: what the start found in the spool |
| `OFTP050E` | Console + log | `LISTEN ON PORT n FAILED, ERRNO n` |
| `OFTP051P` | Log only | `LISTENING ON PORT n, SOCKET n` |
| `OFTP090E` | Console + log | `COM AREA NOT AVAILABLE`; OFTPD ends |
| `OFTP091E` | Console + log | `NO STORAGE`; OFTPD ends |
| `OFTP095I` | Console + log | `WAITING FOR n SESSIONS` at stop (every 10 seconds) |
| `OFTP096W` | Console + log | `<who>: n SESSION SUBTASKS STILL RUNNING`: the stop has waited a minute or more for sessions (repeated every minute; it keeps waiting, the operator's CANCEL ends OFTPD) |
| `OFTP098E` | Console + log | `OFTPD ABEND, n SOCKETS CLOSED, SVC <state>`: OFTPD abended; written even when the log is busy, then to the console only |

## Cross memory (the SVC)

| Id | Dest | Text and meaning |
|---|---|---|
| `OFTP030P` | Log only | `CROSS MEMORY ON SVC n, ANCHOR=... MODULE=... (n BYTES), NOT ACTIVE` |
| `OFTP031P` | Log only | `SVC n ENTRY FOUND <8 bytes>`: the SVC table entry before OFTPD took it |
| `OFTP032P` | Log only | `SVC n RESTORED, ENTRY READS <8 bytes>` |
| `OFTP033E` | Console + log | `SVC n NOT FREE (...)`, no cross memory |
| `OFTP034E` | Console + log | `NO CROSS MEMORY: <reason>` |
| `OFTP035W` | Console + log | `NO CROSS MEMORY, NOT AUTHORIZED` |
| `OFTP036W` | Console + log | `SVC n NOT RESTORED (...)`, CSA retained |
| `OFTP037W` | Console + log | `CROSS MEMORY DRAIN TIMED OUT, n IN FLIGHT, CSA RETAINED` |
| `OFTP038P` | Log only | `NO SVC= IN OFTPPRM, NO CROSS MEMORY`: a choice of the installation |
| `OFTP039P` | Log only | `CROSS MEMORY ACTIVE ON SVC n, SUBSYSTEM <name>` |
| `OFTP060W` | Console + log | `CROSS MEMORY SWEPT SLOTS X'..', A CALLER FAULTED INSIDE THE SVC ROUTINE` |
| `OFTP061W` | Console + log | `CROSS MEMORY FREED SLOTS X'..' UNCOLLECTED FOR n SECONDS` |
| `OFTP062W` | Console + log | `SUBSYSTEM <name> WAS STILL INSTALLED FROM AN EARLIER RUN, UNCHAINED; ITS CSA IS RETAINED` |
| `OFTP063E` | Console + log | `SUBSYSTEM <name> NOT INSTALLED, CROSS MEMORY STAYS OFF` |
| `OFTP064W` | Console + log | `CROSS MEMORY: THE OWNER OF SLOT n IS GONE, ANSWER DISCARDED` |
| `OFTP065E` | Console + log | `CROSS MEMORY SLOT THREAD NOT STARTED, CROSS MEMORY STAYS OFF` |
| `OFTP066W` | Console + log | `CROSS MEMORY: THE OWNER OF SLOT n IS GONE, REQUEST NOT CARRIED OUT` |
| `OFTP067W` | Console + log | `CROSS MEMORY FREED SLOTS X'..', THEIR CALLER IS GONE` |

## Operator commands (`F OFTPD,...`)

These answer a command, so they stay on the console.

| Id | Dest | Text and meaning |
|---|---|---|
| `OFTP100I` | Console + log | `D P`: one line per partner (name, Odette ID, enabled or disabled, level, host and port) |
| `OFTP101I` | Console + log | `D P`: `n PARTNERS, n SESSIONS`; `D S`: `n SESSIONS` |
| `OFTP102I` | Console + log | `D S`: `LARGEST FREE BLOCK n KB, n SUBTASK HANDLES, n SESSIONS` |
| `OFTP110I` | Console + log | `D Q`: one line per transfer (key, direction, state, partner, data set) |
| `OFTP111I` | Console + log | `n TRANSFERS IN THE SPOOL` |
| `OFTP120E` | Console + log | `<name> IS NOT A PARTNER THAT CAN BE CALLED` / `IS NOT A PARTNER` |
| `OFTP121I` | Console + log | `CALL OF <name> REQUESTED` |
| `OFTP122E` | Console + log | `PURGE: DAYS MUST BE 0-9999` / `PURGE NEEDS DAYS: KEEPDAYS=0 KEEPS EVERYTHING` |
| `OFTP123E` | Console + log | `<verb> NEEDS A PARTNER OR ALL` |
| `OFTP124I` | Console + log | `TRACE ON\|OFF FOR ALL SESSIONS` / `FOR PARTNER <partner>`: the answer to `F OFTPD,TRACE` |
| `OFTP125E` | Console + log | `TRACE IS ON FOR ALL SESSIONS, TRACE OFF WITHOUT A PARTNER ENDS IT` |
| `OFTP126E` | Console + log | `TRACE NEEDS ON OR OFF, THEN A PARTNER OR NOTHING` |
| `OFTP127I` | Console + log | `TRACE ON FOR ALL SESSIONS` / `TRACE OFF` / `TRACE ON FOR PARTNERS: <names>`: what `F OFTPD,D` shows |
| `OFTP199E` | Console + log | `UNKNOWN COMMAND: <text>` |
| `OFTP233I` | Console + log | `n SENDS FOR <partner> HELD` / `RELEASED BY OPERATOR`: the answer to `F OFTPD,HOLD` / `RELEASE` |
| `OFTP235I` | Console + log | `n FINISHED TRANSFERS OLDER THAN n DAYS DELETED BY <who>`: after `F OFTPD,PURGE`, or the daily purge when it deleted something |
| `OFTP236E` | Console + log | `SPOOL HOUSEKEEPING FAILED AFTER n, BY <who>` |
| `OFTP237E` | Console + log | `<verb> OF <partner>: SPOOL NOT AVAILABLE` |
| `OFTP243I` | Console + log | `PARTNER <name> ADDED BY <who>`: after `PARTNER ADD` (`docs/partners.md`) |
| `OFTP244I` | Console + log | `PARTNER <name> CHANGED BY <who>`: after `PARTNER CHANGE`, `ENABLE` or `DISABLE` |
| `OFTP245I` | Console + log | `PARTNER <name> DELETED BY <who>`: after `PARTNER DELETE` |

## Sessions and transfers

Every message begins with the partner's name (or the caller's address).
The ones that report a normal transfer are `P`.

| Id | Dest | Text and meaning |
|---|---|---|
| `OFTP201W` | Console + log | `<partner> CALLED FROM <address>, NOT <address>`: a call from an address other than the partner's entry; refused |
| `OFTP202W` | Console + log | `<partner> REFUSED, n SESSIONS RUNNING, SESSIONS=n` |
| `OFTP203P` | Log only | `<partner>: SESSION ENDED, ESID nn (OURS)/(PARTNER), FILES n OUT n IN`, or `ENDED BY ABEND <code>` |
| `OFTP204W` | Console + log | `<address>: CALL REFUSED, n SESSIONS`: OFTPD is at `MAXSESS` |
| `OFTP205P` | Log only | `<address>: CALL RECEIVED` |
| `OFTP206P` | Log only | `<partner>: CALLING AT <host> PORT n` |
| `OFTP207P` | Log only | `<partner>: NOT REACHED`; repeated at each retry. The message that needs action is `OFTP215W` |
| `OFTP208W` | Console + log | `<partner>: SESSION WITHOUT ABEND RECOVERY, RC n` |
| `OFTP209E` | Console + log | `<partner>: SESSION ABEND <code>, CLEANING UP` |
| `OFTP210E` | Console + log | `<partner>: <key> <data set> CANNOT BE OPENED` |
| `OFTP211P` | Log only | `<partner>: <key> <dsn> SENT, n RECORDS`: the partner accepted the file |
| `OFTP212P` | Log only | `<partner>: <key> <dsn> REFUSED, REASON nn RETRY Y/N` |
| `OFTP213P` | Log only | `<partner>: <key> <dsn> DELIVERED` / `NOT DELIVERED (NERP)`: the receipt came |
| `OFTP214P` | Log only | `<partner>: EERP/NERP FOR UNKNOWN FILE <dsn>` |
| `OFTP215W` | Console + log | `<partner>: <key> <dsn> HELD AFTER n ATTEMPTS`: a person has to decide (`RELEASE` or `DELETE`) |
| `OFTP215P` | Log only | `<partner>: <key> <data set> DELETED (DISP=DELETE)` |
| `OFTP216E` | Console + log | `<partner>: ABEND <code> CLOSING THE FILES OF THE SESSION` |
| `OFTP216W` | Console + log | `<partner>: <key> <data set> NOT DELETED (DISP=DELETE)` |
| `OFTP217W` | Console + log | `<partner>: NO ROOM FOR ANOTHER SESSION SUBTASK` |
| `OFTP218P` | Log only | `<partner> BUSY, CALLED AGAIN IN n S`: the partner is in a session with us already |
| `OFTP220E` | Console + log | `<partner>: <dsn> NOT RECEIVED, <data set> NOT ALLOCATED (nn)` |
| `OFTP221E` | Console + log | `<partner>: <data set> NOT RENAMED TO <data set>` |
| `OFTP222P` | Log only | `<partner>: <key> <dsn> RECEIVED AS <data set>, n RECORDS` |
| `OFTP223W` | Console + log | `<partner>: <dsn> FOR <id> REFUSED, <reason>`: not our Odette ID, no route, or it would go back |
| `OFTP240P` | Log only | `<partner>: <key> RELAY: EERP/NERP FOR <dsn> QUEUED` |
| `OFTP241P` | Log only | `<partner>: <key> RELAY: <dsn> FROM <id> FOR <id> QUEUED FOR <partner>` |
| `OFTP242W` | Console + log | `<partner>: <key> RELAY: <dsn> FOR <id> GIVEN UP AFTER n ATTEMPTS`: the originator gets a NERP |
| `OFTP290W` | Console + log | `<partner>: <text>` for a failed identity or password check: `UNKNOWN CALLER <id>`, `WRONG PASSWORD FROM <partner>` (the engine's text, in lower case), `ANSWERED BY <id>, NOT <id>`, `PARTNER PASSWORD DOES NOT MATCH` |
| `OFTP290P` | Log only | `<partner>: <text>`: any other line the protocol engine reports (a refusal reason, a timeout, a file sent or received, the session parameters, and the `TRACE SEND|RECV ...` lines of `F OFTPD,TRACE`) |

## Requests from the utilities

The caller gets the answer from the utility; the console is not used,
except for a refusal for lack of authority.

| Id | Dest | Text and meaning |
|---|---|---|
| `OFTP230P` | Log only | `<key> QUEUED FOR <partner>: <data set> AS <dsn>` |
| `OFTP231P` | Log only | `<key> DELETED BY <user>` |
| `OFTP232P` | Log only | `<key> HOLD/RELEASE/RETRY BY <user>` |
| `OFTP233P` | Log only | `n SENDS FOR <partner> HELD/RELEASED BY <user>` |
| `OFTP238W` | Console + log | `SEND OF <data set> REFUSED: <reason>`: the caller may not read it |

## The OFTP utility

`OFTP001E`, `OFTP002E`, `OFTP003E`, `OFTP004E`, `OFTP007E` and `OFTP010E`
come from the utility itself and are written to its own output (SYSPRINT or
the terminal). They do not use the console or the log. Except for
`OFTP010E`, they are found before OFTPD is asked, and the return code is 8.

| Id | Text and meaning |
|---|---|
| `OFTP001E` | `NO COMMAND IN PARM OR SYSIN` |
| `OFTP002E` | `UNKNOWN COMMAND <word>`; `UNKNOWN PARTNER COMMAND <word>`; or, for `PARTNER` alone, `PARTNER LIST, SHOW, ADD, CHANGE, ENABLE, DISABLE OR DELETE` |
| `OFTP003E` | `COMMAND TOO LONG`, or `OPERAND TOO LONG: <keyword>`: the value of that operand is longer than 79 characters. The keyword is shown as typed, never the value, which may be a password. A keyword of more than 15 characters, or a word of more than 15 characters without `(` or `=`, gives `OPERAND TOO LONG` alone |
| `OFTP004E` | `PARTNER <sub> NEEDS A PARTNER NAME`: `PARTNER ADD`, `CHANGE`, `SHOW` or `DELETE` without a name (`ENABLE` and `DISABLE` are shown as `CHANGE`) |
| `OFTP007E` | `OPERAND <n> HAS NO VALUE: WRITE KEYWORD(VALUE)`: a second word without a value after the one that names the key, the partner or the days (`SHOW`, `DELETE`, `HOLD`, `RELEASE`, `RETRY`, `CALL`, `PARTNER`, `PURGE`), for example `PARTNER CHANGE ACME OURPASS`. `SEND`, `LIST` and `PARTNERS` have no such word. `<n>` is the place of the word, counted after the verb (after `PARTNER CHANGE` for a partner command); the word is not shown, because it may be a password. `XLATE` and `HOLD` alone are taken as `Y` |
| `OFTP010E` | the answer of OFTPD to a request that ends with return code 12, such as an unknown partner or OFTPD not reached (`docs/partners.md`, "What is refused") |
