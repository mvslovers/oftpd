# The test network: six OFTP nodes

Approved by the maintainer 2026-10-13. Checked on the host before any
stand has it: the members below read without an error by `src/oftp#cfg.c`,
every link matches both ways (ID, port, passwords), and all 30 routes
between the six nodes arrive through `oftp_cfg_route()` in at most four
hops, each node on the way with `RELAY=Y`. Five MVS
stands on mvsdev and the mendelson OFTP2 CE peer on the Mac, wired as a
tree so that files cross up to four hops. The M38 stands are MBT's to
schedule (after the ufsd/ftpd/httpd/mvsMF rollout); oftpd installs there
one slot per stand.

```
                     MAC1 (Mac, mendelson)
                        |
                      DEV1  relay
                     /     \
            relay  M38L    M38B  relay
                    |        |
                  M38X     M38R
```

Every node knows every other one: its neighbours directly (`HOST=`), the
rest `VIA=` the neighbour towards them. DEV1, M38L and M38B are nodes
(`RELAY=Y`). The longest path is M38X -> M38L -> DEV1 -> M38B -> M38R,
four hops, with the EERP coming back the same way.

## Addresses

All five MVS stands run on the same Hercules host, and DYN75 sockets are
the host's, so every OFTPD needs ports of its own on 192.168.0.233
(mvsdev.lan). Free there on 2026-10-13 (`ss -ltn`): 3315-3346.

| Node | Stand | Odette ID | OFTP port | Control port |
|---|---|---|---|---|
| DEV1 | dev1 (MVS/CE) | `O0000000000DEV1` (`O01791824916311` until 2026-10-14) | 3305 | 3306 |
| M38L | m38l (MVS/CE LAB) | `O0000000000M38L` | 3315 | 3316 |
| M38X | m38x (MVS/CE EXP) | `O0000000000M38X` | 3325 | 3326 |
| M38B | m38b (TK5) | `O0000000000M38B` | 3335 | 3336 |
| M38R | m38r (TK5 REF) | `O0000000000M38R` | 3345 | 3346 |
| MAC1 | Mac 192.168.0.23 | `O0000000000MAC1` (MENDEL, `O01791824860109`, until 2026-10-14) | 3305 | - |

The utility and the dialog take the control port with `PORT(3316)` and
so on; the dialog keeps it in the user's profile. That is 0.1.0, which
the stands run: the control port is removed on `main` (#32, 2026-10-19),
so a stand moving to a later build needs `SVC=237` in its `OFTPPRM0` and
`CONTROLPORT=` taken out (OFTPD warns about it with `OFTP010W`), and the
utility and the dialog then reach OFTPD only through the SVC.

Passwords: a node sends its own name (`OURPASSWORD=<own name>`) and
expects the partner's (`PASSWORD=<partner name>`), MAC1 included (it
had `TEST` both ways until 2026-10-14).

## The members

Per stand: `SYS2.PARMLIB(OFTPPRM0)` and `(OFTPPTN0)`, received files
under `OFTPD.IN`, in transit `OFTPD.TMP`. The spool is
`IBMUSER.OFTPD.SPOOL` (MVS/CE) or `HERC01.OFTPD.SPOOL` (TK5), not
`OFTPD.SPOOL`: `OFTPD` has no catalog alias on either, and neither master
catalog owns VSAM space outside the system volumes
(`internals/m38-changes.md`).

### M38L

```
* OFTPPRM0 - OFTPD on M38L (test network, internals/testnet.md)
ODETTEID=O0000000000M38L
PORT=3315 CONTROLPORT=3316
INPREFIX=OFTPD.IN TMPPREFIX=OFTPD.TMP
RETRY=60 RELAY=Y
```

```
* OFTPPTN0 - OFTPD partners on M38L
PARTNER=DEV1 ODETTEID=O0000000000DEV1
  PASSWORD=DEV1 OURPASSWORD=M38L HOST=192.168.0.233 PORT=3305
PARTNER=M38X ODETTEID=O0000000000M38X
  PASSWORD=M38X OURPASSWORD=M38L HOST=192.168.0.233 PORT=3325
PARTNER=MAC1 ODETTEID=O0000000000MAC1 VIA=DEV1
PARTNER=M38B ODETTEID=O0000000000M38B VIA=DEV1
PARTNER=M38R ODETTEID=O0000000000M38R VIA=DEV1
```

### M38X

```
* OFTPPRM0 - OFTPD on M38X (test network, internals/testnet.md)
ODETTEID=O0000000000M38X
PORT=3325 CONTROLPORT=3326
INPREFIX=OFTPD.IN TMPPREFIX=OFTPD.TMP
RETRY=60
```

```
* OFTPPTN0 - OFTPD partners on M38X
PARTNER=M38L ODETTEID=O0000000000M38L
  PASSWORD=M38L OURPASSWORD=M38X HOST=192.168.0.233 PORT=3315
PARTNER=DEV1 ODETTEID=O0000000000DEV1 VIA=M38L
PARTNER=MAC1 ODETTEID=O0000000000MAC1 VIA=M38L
PARTNER=M38B ODETTEID=O0000000000M38B VIA=M38L
PARTNER=M38R ODETTEID=O0000000000M38R VIA=M38L
```

### M38B

```
* OFTPPRM0 - OFTPD on M38B (test network, internals/testnet.md)
ODETTEID=O0000000000M38B
PORT=3335 CONTROLPORT=3336
INPREFIX=OFTPD.IN TMPPREFIX=OFTPD.TMP
RETRY=60 RELAY=Y
```

```
* OFTPPTN0 - OFTPD partners on M38B
PARTNER=DEV1 ODETTEID=O0000000000DEV1
  PASSWORD=DEV1 OURPASSWORD=M38B HOST=192.168.0.233 PORT=3305
PARTNER=M38R ODETTEID=O0000000000M38R
  PASSWORD=M38R OURPASSWORD=M38B HOST=192.168.0.233 PORT=3345
PARTNER=MAC1 ODETTEID=O0000000000MAC1 VIA=DEV1
PARTNER=M38L ODETTEID=O0000000000M38L VIA=DEV1
PARTNER=M38X ODETTEID=O0000000000M38X VIA=DEV1
```

### M38R

```
* OFTPPRM0 - OFTPD on M38R (test network, internals/testnet.md)
ODETTEID=O0000000000M38R
PORT=3345 CONTROLPORT=3346
INPREFIX=OFTPD.IN TMPPREFIX=OFTPD.TMP
RETRY=60
```

```
* OFTPPTN0 - OFTPD partners on M38R
PARTNER=M38B ODETTEID=O0000000000M38B
  PASSWORD=M38B OURPASSWORD=M38R HOST=192.168.0.233 PORT=3335
PARTNER=DEV1 ODETTEID=O0000000000DEV1 VIA=M38B
PARTNER=MAC1 ODETTEID=O0000000000MAC1 VIA=M38B
PARTNER=M38L ODETTEID=O0000000000M38L VIA=M38B
PARTNER=M38X ODETTEID=O0000000000M38X VIA=M38B
```

### DEV1: additions to today's members

`OFTPPRM0` keeps its settings (`RELAY=Y` already); since 2026-10-14 it
reads `ODETTEID=O0000000000DEV1`. `OFTPPTN0` keeps MAC1 (formerly
MENDEL, now `PASSWORD=MAC1 OURPASSWORD=DEV1`), BEHIND (`VIA=MAC1`), MACA
and MACB and gets:

```
PARTNER=M38L ODETTEID=O0000000000M38L
  PASSWORD=M38L OURPASSWORD=DEV1 HOST=192.168.0.233 PORT=3315
PARTNER=M38B ODETTEID=O0000000000M38B
  PASSWORD=M38B OURPASSWORD=DEV1 HOST=192.168.0.233 PORT=3335
PARTNER=M38X ODETTEID=O0000000000M38X VIA=M38L
PARTNER=M38R ODETTEID=O0000000000M38R VIA=M38B
```

### MAC1

mendelson's own station is `O0000000000MAC1` (name MAC1). It calls
DEV1 (mvsdev.lan:3305, password `MAC1`, marked as a gateway) and knows
the four M38 nodes with DEV1 as their gateway (`GATEWAYOFTPID`; their
host is DEV1's too), so a file for M38R goes to DEV1 with SFIDDEST
M38R. Without these entries mendelson refuses a file from an M38 node as
an unknown origin (reason 03). Each partner has an outbox poll; DEV1's
and M38R's carry a virtual file name of their own (`REN.MAC1.TO.DEV1`,
`REN.MAC1.TO.M38R`), the others still send `NOT-CATEGORIZED`. Set on
2026-10-14 straight in its configuration database (HSQLDB, tables
`PARTNER`, `CERTIFICATES`, `OUTBOUNDPOLL`; each new partner copied from
DEV1's rows), then mendelson restarted.

## Tests, in order

1. Each stand alone: install, start, `OFTP004I` (which authorization
   route), `F OFTPD,D P`.
2. Each direct link both ways: a T file and an F file, EERP back.
3. One hop through a node: M38X -> M38L -> DEV1, M38R -> M38B -> DEV1.
4. Four hops: M38X -> M38R and back; M38R -> MAC1 (MENDEL until
   2026-10-14).
5. A NERP over several hops: a file for M38R while M38R is down, until
   `MAXRETRY` gives it up at M38B, the NERP travelling back to M38X.
6. Later, on purpose: a ring (M38X routed back VIA M38L towards itself),
   which loop detection beyond one hop does not catch yet.

## Results, 2026-10-13

All five MVS nodes on 0.1.0-dev (`1ca9786`), installed through SMP
(`internals/m38-changes.md`); every file below compared with its source
afterwards: identical.

| Step | Path | Result |
|---|---|---|
| 1 | each stand alone | `OFTP004I AUTHORIZED BY SVC` on all four (MVS/CE and TK5) |
| 2 | DEV1-M38L, DEV1-M38B, M38L-M38X, M38B-M38R | an F and a T file each way, EERP back each time |
| 3 | M38X -> M38L -> DEV1, M38R -> M38B -> DEV1 | delivered, EERP back to the origin (m38x JOB00612, m38r JOB00385) |
| 4 | M38X -> M38L -> DEV1 -> M38B -> M38R and back | delivered both ways, EERPs back (JOB00614, JOB00387) |
| 4 | M38R -> M38B -> DEV1 -> MENDEL | refused by mendelson, reason 03; the NERP came back to M38R |
| 5 | M38X -> M38R with M38R down | m38b (`RETRY=10 MAXRETRY=2` for the test) gave up after 2 attempts; NERP 99 back over three hops to M38X in about 40 s (JOB00616) |

Found on the way:

- **mendelson refuses a file from an origin it has no partner for**
  (`getTransmissionSender()` is null in its log, NERP 03). To test
  M38R -> MENDEL, mendelson needs the M38 nodes as partners of its own,
  without a connection (done 2026-10-14, below).
- **An EERP waited at a relay until a manual `CALL`**: in step 4 the
  EERP and the NERP for M38R stayed at m38b ("relay: EERP to pass back to
  M38R") for minutes, after M38B and M38R had called each other at the
  same second (15:59:44; one side refused with ESID 03, which sets the
  caller's next call RETRY seconds ahead). `CALL M38R` on m38b delivered
  both. Repeated with normal settings (JOB00391): back in about 25 s
  without a call. Not reproduced, cause not established.
- **The control port can fail to open on a quick restart**: `OFTP052W
  CONTROL PORT 3336 NOT OPEN, ERRNO 48`, a TIME-WAIT on 127.0.0.1:3336
  left by the last utility connections (`ss` on the host). There is no
  `setsockopt` for SO_REUSEADDR, so OFTPD must retry the bind instead of
  giving up. Restarted after the TIME-WAIT was gone.
- `F OFTPD,D P` shows a partner reached through another one as `-:3305`;
  it should name the `VIA` partner.
- In SYSIN, columns 73-80 are ignored: an operand that ends past column
  72 is lost silently (here `PORT(...)`, so the request went to the
  default control port 3306). Continue the line instead.

## Renamed, 2026-10-14

DEV1 became `O0000000000DEV1` and MENDEL became MAC1, `O0000000000MAC1`,
so that every node follows the same scheme (maintainer's request).
Changed: dev1 `OFTPPRM0` and `OFTPPTN0` (OFTPD restarted, STC00675),
`OFTPPTN0` on the four M38 stands (`F OFTPD,REFRESH`: `5 ENABLED, 1 NEW,
1 DISABLED` on each), and mendelson's configuration (section MAC1).
`REFRESH` keeps a partner that left the member: MENDEL stays listed as
DISABLED until the next restart. Every file compared with its source:
identical.

| Path | Result |
|---|---|
| DEV1 -> MAC1 (F, 3 x 80) | delivered, mendelson identified DEV1 by its new ID and sent the EERP (dev1 JOB02229) |
| MAC1 -> DEV1 (U, from mendelson's outbox) | received as `IBMUSER.OFTPIN.REN.MAC1.TO.DEV1`, EERP back to mendelson |
| M38R -> M38B -> DEV1 -> MAC1 | accepted by mendelson now that it knows M38R, EERP back over three hops (m38r JOB00414) |
| MAC1 -> DEV1 -> M38B -> M38R | mendelson sent it through its gateway DEV1, stored as `OFTPD.IN.REN.MAC1.TO.M38R`, EERP back to mendelson |
