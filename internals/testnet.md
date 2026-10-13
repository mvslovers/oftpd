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
                     MENDEL (Mac, mendelson)
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
| DEV1 | dev1 (MVS/CE) | `O01791824916311` (as today) | 3305 | 3306 |
| M38L | m38l (MVS/CE LAB) | `O0000000000M38L` | 3315 | 3316 |
| M38X | m38x (MVS/CE EXP) | `O0000000000M38X` | 3325 | 3326 |
| M38B | m38b (TK5) | `O0000000000M38B` | 3335 | 3336 |
| M38R | m38r (TK5 REF) | `O0000000000M38R` | 3345 | 3346 |
| MENDEL | Mac 192.168.0.23 | `O01791824860109` (as today) | 3305 | - |

The utility and the dialog take the control port with `PORT(3316)` and
so on; the dialog keeps it in the user's profile.

Passwords: a node sends its own name (`OURPASSWORD=<own name>`) and
expects the partner's (`PASSWORD=<partner name>`). MENDEL keeps `TEST`.

## The members

Per stand: `SYS2.PARMLIB(OFTPPRM0)` and `(OFTPPTN0)`, the spool
`OFTPD.SPOOL`, received files under `OFTPD.IN`, in transit `OFTPD.TMP`.
The volume for the spool comes from MBT's state report at the start of
the slot.

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
PARTNER=DEV1 ODETTEID=O01791824916311
  PASSWORD=DEV1 OURPASSWORD=M38L HOST=192.168.0.233 PORT=3305
PARTNER=M38X ODETTEID=O0000000000M38X
  PASSWORD=M38X OURPASSWORD=M38L HOST=192.168.0.233 PORT=3325
PARTNER=MENDEL ODETTEID=O01791824860109 VIA=DEV1
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
PARTNER=DEV1 ODETTEID=O01791824916311 VIA=M38L
PARTNER=MENDEL ODETTEID=O01791824860109 VIA=M38L
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
PARTNER=DEV1 ODETTEID=O01791824916311
  PASSWORD=DEV1 OURPASSWORD=M38B HOST=192.168.0.233 PORT=3305
PARTNER=M38R ODETTEID=O0000000000M38R
  PASSWORD=M38R OURPASSWORD=M38B HOST=192.168.0.233 PORT=3345
PARTNER=MENDEL ODETTEID=O01791824860109 VIA=DEV1
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
PARTNER=DEV1 ODETTEID=O01791824916311 VIA=M38B
PARTNER=MENDEL ODETTEID=O01791824860109 VIA=M38B
PARTNER=M38L ODETTEID=O0000000000M38L VIA=M38B
PARTNER=M38X ODETTEID=O0000000000M38X VIA=M38B
```

### DEV1: additions to today's members

`OFTPPRM0` stays as it is (`RELAY=Y` already). `OFTPPTN0` keeps MENDEL,
BEHIND, MACA and MACB and gets:

```
PARTNER=M38L ODETTEID=O0000000000M38L
  PASSWORD=M38L OURPASSWORD=DEV1 HOST=192.168.0.233 PORT=3315
PARTNER=M38B ODETTEID=O0000000000M38B
  PASSWORD=M38B OURPASSWORD=DEV1 HOST=192.168.0.233 PORT=3335
PARTNER=M38X ODETTEID=O0000000000M38X VIA=M38L
PARTNER=M38R ODETTEID=O0000000000M38R VIA=M38B
```

### MENDEL

mendelson gets DEV1 as its only partner, which it has already. It cannot
address a file to an Odette ID other than the partner it calls, so from
the Mac only files *to* DEV1 start; files *for* MENDEL can start anywhere
and arrive through DEV1.

## Tests, in order

1. Each stand alone: install, start, `OFTP004I` (which authorization
   route), `F OFTPD,D P`.
2. Each direct link both ways: a T file and an F file, EERP back.
3. One hop through a node: M38X -> M38L -> DEV1, M38R -> M38B -> DEV1.
4. Four hops: M38X -> M38R and back; M38R -> MENDEL.
5. A NERP over several hops: a file for M38R while M38R is down, until
   `MAXRETRY` gives it up at M38B, the NERP travelling back to M38X.
6. Later, on purpose: a ring (M38X routed back VIA M38L towards itself),
   which loop detection beyond one hop does not catch yet.
