# Interop log

Sessions against other OFTP implementations, newest first. Each entry
says what ran, at which level, and what the partner's own log said, so a
result can be checked rather than believed.

| Date | Partner | Who called | Level | What ran | Result |
|---|---|---|---|---|---|
| 2026-10-12 | mendelson 1.1 b53 (workstation) | mendelson | 5 | U file `NOT-CATEGORIZED`, 43 octets, to OFTPD on MVS (dev1, JOB02129) | stored as `IBMUSER.OFTPIN.NOT-CATE`, the 43 octets unchanged (read back through mvsMF); OFTPD's EERP went back in the same session, mendelson deleted its outbound copy; ESID 00 |
| 2026-10-12 | mendelson 1.1 b53 (workstation) | mendelson | 5 | mendelson sent a file from its outbox to OFTPD on MVS (dev1, JOB02121) | refused by OFTPD with SFNA 01: mendelson's virtual file name `NOT-CATEGORIZED` is not a valid data set qualifier; session ended normally |
| 2026-10-12 | same | OFTPD on MVS (dev1, JOB02121), `F OFTPD,CALL MENDEL` | 5 | no file; mendelson used the session to send its EERP for `MVS.SAMPLE` | spool record T0000001 DELIVERED |
| 2026-10-12 | same | OFTPD on MVS (dev1, JOB02119), queued through the control port | 5, buffer 4096, credit 99 | T file `MVS.SAMPLE` from a FB 80 data set, 3 records | ESID 00. mendelson stored 108 octets: the three lines without their trailing blanks, each with CR-LF, in ASCII |
| 2026-10-12 | mendelson opensource OFTP2 1.1 build 53, plain port 3305, workstation | `host/oftph` | 5, buffer 4096, credit 99 | in the same session: EERP for the T file below received; a U file (33 octets) received from mendelson's outbox; our EERP for it sent | ESID 00 from mendelson. mendelson's log: our EERP received, its outbound copy deleted |
| 2026-10-12 | same | `host/oftph` | 5, buffer 4096, credit 99 | T file (20 octets) sent | ESID 00 from us. mendelson's log: 20 octets received, "unsigned EERP requested" queued |

## Notes

- **mendelson sends a per-partner password** in its SSID, the one entered
  for the remote partner, not one of its local station.
- **mendelson names files from its outbox `NOT-CATEGORIZED`** unless its
  directory watch is given a virtual file name, and sends them as U.
- mendelson answered our level-5 SSID with level 5, buffer 4096, credit
  99, no compression, no restart (`X5...04096BNNN099N`).
