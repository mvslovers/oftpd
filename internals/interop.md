# Interop log

Sessions against other OFTP implementations, newest first. Each entry
says what ran, at which level, and what the partner's own log said, so a
result can be checked rather than believed.

| Date | Partner | Who called | Level | What ran | Result |
|---|---|---|---|---|---|
| 2026-10-12 | mendelson opensource OFTP2 1.1 build 53, plain port 3305, workstation | `host/oftph` | 5, buffer 4096, credit 99 | in the same session: EERP for the T file below received; a U file (33 octets) received from mendelson's outbox; our EERP for it sent | ESID 00 from mendelson. mendelson's log: our EERP received, its outbound copy deleted |
| 2026-10-12 | same | `host/oftph` | 5, buffer 4096, credit 99 | T file (20 octets) sent | ESID 00 from us. mendelson's log: 20 octets received, "unsigned EERP requested" queued |

## Notes

- **mendelson sends a per-partner password** in its SSID, the one entered
  for the remote partner, not one of its local station.
- **mendelson names files from its outbox `NOT-CATEGORIZED`** unless its
  directory watch is given a virtual file name, and sends them as U.
- mendelson answered our level-5 SSID with level 5, buffer 4096, credit
  99, no compression, no restart (`X5...04096BNNN099N`).
