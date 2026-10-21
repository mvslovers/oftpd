# Interop log

Sessions against other OFTP implementations, newest first. Each entry
says what ran, at which level, and what the partner's own log said, so a
result can be checked rather than believed.

| Date | Partner | Who called | Level | What ran | Result |
|---|---|---|---|---|---|
| 2026-10-21 | `host/oftph listen -x 10000` as partner CUT (resets the connection once a received file has delivered 10 000 octets) | OFTPD called CUT (MVS/CE container, OFTPD `9d854d1`, `RETRY=30 MAXRETRY=3`) | 5 | a T file of 500 lines queued for CUT; the connection is cut in the middle of the file every time | three calls, at +0 s, +31 s and +61 s after the previous one (`RETRY` times the attempts so far, 30 s then 60 s, up to the 10-second scheduler tick); the partner saw the file from the first octet each time ("cut CUT.TEST1 after 10019 octets" three times); each ended with `send failed`, ESID 99 (ours), `FILES 0 OUT`; after the third `OFTP215W ... HELD AFTER 3 ATTEMPTS`, the transfer is `S H`, no fourth call within five minutes |
| 2026-10-21 | `host/oftph listen` with a wrong password as partner BADPW | OFTPD called BADPW (same set-up) | 5 | a T file queued for BADPW; the partner's password does not match, so every call fails before any file | `OFTP290W partner password does not match`, ESID 04 (ours), `FILES 0 OUT 0 IN`; five calls, each 30 s after the previous one (`RETRY`, not growing with the number of calls); five is more than `MAXRETRY=3`, and the transfer stayed `S Q`: a call that fails before a file is claimed is not an attempt, so it is never HELD for it. `HOLD BADPW` ended it |
| 2026-10-21 | same partner CUT | OFTPD called CUT | 5 | two T files queued for CUT: a 3-line file, then the 500-line one. `oftph` takes one file per session and refuses the second with SFNA 99, retry | session 1: the small file sent and DELIVERED (EERP back), the big one `REFUSED, REASON 99 RETRY Y`, which counts as attempt 1; the next call came 38 s later (30 s, then the tick); the connection was cut in the middle of the file in calls 2 and 3 (the second wait 61 s); `OFTP215W ... HELD AFTER 3 ATTEMPTS`; the small file stayed DELIVERED. A cut of the second file *in the same session* was not measured (`oftph` refuses it); `TSTSESS` covers the engine's side |
| 2026-10-20 | mendelson opensource OFTP2 1.1 build 53 (headless under Xvfb, in a container; plain port) | mendelson called OFTPD (MVS/CE container, buffer 4096, credit 99, level 5) | 5 | F, RECORDFORMAT F 80, 64 000 records = 5 120 000 octets, after the fix of #58 | delivered, EERP back, mendelson deleted its copy; data set `FB 80`, read back through mvsMF: md5 identical to the source. Before the fix: EFNA 05 (see Notes) |
| 2026-10-20 | same | mendelson called OFTPD | 5 | T file of 64 000 lines (5 248 000 octets with CR-LF) | delivered, EERP back; 64 000 records, each line identical to the source |
| 2026-10-20 | same | mendelson called OFTPD | 5 | U file, 5 MiB (5 242 880 octets); also 1000 octets | delivered, EERP back, md5 of the data set read back identical to the source; 342 tracks allocated |
| 2026-10-20 | same | OFTPD called mendelson (batch `SEND`) | 5 | T, F 80 and U files of 64 000 records each (5 248 000 octets as mendelson stores T, 5 120 000 for F and U) | all three delivered with the EERP; md5 of what mendelson stored identical to the source |
| 2026-10-13 | `host/oftph` as MACA; MACB not running | MACA called OFTPD; OFTPD called MACB | 5 | retry limit, OFTPD (STC00639) with `RETRY=10 MAXRETRY=2`: (a) our own T file for MACB; (b) a T file from MACA for MACB, relayed | (a) WAITING, then HELD after 2 attempts ("partner not reached, given up after 2 attempts"); after RELEASE the count started again from 1. (b) REJECTED after 2 attempts, copy `OFTPTMP.T0000014` deleted, a NERP (reason 99, created by OFTPD) queued for MACA; MACA called again and got it, ESID 00 |
| 2026-10-12 | two `host/oftph` (MACA, MACB) through OFTPD on dev1 as a node (`RELAY=Y`) | MACA called OFTPD; OFTPD called MACB | 5 | T file from MACA addressed to MACB (`-D`); OFTPD stored it (T0000011), passed it on, got MACB's EERP, queued it for MACA (T0000012); MACA called again | MACB got the 47 octets unchanged (cmp); MACA got the EERP "from O0000000000MACB"; OFTPD's copy deleted; all ESID 00 |
| 2026-10-12 | mendelson 1.1 b53 as a node | OFTPD on dev1 | 5 | T file for partner BEHIND (`VIA=MENDEL`): session with MENDEL, SFIDDEST = BEHIND's Odette ID | refused with SFNA 02; mendelson's log: "receiver not configured in this system ... Invalid destination; SFID.SFIDDEST" -- the route reached the wire as designed |
| 2026-10-12 | mendelson 1.1 b53 | OFTPD as a started task on dev1 (`S OFTPD,S='IBMUSER.OFTP370.SPOOL'`) | 5 | T file `STC.TEST` queued through the control port, EERP fetched with `F OFTPD,CALL MENDEL` | T0000003 DELIVERED |
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
- **mendelson sets the end-of-record flag wrongly in a long F file.** It
  packs a record as a 63-octet subrecord and a 17-octet one with the flag
  (LRECL 80), starting that pattern afresh at the head of every DATA
  buffer. A record cut by the end of a buffer loses its flag, and the
  flags after it fall 63 octets into the next record. In 5 120 000
  octets (64 000 records) it set 62 989 flags, and its EFID counts the
  flags, not the records; the octets (5 120 000) were right and the
  payload, reassembled from a capture, identical to the file. OFTPD took
  each flag as a record end and rejected the file with EFNA 05 (a record
  longer than the LRECL). A file within one buffer (10 records) passed;
  the capture shows the flags going wrong at the first buffer end. OFTPD
  now cuts F records at the LRECL of the SFID, counts the flags only, and
  accepts an EFID record count that equals either its own record count or
  the flags it saw (#58). V is not given this treatment: it has no fixed
  length to cut at. Not measured against mendelson (its poll did not
  pick up a V file in the time available); a V file from mendelson that
  spans buffers may meet the same flag problem.
- A U or T file from mendelson of 5 MB needed no change; the size problem
  of #21 (the space estimate from the SFID) does not bite there, the SFID
  size being right.
