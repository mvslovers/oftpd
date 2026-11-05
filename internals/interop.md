# Interop log

Sessions against other OFTP implementations, newest first. Each entry
says what ran, at which level, and what the partner's own log said, so a
result can be checked rather than believed.

| Date | Partner | Who called | Level | What ran | Result |
|---|---|---|---|---|---|
| 2026-11-05 | Oftp4Net (image `ghcr.io/jskrobak/oftp4net:latest`, revision `f9ca21d`, built 2026-10-14), partner OFTPD set to `ProtocolLevel` 1 | both, one after the other (OFTPD `03f7fba`, MVS/CE container) | 1 (1.2) | a T file of 3 lines from OFTPD; a U file of 34 octets from Oftp4Net | Oftp4Net to OFTPD: delivered, EERP back, the data set identical to the source (md5). OFTPD to Oftp4Net: Oftp4Net stored the 44 octets and sent its EERP, but OFTPD logged `OFTP214P ... EERP FOR UNKNOWN FILE` and the transfer stayed `SENT` (#190) |
| 2026-11-05 | same, refusals at level 2 with receive rules `VFN=BADQ.*  DSN=IBMUSER.IN.%Q2` and `VFN=CTR.*  DSN=IBMUSER.CTR.C%CTR` | Oftp4Net called OFTPD | 2 (1.3) | `BADQ.9ABC` (qualifier starts with a digit), `CTR.A` | `BADQ.9ABC`: OFTPD `OFTP224W ... RULE BADQ MAKES NO VALID DATA SET NAME`, SFNA 01 retry N; Oftp4Net marked it `FAILED`, "Refused by partner (01)", and did not retry. `CTR.A` was accepted as `...C0000` although the text promises a refusal without a counter (#191) |
| 2026-11-05 | same, OFTPD as a node (`RELAY=Y`, a second Oftp4Net listener as partner D, route `X` `VIA=D` that D does not know) | Oftp4Net called OFTPD (sender S); OFTPD called D, then S | 2 (1.3) | U file `R2.TO.X` for X | OFTPD took it as a relay, D refused it with SFNA 02 (retry N); OFTPD logged `OFTP212P ... REFUSED, REASON 02 RETRY N` and queued a NERP for S, then called S: `response 'N' not possible at this level`, nothing sent. The record for S reads `NERP passed on, or dropped below 1.4`; Oftp4Net's item stays `SENT`, S is never told |
| 2026-11-05 | same, OFTPD as a node, same route | Oftp4Net called OFTPD (sender S); OFTPD called D, then S | 4 (1.4) | U file `R4.TO.X` for X | D refused with SFNA 02; OFTPD sent a NERP (reason 02) to S in a new session; Oftp4Net: item `NOT_DELIVERED`, "NERP from ... (02)", event `NerpReceived`. OFTPD as a node sends the correct NERP to a 1.4 sender |
| 2026-11-05 | same, Oftp4Net `POST /inbox/{id}/not-delivered` with reason 05 | OFTPD called Oftp4Net for the file; Oftp4Net called back with the NERP in the next session | 4 (1.4) | T file `N4.FROM.OFTPD`; Oftp4Net had already sent its EERP, then reported the file as not delivered (the REST call is allowed only while no end response has gone, so the status was put back by hand in its database) | OFTPD matched the NERP to the file: `OFTP213P ... NOT DELIVERED (NERP)`, the transfer became `REJECTED`, "NERP received, reason 05" (it was `DELIVERED` before). A NERP after an EERP flips the state; no other check |
| 2026-11-05 | same, partner OFTPD at `ProtocolLevel` 2 | OFTPD called Oftp4Net; Oftp4Net called OFTPD | 2 (1.3) | a T file of 3 lines from OFTPD; a T file from Oftp4Net (44 octets, CR-LF) | Oftp4Net to OFTPD: delivered, EERP back, identical content. OFTPD to Oftp4Net: file stored (short stamps `YYMMDD`/`HHMMSS` seen by Oftp4Net), EERP sent, but OFTPD did not match it, transfer `SENT` (#190) |
| 2026-11-05 | same, partner OFTPD at `ProtocolLevel` 4 | OFTPD called Oftp4Net; Oftp4Net called OFTPD | 4 (1.4) | T file of 3 lines and a U file, both ways | all four delivered with the EERP: OFTPD's records `DELIVERED` / `CONFIRMED`; Oftp4Net `CONFIRMED` / `DELIVERED`; content identical (md5) for the file from Oftp4Net, the T file arrives at Oftp4Net with CR-LF |
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

- **Oftp4Net** (free, MIT, .NET; run as a container with its database) is
  the 1.x partner: the release is set per partner (`ProtocolLevel` 5, 4, 2
  or 1) and the session runs at the lower of the two levels. Plain TCP is
  a listener without TLS. It was set up with its database and driven with
  its REST API (`POST /api/v1/outbox`, `GET /api/v1/inbox`); the OFTPD side
  used `PARTNER CHANGE ... LEVEL(n)` to change the level between runs.
  OFTPD's `LEVEL=` is the highest level it offers: the lower of the two
  wins, and the SSID is 61 bytes at every level, so one set of partner
  entries serves all of them.
- **Date and time per level.** At 1.4 both sides send `CCYYMMDD` and
  `HHMMSScccc` (OFTPD's last four digits are its counter, `0001`, `0002`,
  ...); at 1.3 and 1.2 both send `YYMMDD` and `HHMMSS`, which is what
  Oftp4Net showed for OFTPD's files.
- **The EERP of a 1.3 or 1.2 partner is not matched** (#190). The spool
  record keeps `HHMMSScccc`, the EERP gives `HHMMSS`, padded with `0000`.
  Everything else at those levels passed.
- **There is no NERP below 1.4.** A refusal inside a session is an SFNA or
  EFNA and works at every level. A NERP is something a *later* node
  sends. OFTPD sends one only as a relay; below 1.4 it cannot (the log
  says `response 'N' not possible at this level`), so a 1.3 sender of a
  relayed file that fails further on is never told, and the item stays
  `SENT` on its side. The requirement "a failed file from a 1.3 sender gets
  a correct NERP" is therefore met at level 4 (the sender was at 1.4) and
  can only be "dropped, and recorded" at 1.3.
- **Oftp4Net answers with the identity of the listener** (unless the
  partner has an inbound identity set; then a call to the other listener
  is answered with the wrong Odette ID and OFTPD ends it, ESID 03). It
  sends the one password of the partner entry on every listener, so
  OFTPD's entry for the second listener needs that password, not the
  identity's own (otherwise `partner password does not match`, ESID 04).
- Oftp4Net's `not-delivered` call is accepted only while no end response
  has been sent, and it sends the NERP in the next session. In a normal
  run the EERP leaves in the same session as the file, so the call comes
  too late; the test had to put the status back in the database first.
- A virtual file name over 8 characters per qualifier is shortened by
  OFTPD to 8 (`TOOLONGQUALIFIER.X` is stored as `...TOOLONGQ.X`) and a
  hyphen is kept: both are valid data set names, so no SFNA arises there.
  A rule with `%Qn` is where an invalid name comes from (see the table).
