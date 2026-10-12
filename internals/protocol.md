# OFTP (ODETTE-FTP) 1.3 / 1.4 / 2.0 -- technical summary for an MVS 3.8j implementation

Compiled 2026-10-12 from the sources in section 0. **The 1.4 layouts come from the expired
draft-nash-mokry-rfc2204bis-01, not from the ODETTE 1.4 document**, which is not public; treat
every 1.4-only detail as unconfirmed until checked against a 1.4 peer. Design basis for an OFTP server and client utilities on MVS 3.8j
(S/370, 24-bit, EBCDIC, plain TCP sockets, no TLS/RSA/X.509/CMS; available: SHA-256,
Blowfish, base64, old zlib port).

## 0. Sources and how far each was verified

| Tag | Source | Read | Notes |
|---|---|---|---|
| [5024] | RFC 5024, ODETTE File Transfer Protocol 2 (Nov 2007), Informational, obsoletes 2204 | completely (sections 1-11, App. A-C; App. D ISDN skimmed) | Updated by RFC 8996 (TLS 1.0/1.1 deprecation) |
| [5024E] | RFC 5024 errata 3453, 3454 (both technical, "Held for Document Update", 2014-01-20) | completely | text quoted in 1.7 and 3.1 |
| [2204] | RFC 2204, ODETTE File Transfer Protocol (1.3 over TCP), Nov 1997 | completely | |
| [14] | draft-nash-mokry-rfc2204bis-00/-01 "ODETTE File Transfer Protocol Revision 1.4" (Feb 2002, expired Internet-Draft) | change-marked sections fully, rest diffed against [2204] | **The authoritative ODETTE 1.4 document (April 2000) is not freely available; this draft is the only public text.** 1.4 = 1.3 + NERP + extended date/time + reason code 14 + state-table fixes. |
| [IG] | ODETTE OP08 "OFTP2 Implementation Guidelines" v2.4, March 2015 | Parts 1 and 2 completely, appendices partly | Best-practice recommendation, not the protocol spec |
| [TC] | ODETTE "OFTP2 Interoperability Tests -- Test Cases" v2.4, 2020-09-09 | test list + 4.1.x, 4.2.x read | the certification test suite |
| [twdes] | github.com/twdes/odette (C#, EUPL 1.1) | DATA buffer encoder/decoder and SSID negotiation code read | used only as a sample of real-world behaviour |
| secondary | vendor pages (mendelson, Accord, nsoftware, Volvo), web search snippets | as cited | marked "secondary" where used |

Section numbers below are those of [5024] unless prefixed otherwise ("2204 s5.3.3", "1.4 s5.3.15", "IG s2.1").

**RFC 5024 contains no RFC 2119 boilerplate.** It is Informational; "must"/"should" appear in
lower case in prose. Where this document says MUST it quotes the RFC's own wording, it does not
upgrade it. The only sentence in [5024] that literally says "mandatory" about a feature set is
s10.2: "Support of all cipher suites listed here is mandatory." (see 3.4).

Not verifiable from any primary source I could obtain (flagged again where relevant):
- the exact text of ODETTE OFTP 1.4 (April 2000) and of OFTP 1.2 (SSIDLEV '1');
- any rule for when an encoder should use buffer-compression subrecords (minimum run length);
- concrete timer values (the RFCs define an "inactivity timer" but no duration);
- which production partners accept non-TLS OFTP today (only one OEM page fetched, see 9).

---

## 1. Protocol architecture

### 1.1 Layering
OFTP is modelled on OSI layers 4-7 over a network service (s1.4, s2). Over TCP every
ODETTE Exchange Buffer (OEB) is prefixed with a Stream Transmission Header (STH); together they
form a Stream Transmission Buffer (STB) (s8). The STH is not used over X.25.

### 1.2 Stream Transmission Header (identical in 1.3 / 1.4 / 2.0) -- s8.2, 2204 s7.2
```
 byte 0          byte 1..3
+-------+-------+------------------------------------------------+
|Version| Flags | Length (24 bits, unsigned, network byte order) |
| 0001  | 0000  |                                                |
+-------+-------+------------------------------------------------+
```
- byte 0 = X'10' (version 1 in the high nibble, flags 0 in the low nibble).
- Length = length of the whole STB = 4 + OEB length. Range 5..100003 (OEB 1..99999).
- Verified from bytes: the first STB of 2204 Appendix A starts `10 00 00 84` = 132 = 4 + 128-octet
  Data Exchange Buffer (decoded with a script from the hex dump).
- Receiver: read 4 bytes, check X'1' version nibble, read Length-4 bytes. A length that disagrees
  with what the command code implies -> ESID reason 07 (s5.3.11). Robustness principle applies
  (s8.2: "conservative in what is sent and liberal in what is accepted").
- One OEB contains exactly one command; commands and data never mixed; commands cannot be
  compressed (s5.2).

### 1.3 Ports -- s2.4, 2204 s2.3, IG s1.18
- 3305/tcp `odette-ftp` -- plain (1.3 RFC and 2.0 RFC both).
- 6619/tcp `odette-ftps` -- OFTP over TLS (2.0 only). TLS is layered *under* OFTP exactly like TCP
  (s2.3): the responder acts as TLS server, initiator as TLS client, SSRM is sent after the handshake.
  Nothing in the OFTP byte stream differs between 3305 and 6619.
- Caller uses an ephemeral source port (IG s1.18).

### 1.4 Roles
- **Initiator** = the side that opened the connection; **Responder** = the side that accepted (s4.2.1).
- **Speaker** / **Listener**: the protocol is strictly half-duplex flip-flop (s9.3). At the end of
  session setup the Initiator is the first Speaker (s3.3.4.1, s4.3.1). Only the Speaker sends
  SFID/DATA/EFID/EERP/NERP/CD/ESID(normal); the Listener sends SFPA/SFNA/CDT/EFPA/EFNA/RTR.
  Either side may send ESID with an error reason at any time (s3.4.2).
- Turn exchange (s3.3.4.2):
  1. after a *failed* end of file (EFNA) the turn is never exchanged;
  2. after a successful EFID the Listener may answer EFPA with EFPACD='Y'; the Speaker then MUST send
     CD and becomes Listener (Speaker table 2, transition C: EFPA(CD=Y) -> output CD, state IDLELI);
  3. the Speaker may send CD voluntarily when idle;
  4. loop prevention: a Speaker that received an *unsolicited* CD may not answer with CD; it may only
     send files/EERPs/NERPs or end the session (ESID). If the CD was *solicited* via EFPA(CD=Y) it may
     immediately give the turn back with CD (s3.3.4.2 rule 4; 1.3 just says "it is an error to send
     F_CD_RQ immediately after having received a F_CD_IND").
- **Session end** (s3.4.1, s4.6.1): the Speaker may send ESID reason 00 *only* immediately after
  receiving an unsolicited CD (state IDLESPCD) -- i.e. the partner has nothing more to send. Sending
  an EERP/NERP leaves that special state again (s3.3.5). Erratum 3453 adds: after EFPA(CD=N) the
  Speaker may send SFID, EERP/NERP, CD, or ESID with an error; to end normally it MUST send CD and
  let the partner end the session.
- After ESID the sender waits for the TCP close (WF_NDISC); both sides issue a close (s2.2.3).

### 1.5 Phases and flows (s4)
```
Start Session      I <-SSRM-- R          ("ODETTE FTP READY ")
                   I --SSID-> R
                   I <-SSID-- R          (negotiated values)
  [2.0, if SSIDAUTH=Y both sides]
                   I --SECD-> R ; I <-AUCH-- R ; I --AURP-> R
                   I <-SECD-- R ; I --AUCH-> R ; I <-AURP-- R
Start File         S --SFID-> L ; S <-SFPA-- L   or  S <-SFNA-- L (turn kept)
Data Transfer      S --DATA-> L  (x credit) ; S <-CDT-- L ; ...
End File           S --EFID-> L ; S <-EFPA(CD=Y|N)-- L  or  S <-EFNA-- L
End to End         S --EERP-> L ; S <-RTR-- L ; S --NERP-> L ; S <-RTR-- L ...
Change direction   S --CD-> L   (roles swap)
End Session        S --ESID(00)-> L      (only right after an unsolicited CD)
```
After EERP/NERP the Speaker MUST wait for RTR and may then only send EERP, NERP, or (when no more
responses are pending) SFID or CD (s4.3.8). RTR has no end-to-end meaning.

### 1.6 Exchange buffer size and credit window (s5.3.2, s4.4, s9)
- **SSIDSDEB** (Data Exchange Buffer size) 9(5), min 128, max 99999; counts the command octet,
  excludes the STH. Each side offers its maximum; the smaller is used (responder answers <= offered).
  Every OEB (commands too) must fit. Note: 2.0 commands with UTF-8 text can reach 1000+ octets
  (SFID with 999-octet description = 1164); with SDEB below that a sender must truncate the
  optional text. [The RFC does not say this explicitly; it is a consequence of "the largest
  exchange buffer that can be accepted".]
- **SSIDCRED** 9(3), 1..999: number of DATA buffers the Speaker may send before it must wait for
  CDT. The responder MUST answer a value <= the initiator's, otherwise protocol error (s5.3.2).
- Mechanics (state tables 9.10-9.12):
  - SFPA resets the Speaker's credit to the window (implicit CDT; action 12).
  - Speaker decrements per DATA; at 0 it enters OPOWFC and waits for CDT.
  - **EFID may not be sent in OPOWFC** (s9.11.3 note 1) -- i.e. if the last DATA used up the
    credit, the Speaker must wait for a CDT before it may send EFID.
  - EFID zeroes the credit.
  - Listener counts down; when its counter reaches 0 with the last DATA it sends CDT (transition I,
    predicate P7); a DATA beyond the credit is a protocol error -> ESID 02 (P6).
  - CDT outside OPOWFC is a protocol error (Speaker table 1, column OPO: CDT -> P).
- Memory consequence for MVS: the Listener never needs `credit x SDEB` octets of application
  buffer -- over TCP the unread buffers sit in the socket/TCP receive window. One OEB buffer of
  SDEB+4 octets per direction suffices. A small SDEB (e.g. 2048, the value used in [TC] 1.3.1)
  and moderate credit (e.g. 7..99) are fine; credit mainly affects throughput.

### 1.7 Version (release level) negotiation
- SSIDLEV values (s5.3.2): '1' = 1.2, '2' = 1.3, '4' = 1.4, '5' = 2.0. **There is no '3'.**
  RFC 2204 (1.3) itself sends '1'; the 1.4 draft and RFC 5024 say '2' is 1.3. In practice a level
  '1' or '2' peer speaks the 1.3 layout. [No primary source for genuine 1.2 exists; treat 1 and 2
  identically.]
- "The protocol release level is negotiable, with the lowest level being selected" (s5.3.2,
  1.4 s5.3.2 "negotiable in the same way as CREDIT VALUE"). Erratum 3454: responder answers
  level n <= m; the initiator may reject with ESID '10' (mode or capabilities incompatible).
- IG s2.1: an OFTP2 product that cannot/may not use level 5 **MUST NOT simply disconnect** when
  offered 5; it MUST try to negotiate 1..4; the caller then accepts or sends ESID 10.
  [TC] 1.3.2 tests exactly this (initiator 5, responder 4 or less).
- The SSID itself is 61 octets at every level, so the responder can parse the initiator's SSID
  before it knows the outcome. From the responder's SSID onwards, **all commands use the layout
  of the negotiated level** (an OFTP2 engine talking at level 4 sends 128-octet SFIDs, 22-octet
  EFIDs, etc.).

### 1.8 Restart -- see section 8.

---

## 2. Command formats (all versions)

Conventions (s5.1, s5.3): octet unit; command fields ISO 646 IRV 7-bit (Appendix B subset) except
fields marked T(n) (UTF-8) and U(n) (binary, unsigned, network byte order). X(n) alphanumeric:
left-justified, space padded. 9(n) numeric: right-justified, zero padded. Reserved: spaces.
"String" charset: `0-9 A-Z / - . & ( )` and space; **space not allowed embedded**. Variable-length
2.0 fields may be omitted with length 0 (s5.2).

Command identifiers (unchanged across versions):

| Cmd | Code | ASCII hex | Dir | Exists in |
|---|---|---|---|---|
| SSRM | `I` | 49 | R->I | all |
| SSID | `X` | 58 | both | all |
| SFID | `H` | 48 | S->L | all |
| SFPA | `2` | 32 | L->S | all |
| SFNA | `3` | 33 | L->S | all |
| DATA | `D` | 44 | S->L | all |
| CDT | `C` | 43 | L->S | all |
| EFID | `T` | 54 | S->L | all |
| EFPA | `4` | 34 | L->S | all |
| EFNA | `5` | 35 | L->S | all |
| ESID | `F` | 46 | either | all |
| CD | `R` | 52 | S->L | all |
| EERP | `E` | 45 | S->L | all |
| NERP | `N` | 4E | S->L | 1.4, 2.0 |
| RTR | `P` | 50 | L->S | all |
| SECD | `J` | 4A | both | 2.0 |
| AUCH | `A` | 41 | both | 2.0 |
| AURP | `S` | 53 | both | 2.0 |

An unknown first octet -> ESID 01 (Error state table, "Invalid Buffer" -> transition I).

Legend in the tables: **[Δ1.4]** changed in 1.4 vs 1.3; **[Δ2.0]** changed in 2.0 vs 1.4; **[new2.0]**.

### 2.1 SSRM -- Start Session Ready Message (all levels identical, 19 octets) s5.3.1
| Pos | Field | Fmt | Value |
|---|---|---|---|
| 0 | SSRMCMD | X(1) | `I` |
| 1 | SSRMMSG | X(17) | `ODETTE FTP READY ` (trailing space) |
| 18 | SSRMCR | X(1) | X'0D' or X'8D' |

### 2.2 SSID -- Start Session (61 octets at every level) s5.3.2
| Pos | Field | 1.3 [2204] | 1.4 [14] | 2.0 [5024] |
|---|---|---|---|---|
| 0 | SSIDCMD | `X` | `X` | `X` |
| 1 | SSIDLEV | 9(1) `1` | 9(1) `4` (`2`=1.3,`1`=1.2) [Δ1.4 clarified] | 9(1) `5` |
| 2 | SSIDCODE | X(25) | X(25) | X(25) Initiator's/Responder's Odette ID (5.4) |
| 27 | SSIDPSWD | X(8) | X(8) | X(8) |
| 35 | SSIDSDEB | 9(5) 128..99999 | same | same |
| 40 | SSIDSR | X(1) S/R/B | same | same |
| 41 | SSIDCMPR | X(1) Y/N | same | same ("buffer compression") |
| 42 | SSIDREST | X(1) Y/N | same | same |
| 43 | SSIDSPEC | X(1) "N only valid for TCP" | same | X(1) Y/N; "not supported for TCP/IP" -> send N |
| 44 | SSIDCRED | 9(3) 1..999 | same | same |
| 47 | SSIDRSV1 X(5) | reserved | reserved | **SSIDAUTH X(1) Y/N** [new2.0] |
| 48 | -- | (part of RSV1) | (part of RSV1) | SSIDRSV1 X(4) |
| 52 | SSIDUSER | X(8) bilateral, spaces if unused | same | same |
| 60 | SSIDCR | X'0D'/X'8D' | same | same |

Notes:
- SSIDCODE in the initiator's SSID identifies the initiator; in the responder's SSID the responder
  puts its own ID (the field is "Initiator's Identification Code" in the table title, but each side
  sends its own identity -- the state tables build O.SSID "from local variables" in both directions;
  [twdes] does the same). Password likewise: each side sends its own password, checked by the peer
  -> ESID 03 (unknown code) / 04 (invalid password).
- A 1.x peer has a space at offset 47; treat as SSIDAUTH = N. At negotiated level <= 4, send space.
- SSIDSR: error if both S or both R (-> ESID 10). B may be narrowed by the responder.
- SSIDCMPR/SSIDREST/SSIDSPEC: used only if both say Y; responder answers Y only if the initiator
  said Y and it supports it.
- SSIDAUTH: **not negotiable**; if p != q the initiator aborts with ESID 12 (s5.3.2; state table
  9.8 transition D: `P2 & !P8 -> ESID(R=12)`); the responder sends ESID 12 if its user's answer
  differs from the request (transition G, `!P10`).
- There is no "SSIDSEC" field (a common misreading); the session-level security flag is
  SSIDAUTH. File-level security is per file in SFIDSEC.

### 2.3 SFID -- Start File
| Pos | 1.3 [2204 s5.3.3] | 1.4 [14 s5.3.3] | 2.0 [5024 s5.3.3] |
|---|---|---|---|
| 0 | SFIDCMD `H` | `H` | `H` |
| 1 | SFIDDSN X(26) | X(26) | X(26) |
| 27 | SFIDRSV1 X(9) | SFIDRSV1 X(3) [Δ1.4] | SFIDRSV1 X(3) |
| 30 | (RSV1) | SFIDDATE 9(8) CCYYMMDD [Δ1.4] | SFIDDATE 9(8) CCYYMMDD |
| 36 | SFIDDATE X(6) YYMMDD | (DATE) | (DATE) |
| 38 | (DATE) | SFIDTIME 9(10) HHMMSScccc [Δ1.4] | SFIDTIME 9(10) HHMMSScccc |
| 42 | SFIDTIME X(6) HHMMSS | (TIME) | (TIME) |
| 48 | SFIDUSER X(8) | X(8) | X(8) |
| 56 | SFIDDEST X(25) | X(25) | X(25) |
| 81 | SFIDORIG X(25) | X(25) | X(25) |
| 106 | SFIDFMT X(1) F/V/U/T | same | same |
| 107 | SFIDLRECL 9(5) | 9(5) | 9(5) |
| 112 | SFIDFSIZ 9(7) (1K blocks) | 9(7) | SFIDFSIZ 9(13) transmitted size, 1K blocks [Δ2.0] |
| 119 | SFIDREST 9(9) | 9(9) | (FSIZ) |
| 125 | -- | -- | SFIDOSIZ 9(13) original size, 1K blocks [new2.0] |
| 138 | -- | -- | SFIDREST 9(17) [Δ2.0] |
| 155 | -- | -- | SFIDSEC 9(2) 00/01/02/03 [new2.0] |
| 157 | -- | -- | SFIDCIPH 9(2) 00/01.. [new2.0] |
| 159 | -- | -- | SFIDCOMP 9(1) 0/1 [new2.0] |
| 160 | -- | -- | SFIDENV 9(1) 0/1 [new2.0] |
| 161 | -- | -- | SFIDSIGN X(1) Y/N [new2.0] |
| 162 | -- | -- | SFIDDESCL 9(3) 0..999 [new2.0] |
| 165 | -- | -- | SFIDDESC T(n) UTF-8 [new2.0] |
| len | **128** | **128** | **165 + n** |

Field semantics:
- DSN: by bilateral agreement, no structure (s5.3.3). (DSN, date, time) assigned by the originator
  uniquely identify a virtual file and must not be changed by intermediates (s1.5.2).
  Time counter cccc = 0001..9999 for sub-second uniqueness.
- In 1.3 the date/time fields are `X(6)` strings, in 1.4/2.0 `9(n)` numerics.
- DEST = final recipient (it creates the EERP); ORIG = originator (who mapped the data).
- FMT: F fixed, V variable, U unstructured, T text. Once signed/compressed/encrypted the transmitted
  file is U regardless; SFIDFMT then states the *original* format (s5.3.3).
- LRECL: max user-data record length; must be 00000 for T and U. If V and the file is
  compressed/encrypted/signed, max '65536' (s5.3.3) [note: the 2-octet length prefix of s6.5 can
  only express 65535 -- inconsistency in the RFC].
- FSIZ/OSIZ: estimates; OSIZ = FSIZ if no file services; 0 if unknown.
- REST: restart position (section 8); 0 if restart not agreed.
- SEC: 00 none, 01 encrypted, 02 signed, 03 both. CIPH: 00 none, else cipher suite (s10.2, IG s2.9).
  COMP: 0 none, 1 ZLIB. ENV: 0 none, 1 CMS. SIGN: Y = EERP must be signed, N = must not be signed.
- DESC: UTF-8, max 999 octets, bilateral meaning. IG v2.4 s1.2 presents it as the "extended file
  name ... coded as a variable length UTF 8 character string, in order to support Asian, Arabian...
  file names" -- partners may put the real file name here. The virtual file name proper (SFIDDSN)
  stays X(26) at every level; there is no longer DSN field in any version.
- Certificate exchange files (IG s2.5): DSN `ODETTE_CERTIFICATE_REQUEST` / `_DELIVER` / `_REPLACE`,
  FMT U, SEC 00, CIPH 00, COMP 0, ENV 0 (IG table; s10.4 says ENV may be 1), SIGN N. Note these DSNs
  contain `_`, which is outside the X(n) character set -- accept liberally.

### 2.4 SFPA -- Start File Positive Answer
| Pos | 1.3 / 1.4 | 2.0 |
|---|---|---|
| 0 | `2` | `2` |
| 1 | SFPAACNT 9(9) | SFPAACNT 9(17) [Δ2.0] |
| len | 10 | 18 |

SFPAACNT <= SFIDREST; 0 if no restart. Greater -> Speaker sends ESID 02 (s9.10.3 P2).

### 2.5 SFNA -- Start File Negative Answer
| Pos | 1.3 / 1.4 | 2.0 |
|---|---|---|
| 0 | `3` | `3` |
| 1 | SFNAREAS 9(2) | 9(2) |
| 3 | SFNARRTR X(1) Y/N | X(1) |
| 4 | -- | SFNAREASL 9(3) [new2.0] (ASCII digits, not binary) |
| 7 | -- | SFNAREAST T(n) UTF-8, max 999 [new2.0] |
| len | 4 | 7 + n |

Turn is kept by the Speaker; it may try the next file. Retry=Y: retry in a later session.

### 2.6 DATA (all levels) s5.3.6 / section 4 of this doc
`D` + data exchange buffer payload (subrecords). Length 2..SDEB.

### 2.7 CDT -- Set Credit (all levels, 3 octets)
`C` + CDTRSV1 X(2) (spaces).

### 2.8 EFID -- End File
| Pos | 1.3 / 1.4 | 2.0 |
|---|---|---|
| 0 | `T` | `T` |
| 1 | EFIDRCNT 9(9) | EFIDRCNT 9(17) [Δ2.0] |
| 10 | EFIDUCNT 9(12) | (RCNT) |
| 18 | -- | EFIDUCNT 9(17) [Δ2.0] |
| len | 22 | 35 |

RCNT = exact record count for F/V, zeros for U/T. UCNT = exact number of octets of user data
("real size of the file", before buffer compression, headers excluded). For T files that is the
size of the *virtual* file, i.e. including the CR-LF pairs the MVS sender inserts (and excluding
any trailing blanks it strips) -- counting MVS record bytes instead yields EFNA 11 (invalid byte
count) on the first T-file test. **Totals for the whole file
even after a restart.**

### 2.9 EFPA -- End File Positive Answer (all levels, 2 octets)
`4` + EFPACD X(1) Y/N (Y = Listener wants the turn).

### 2.10 EFNA -- End File Negative Answer
| Pos | 1.3 / 1.4 | 2.0 |
|---|---|---|
| 0 | `5` | `5` |
| 1 | EFNAREAS 9(2) | 9(2) |
| 3 | -- | EFNAREASL 9(3) [new2.0] |
| 6 | -- | EFNAREAST T(n) [new2.0] |
| len | 3 | 6 + n |

### 2.11 ESID -- End Session
| Pos | 1.3 / 1.4 | 2.0 |
|---|---|---|
| 0 | `F` | `F` |
| 1 | ESIDREAS 9(2) | 9(2) |
| 3 | ESIDCR | ESIDREASL 9(3) [new2.0] |
| 6 | -- | ESIDREAST T(n) [new2.0] |
| 6+n | -- | ESIDCR X'0D'/X'8D' (moved behind the text) |
| len | 4 | 7 + n |

### 2.12 CD -- Change Direction (all levels, 1 octet): `R`.

### 2.13 EERP -- End to End Response
| Pos | 1.3 [2204] | 1.4 [14] | 2.0 [5024] |
|---|---|---|---|
| 0 | `E` | `E` | `E` |
| 1 | EERPDSN X(26) | X(26) | X(26) |
| 27 | EERPRSV1 X(9) | EERPRSV1 X(3) [Δ1.4] | X(3) |
| 30 | (RSV1) | EERPDATE 9(8) [Δ1.4] | 9(8) |
| 36 | EERPDATE X(6) | | |
| 38 | | EERPTIME 9(10) [Δ1.4] | 9(10) |
| 42 | EERPTIME X(6) | | |
| 48 | EERPUSER X(8) | X(8) | X(8) |
| 56 | EERPDEST X(25) | X(25) | X(25) |
| 81 | EERPORIG X(25) | X(25) | X(25) |
| 106 | -- | -- | EERPHSHL U(2) binary [new2.0] |
| 108 | -- | -- | EERPHSH U(h) [new2.0] |
| 108+h | -- | -- | EERPSIGL U(2) binary [new2.0] |
| 110+h | -- | -- | EERPSIG U(s) CMS SignedData [new2.0] |
| len | 106 | 106 | 110 + h + s (unsigned: 110) |

- **DEST/ORIG are reversed relative to SFID**: EERPDEST = originator of the virtual file (= SFIDORIG),
  EERPORIG = final recipient that creates the EERP (= SFIDDEST). DSN/date/time/user copied from the SFID.
- The 1.4 draft's EERP table labels positions 27/30/38 as "SFIDRSV1/SFIDDATE/SFIDTIME" -- a copy
  typo in the draft; the field descriptions below the table name them EERPRSV1/EERPDATE/EERPTIME.
- HSHL = 0 "is always the case if the EERP is not signed" (s5.3.13). An unsigned 2.0 EERP therefore
  ends with two binary zero pairs: `00 00 00 00`.
- Signature = CMS SignedData (encapsulated id-data) over DSN||DATE||TIME||DEST||ORIG||HSH, fields
  taken in full including padding; the envelope must contain the data (s5.3.13).
- Hash = hash of the *transmitted* virtual file, algorithm per SFIDCIPH cipher suite.

### 2.14 NERP -- Negative End Response (1.4 and 2.0)
| Pos | 1.4 [14 s5.3.15] | 2.0 [5024 s5.3.14] |
|---|---|---|
| 0 | `N` | `N` |
| 1 | NERPDSN X(26) | X(26) |
| 27 | NERPRSV1 X(6) | X(6) |
| 33 | NERPDATE 9(8) | 9(8) |
| 41 | NERPTIME 9(10) | 9(10) |
| 51 | NERPDEST X(25) (= VF originator) | X(25) |
| 76 | NERPORIG X(25) (= VF final recipient) | X(25) |
| 101 | NERPCREA X(25) creator of NERP | X(25) |
| 126 | NERPREAS 9(2) | 9(2) |
| 128 | -- | NERPREASL 9(3) (ASCII digits) [new2.0] |
| 131 | -- | NERPREAST T(n) [new2.0] |
| 131+n | -- | NERPHSHL U(2), NERPHSH U(h), NERPSIGL U(2), NERPSIG U(s) [new2.0] |
| len | 128 | 135 + n + h + s |

NERP has no USER field (RSV1 is 6 wide instead). Not available at level 1/2: a 1.3 peer cannot
receive it (an intermediate must hold the information or translate otherwise -- outside the protocol).

### 2.15 RTR -- Ready To Receive (all levels, 1 octet): `P`.

### 2.16 SECD / AUCH / AURP (2.0 only, s5.3.16-18)
- SECD: `J` (1 octet).
- AUCH: `A`, AUCHCHLL U(2) @1, AUCHCHAL U(n) @3 = CMS EnvelopedData encrypting a 20-octet random
  challenge with the peer's certificate public key.
- AURP: `S`, AURPRSP U(20) @1 = decrypted challenge. Length 21.
- Sequence (s4.2.4): I-SECD->, <-AUCH, I-AURP->, <-SECD, I-AUCH->, <-AURP. Mismatch -> ESID 11.

### 2.17 Length summary (OEB length incl. command octet, excl. STH)
| Cmd | 1.3 | 1.4 | 2.0 |
|---|---|---|---|
| SSRM | 19 | 19 | 19 |
| SSID | 61 | 61 | 61 |
| SFID | 128 | 128 | 165+desc |
| SFPA | 10 | 10 | 18 |
| SFNA | 4 | 4 | 7+text |
| CDT | 3 | 3 | 3 |
| EFID | 22 | 22 | 35 |
| EFPA | 2 | 2 | 2 |
| EFNA | 3 | 3 | 6+text |
| ESID | 4 | 4 | 7+text |
| CD, RTR, SECD | 1 | 1 | 1 |
| EERP | 106 | 106 | 110+hash+sig |
| NERP | -- | 128 | 135+text+hash+sig |
| AUCH / AURP | -- | -- | 3+n / 21 |

So the per-level differences are: SFID date/time/reserved (1.3 vs 1.4), the widened numeric counts
(SFID FSIZ/REST, SFPA, EFID) and the 2.0 tails (text length + UTF-8 text, hash/signature length
pairs, SFID security fields). A table-driven encoder/decoder with a level switch covers all three.

---

## 3. Negotiation, mandatory vs optional features (OFTP 2.0)

### 3.1 What a responder MUST do per initiator level
| Initiator offers | We are a 2.0-capable responder | Layout afterwards |
|---|---|---|
| '1' or '2' (1.3) | answer with the same value (1 or 2) -- lowest wins | 1.3: 6-digit dates, no NERP, short counts |
| '4' | answer '4' | 1.4: CCYYMMDD/HHMMSScccc, NERP allowed, short counts |
| '5' | answer '5' (or a lower level if 2.0 is disabled for this partner; never just disconnect, IG s2.1) | 2.0 |
| '3' or >5 | not defined. Liberal choice: answer min(5, offered) and let the initiator decide; '3' does not exist, so ESID 10 or answer '2' are both defensible. [not specified] |

As initiator: offer the partner's configured level; if the answer is lower, accept it (or ESID 10
if policy forbids, erratum 3454).

Other SSID negotiation checks (s9.8.3 P2): buffer size, restart, compression, mode, special logic,
window must be compatible; "any incompatibilities result in failure of the negotiation" -> ESID 10.
Authentication mismatch -> ESID 12. Unknown ID -> 03, bad password -> 04, no resources -> 08.

### 3.2 Feature switches in 2.0 and their "off" value
| Feature | Where negotiated | Off value | Can we refuse? |
|---|---|---|---|
| TLS | port choice (6619) | use 3305 | yes, by not listening on 6619 / not calling it |
| Secure authentication | SSIDAUTH | N | yes; if the peer requires Y the session fails with ESID 12 (not negotiable) |
| Buffer compression | SSIDCMPR | N | yes (both must say Y) |
| Special logic | SSIDSPEC | N | must be N on TCP |
| Restart | SSIDREST | N | yes |
| File encryption/signing | SFIDSEC / SFIDCIPH | 00 / 00 | yes: SFNA/EFNA 15 cipher not supported, 16 encrypted not allowed, 19 signed not allowed (retry N). Note 17/20 exist for the reverse policy (unencrypted/unsigned not allowed). |
| File compression (CMS+zlib) | SFIDCOMP | 0 | yes: SFNA 18 compression not allowed |
| CMS envelope | SFIDENV | 0 | yes (SFNA 18/16/19 or 99) |
| Signed EERP/NERP | SFIDSIGN | N | **yes, without refusing the file**: "If it is not possible to sign the EERP, then an unsigned EERP should still be sent" (s3.3.5 note 3; s4.3.6; s4.3.7 for NERP). Acceptance of an unsigned EERP is the originator's application decision. As *sender* we simply always send SFIDSIGN=N. |
| Virtual file description | SFIDDESCL | 000 | we may send 0 and store/ignore received descriptions |
| Reason texts | *REASL | 000 | send 0; receive as opaque UTF-8 |
| File size > 9 999 999 K | SFIDFSIZ 9(13) | -- | parse as 64-bit; refuse with SFNA 06 if too big for us |
| Signed broadcast | -- | -- | intermediate may ignore SFIDSIGN=Y or NERP (s4.3.6) |

### 3.3 TLS -- what the RFC says vs what Odette says
- RFC 5024 s1.7 / s2.3 / s11: TLS is one of several optional security services ("All, some, or none
  of the security options available in ODETTE-FTP may be used"); port 3305 stays defined for plain
  OFTP 2. The RFC does not require TLS.
- **Odette's guideline does**: IG v2.4 s2.4: "The OFTP2 RFC enforces the usage of TLS. ... TLS
  session encryption is mandatory within OFTP2." and "Any compliant software solution must offer the
  ability to use client authentication for TLS." This contradicts the RFC text but is the
  industry's view. Yet the same IG keeps "T4.1 TLS Y/N" as a partner parameter and port 3305 "for V1
  compatibility" (IG s1.16, s1.18), and allows TLS to be "managed by a gateway in a DMZ"
  (IG s1.6 D.1), with the gateway responsible for the certificate-to-partner binding.
- RFC 8996 deprecates TLS 1.0/1.1 for everything including RFC 5024; [TC] 4.4.1 tests TLS 1.2,
  4.6.4.x require DHE/PFS rules and client-certificate validation.

### 3.4 Cryptographic requirements if any security feature is used (s10.2, IG s2.9)
| Suite | Symmetric | Asymmetric | Hash |
|---|---|---|---|
| 01 | 3DES_EDE_CBC_3KEY | RSA_PKCS1_15 | SHA-1 |
| 02 | AES_256_CBC | RSA_PKCS1_15 | SHA-1 |
| 03 | 3DES_EDE_CBC_3KEY | RSA_PKCS1_15 | SHA-256 (IG) |
| 04 | AES_256_CBC | RSA_PKCS1_15 | SHA-256 (IG) |
| 05 | 3DES_EDE_CBC_3KEY | RSA_PKCS1_15 | SHA-512 (IG) |
| 06 | AES_256_CBC | RSA_PKCS1_15 | SHA-512 (IG) |

"Support of all cipher suites listed here [01, 02] is mandatory" (s10.2). Every security feature
(AUCH, file signing/encryption, signed EERP/NERP) needs RSA + X.509 + CMS (ASN.1 DER) -- none
available on our target. Blowfish is not in any suite. SHA-256 alone is useless without RSA
(the EERP hash is only sent with a signature).

### 3.5 Can a level-5 implementation run with everything off? -- Yes, per the RFC
SSIDAUTH=N, SSIDCMPR=N (or Y), SFIDSEC=00, SFIDCIPH=00, SFIDCOMP=0, SFIDENV=0, SFIDSIGN=N,
SFIDDESCL=0, unsigned EERP/NERP (HSHL=SIGL=0), plain 3305. This is precisely the configuration of
[TC] test 1.3.1 (SSIDLEV 5, SDEB 2048, SR B, CMPR N, REST Y, CRED 99, AUTH N) and tests 1.4/1.5
(file U, no security, EERP). What it cannot do is pass the rest of the Odette test suite (section 9).
Incoming files with SFIDENV=1 can still be *accepted and stored opaquely* (a CMS file is just U data
to the protocol engine) and handed to off-host tooling; the only obligation is the EERP, which may be
unsigned.

---

## 4. Record formats, data exchange buffer, character sets

### 4.1 Data Exchange Buffer (identical in all versions) s7, 2204 s6
```
'D' | HDR | subrecord | HDR | subrecord | ...
HDR (1 octet):  bit0 (X'80') End-of-Record flag: the following subrecord ends the current record
                bit1 (X'40') Compression flag: the following subrecord is 1 octet, repeated COUNT times
                bits2-7 (X'3F') COUNT 0..63: octets of the virtual file represented
```
- Uncompressed subrecord: HDR + COUNT data octets. Compressed: HDR + 1 octet.
- A subrecord is never split across buffers; records may span buffers and multiple records may share
  a buffer. Encoder may either truncate the buffer early or split the subrecord into two (s7.3).
- Zero-length records and zero-length subrecords may appear anywhere (blank line in 2204 App. A is a
  single X'80').
- Records of any length (> 63) are a chain of subrecords with EoR only on the last one.

### 4.2 Buffer compression (SSIDCMPR=Y) -- the complete algorithm
That bit-1 run-length scheme **is** the whole OFTP buffer compression (s7.1 "A simple compression
scheme is defined for strings of repeated characters"; 2204 s6.1 says the format is based on NIFTP).
Decoder: for HDR with X'40' set, emit the next octet COUNT times (COUNT 0..63), honour EoR.
Encoder: free to choose when to compress; any run of 2..63 identical octets may be coded as 2 octets.
- Verified in 2204 App. A: `45 20` = 5 spaces of indentation (compressed, no EoR), followed by
  `A6 ...` = 38 octets with EoR.
- [twdes] compresses runs of more than 4 identical octets (`numberOfRepeatingCharacter = 4`);
  no source states a minimum, so a run threshold is a local choice (>= 3 is break-even).
- Compression is only legal if both SSIDs said Y. Counts (SFIDREST, EFIDUCNT) are always of
  *uncompressed* user data.
- Cost on MVS: trivial (no tables, no window). Recommended to implement both directions.
- s6.4: CMS/zlib file compression "essentially deprecates" buffer compression, which "is kept for
  backwards compatibility" -- but it remains part of 2.0.

### 4.3 Virtual file formats (s1.5.3, s5.3.3)
| Fmt | Meaning | Record mapping in DATA | SFIDLRECL | EFIDRCNT | Restart unit |
|---|---|---|---|---|---|
| F | fixed records, all same length | each record = subrecords, EoR on last | record length | exact count | record number |
| V | variable records | as F | max record length | exact count | record number |
| U | unstructured stream | the whole file is **one** record: EoR appears once, on the last subrecord, acting as end-of-file | 00000 | 0 | 1K blocks |
| T | text: ASCII, no control characters except CR-LF line delimiters, lines <= 2048 chars | **like U**: CR LF are carried inside the data, EoR only on the final subrecord | 00000 | 0 | 1K blocks |

T-format verified from bytes: decoding the hex dump of 5024 Appendix A (Rime of the Ancient Mariner
as T file) gives one 821-octet buffer of twelve `3F` headers (63 octets, no EoR) and a final `B3`
(EoR, 51 octets); `0D 0A` sequences sit inside the subrecord data and no compression is used. So the
receiver of a T file gets an ASCII byte stream with CRLF and must itself split lines for an MVS
record-oriented data set; the sender must insert CRLF after each record (and should strip trailing
blanks of fixed-length text records -- local policy).

V-format with file services (s6.5): before signing/compressing/encrypting, each record gets a 2-octet
big-endian length prefix (data length, excluding the prefix); the transmitted file is then U.

### 4.4 Character sets
- **Command buffers are ISO 646 IRV (= ASCII graphic subset, Appendix B) on the wire, in every
  version and on every network.** Only the command code and fields; UTF-8 fields (2.0 text/DESC) and
  binary fields (U(n)) are exempt. An EBCDIC host must translate every X(n)/9(n) field both ways.
  There is no EBCDIC variant of the protocol (the X.25 heritage is the same ISO 646 encoding; CR may
  arrive as X'0D' or with parity bit X'8D').
- Note: X'0D' is CR in both ASCII and EBCDIC, so a missing translation of SSRMCR/SSIDCR goes unnoticed
  while the rest of the command is garbage -- test with a byte-level trace.
- Allowed field characters (Appendix B / s5.3): `0-9 A-Z space & ( ) - . /`. Lower case is not in the
  set. MVS names using `@ # $` cannot appear in OFTP identifiers.
- **Data (F/V/U)** is binary and transferred unchanged; no protocol rule says who converts. Whether
  F/V records are EBCDIC or ASCII is a bilateral agreement (the virtual-file mapping is "not defined
  here", s1.5). In practice most EDI partners expect ASCII/ISO 8859 payloads; an MVS server needs a
  per-partner/per-file translation option (EBCDIC<->ISO 8859-1/CP037) applied by the mapping layer,
  never by the protocol engine. [practice statement: not verified from a primary source]
- **T** is defined as ASCII + CRLF (s1.5.3); an MVS implementation must translate T data to/from
  EBCDIC and convert CRLF <-> record boundaries. No code page beyond "ASCII" is specified (national
  characters in T files are a bilateral matter).
- UTF-8 fields: do not translate; send length 0 or ASCII-only text (valid UTF-8), store received text
  as opaque bytes (or translate only the 7-bit subset for logging).

---

## 5. EERP / NERP end-to-end responses (s3.3.5, s3.3.6, s4.3.6-4.3.8)

- EERP: generated by the **final destination** (SFIDDEST) after successful receipt and processing,
  addressed to the originator; "The Response is mandatory and may be sent in the same or in any
  subsequent session" (s4.3.6). Lets the originator delete its copy.
- Routing: EERPs travel back hop by hop through clearing centres/VANs like files. An **intermediate
  that broadcasts or collects** must wait for EERPs from all locations it forwarded to before sending
  its own EERP (s4.3.6 examples: distribution, collection) -> intermediates must track file state over
  time. An intermediate that merely relays to one destination forwards the destination's EERP.
- Signed EERP is incompatible with broadcast; the broadcast node may ignore SFIDSIGN=Y or send NERP.
- NERP (1.4+): sent when a file cannot be delivered to the next hop for a **non-temporary** reason
  (SFNA/EFNA with retry=N, ESID 03/04/...), or by the final destination when it received the file but
  cannot process it (2.0 adds this case, s3.3.6). Intermediate then stops retrying and may delete the
  file. NERPREAS maps the refusal reason (table 7.5). NERPCREA names the creating node.
- Both are sent only by the current Speaker, with no file open, each acknowledged by RTR before the
  next command; they never change the turn (s3.3.5).
- Matching key at the originator: (DSN, date, time, originator, destination) -- EERPDEST/EERPORIG
  reversed vs SFID.
- 1.3 partners have no NERP; for them, a node can only withhold the EERP. [what an intermediate does
  with a NERP it cannot forward to a 1.3 originator is not specified]
- 2.0 NERP reason 90 "File size incompatible with recipient's protocol version": a file > 9 999 999
  1K blocks cannot be described in a 1.x SFID.

---

## 6. Identification, passwords, user fields

### 6.1 Odette ID (SSIDCODE, SFIDDEST, SFIDORIG, NERPCREA) -- s5.4, ISO 6523 SIO, 25 octets
| Pos | Field | Fmt | Meaning |
|---|---|---|---|
| 0 | SIOOID | X(1) | `O` = ODETTE-assigned organisation ID; other values for non-ODETTE codes |
| 1 | SIOICD | 9(4) (described as String(4)) | International Code Designator (ISO 6523 ICD) |
| 5 | SIOORG | X(14) | organisation code: A-Z, 0-9, space, hyphen |
| 19 | SIOCSA | X(6) | computer sub-address, locally assigned |

Layout example (illustrative values, not a real ID): `O` + `0013` + `00000001234567` + `ABC   ` =
`O001300000001234567ABC   ` (1+4+14+6 = 25; ICD/ORG values are assigned by Odette or other
registries -- not verified). IG: "25 upper case alphanumeric characters". In practice many IDs do not follow the
SIO structure (e.g. a DUNS-based or free code padded with spaces); treat it as an opaque left-
justified X(25) and compare after trimming trailing spaces.
- SSIDCODE identifies the session partner (adjacent node); SFIDORIG/SFIDDEST identify file originator
  and final recipient, which may differ from the SSID identity (routing, mailboxes; IG s1.6 D.2).

### 6.2 SSIDPSWD
X(8), "Key to authenticate the sender. Assigned by bilateral agreement" (s5.3.2); IG: "8 upper case
alphanumeric char." Sent **in clear** unless TLS is used (s11). Each side checks the other's
password (ESID 04 on mismatch). Same width in all versions.

### 6.3 SSIDUSER / SFIDUSER / EERPUSER
X(8), free use by bilateral agreement, spaces if unused. NERP has no user field.

### 6.4 SSIDRSV1, SSIDSPEC
- SSIDRSV1: X(5)@47 in 1.x, X(4)@48 in 2.0 -- spaces.
- SSIDSPEC: special logic = X.25-PAD/asynchronous wrapper (Appendix C.2: STX, block sequence number
  0-9, ISO 8073 checksum, shift-out transparency, CR, credit forced to 1). "not supported for TCP/IP"
  (s5.3.2); RFC 2204: 'N' is the only valid value for TCP. Send N; if a peer sends Y over TCP, answer
  N (both must agree; Y-from-one-side is resolved to N by the negotiation rule).

---

## 7. Reason code tables

### 7.1 ESID reason codes (s5.3.11)
| Code | Meaning | Levels |
|---|---|---|
| 00 | Normal session termination | all |
| 01 | Command not recognised (invalid first octet) | all |
| 02 | Protocol violation (command invalid in current state) | all |
| 03 | User code not known (SSID) | all |
| 04 | Invalid password (SSID) | all |
| 05 | Local site emergency close down | all |
| 06 | Command contained invalid data | all |
| 07 | Exchange Buffer size error (STH length vs implied command length) | all |
| 08 | Resources not available (retry later) | all |
| 09 | Time out | all |
| 10 | Mode or capabilities incompatible (incl. level refusal) | all |
| 11 | Invalid challenge response | 2.0 |
| 12 | Secure authentication requirements incompatible | 2.0 |
| 99 | Unspecified abort code | all |

### 7.2 SFNA reason codes (s5.3.5)
| Code | Meaning | Levels |
|---|---|---|
| 01 | Invalid filename | all |
| 02 | Invalid destination | all |
| 03 | Invalid origin | all |
| 04 | Storage record format not supported | all |
| 05 | Maximum record length not supported | all |
| 06 | File size is too big | all |
| 10 | Invalid record count | all |
| 11 | Invalid byte count | all |
| 12 | Access method failure | all |
| 13 | Duplicate file | all |
| 14 | File direction refused | 1.4+ |
| 15 | Cipher suite not supported | 2.0 |
| 16 | Encrypted file not allowed | 2.0 |
| 17 | Unencrypted file not allowed | 2.0 |
| 18 | Compression not allowed | 2.0 |
| 19 | Signed file not allowed | 2.0 |
| 20 | Unsigned file not allowed | 2.0 |
| 99 | Unspecified reason | all |

SFNARRTR: Y = may retry later (temporary, e.g. no space; recommended for mapping problems),
N = do not retry.

### 7.3 EFNA reason codes (s5.3.10)
Same as SFNA 01-20 and 99, plus (2.0 only): 21 Invalid file signature, 22 File decryption failure,
23 File decompression failure. (1.3: 01-13, 99; 1.4 adds 14.) EFNA has no retry indicator.

### 7.4 Where codes come from
Speaker sends ESID 02 for SFPA count > SFID restart, unexpected CDT, etc.; Listener sends ESID 02 for
out-of-state commands and credit overrun; ESID 07 for buffer length mismatch (also used by special
logic CRC errors); ESID 01 for unknown command codes.

### 7.5 NERP reason codes (1.4 s5.3.15, 2.0 s5.3.14) -- "SFNA(RETRY=N)" means "EFNA or SFNA(RETRY=N)"
| Code | Meaning | Levels |
|---|---|---|
| 03 | ESID received with 03 (user code not known) | 1.4, 2.0 |
| 04 | ESID received with 04 (invalid password) | 1.4, 2.0 |
| 09 | ESID received with 99 (unspecified) | 1.4, 2.0 |
| 11 | SFNA(N) 01 invalid file name | 1.4, 2.0 |
| 12 | SFNA(N) 02 invalid destination | 1.4, 2.0 |
| 13 | SFNA(N) 03 invalid origin | 1.4, 2.0 |
| 14 | SFNA(N) 04 invalid storage record format | 1.4, 2.0 |
| 15 | SFNA(N) 05 max record length not supported | 1.4, 2.0 |
| 16 | SFNA(N) 06 file size too big | 1.4, 2.0 |
| 20 | SFNA(N) 10 invalid record count | 1.4, 2.0 |
| 21 | SFNA(N) 11 invalid byte count | 1.4, 2.0 |
| 22 | SFNA(N) 12 access method failure | 1.4, 2.0 |
| 23 | SFNA(N) 13 duplicate file | 1.4, 2.0 |
| 24 | SFNA(N) 14 file direction refused | 1.4, 2.0 |
| 25 | SFNA(N) 15 cipher suite not supported | 2.0 |
| 26 | SFNA(N) 16 encrypted file not allowed | 2.0 |
| 27 | SFNA(N) 17 unencrypted file not allowed | 2.0 |
| 28 | SFNA(N) 18 compression not allowed | 2.0 |
| 29 | SFNA(N) 19 signed file not allowed | 2.0 |
| 30 | SFNA(N) 20 unsigned file not allowed | 2.0 |
| 31 | File signature not valid | 2.0 |
| 32 | File decompression failed | 2.0 |
| 33 | File decryption failed | 2.0 |
| 34 | File processing failed | 2.0 |
| 35 | Not delivered to recipient | 2.0 |
| 36 | Not acknowledged by recipient | 2.0 |
| 50 | Transmission stopped by the operator | 1.4, 2.0 |
| 90 | File size incompatible with recipient's protocol version | 2.0 |
| 99 | Unspecified reason | 1.4, 2.0 |

---

## 8. Restart (s1.5.4, s4.3.3, s5.3.3, s5.3.4, s9.10.3, s9.12.3)

- Enabled only if both SSIDs say SSIDREST=Y. Otherwise SFIDREST and SFPAACNT must be 0; SFIDREST > 0
  without agreed restart is a protocol error (Listener P1 -> ESID 02).
- Unit: record number for F/V; 1K (1024-octet) blocks of the virtual file for U/T. Counted on
  transmitted user data *before* buffer compression, headers excluded. Files with file services
  (SFIDSEC/COMP/ENV != 0) are restarted as U regardless of SFIDFMT.
- Speaker proposes SFIDREST (what it believes was delivered); Listener answers SFPAACNT <= SFIDREST
  (what it actually has safely stored); transmission resumes at the lower value + 1
  ("The sender will start with the lowest record count + 1", s4.3.3). Listener answering more than
  proposed -> Speaker sends ESID 02.
- For U/T the RFC never spells out the arithmetic. Our reading (not stated in any source): the
  receiver reports floor(octets_stored / 1024), truncates its partial file to SFPAACNT x 1024
  octets, and the sender resumes at virtual-file octet offset SFPAACNT x 1024. For F/V the sender
  resumes with record SFPAACNT + 1 and the receiver keeps exactly SFPAACNT records.
- EFID counts are always whole-file totals.
- [TC] 4.1.6 restart test is **optional** ("only if both parties support this functionality").
- Persistent state a server needs (per virtual file, keyed by originator + DSN + date + time
  [+ destination]):
  - inbound: partial data set name, last committed record / 1K-block count, format, partner, so a
    later SFID for the same key can be answered with a non-zero SFPAACNT; commit points must be
    real (data set closed/checkpointed) before they are reported -- on MVS that means writing
    restartable data sets (e.g. QSAM append via DISP=MOD with a known record count, or a
    sequential data set rewritten up to the commit point);
  - outbound: queue of files to send, per file the position last acknowledged, and the state
    "sent, awaiting EERP/NERP" (for housekeeping and to match incoming EERPs);
  - pending inbound EERPs/NERPs to be sent to each adjacent partner in this or a later session;
  - duplicate detection history (SFNA 13) of (orig, DSN, date, time) already received;
  - for an intermediate (routing) node additionally the fan-out state per file (which downstream
    EERPs are still missing).
  Restart state is not required for a minimal implementation; EERP state is (the EERP is mandatory).

---

## 9. Real-world interoperability

### 9.1 Who still accepts plain (non-TLS) OFTP
- No primary source found that names a production partner accepting plain OFTP2 over the Internet.
- Volvo Group (fetched): OFTP2 on port 6619, "Volvo uses TLS line security (session encryption only)";
  requires a CA-issued certificate. No OFTP1 option stated.
- Ford (search snippet only, page 404 when fetched -- **unverified**): an "OFTPv1 server on port 3305"
  reachable over ENX (private automotive network).
- Odette IG s1.4 C: over ENX, confidentiality and authentication are provided at site level by ENX
  routers -- so OFTP 1.x/2 without TLS over ENX is a recognised architecture; reaching ENX requires an
  ENX-certified provider connection (IPsec), outside the scope of an MVS host.
- Secondary sources (supplier training deck) say "security ... Mandatory over Internet".
- Conclusion: production use with automotive OEMs needs TLS (and usually certificates, often EERP
  signing) -- i.e. a TLS front end and, for many OEMs, a certificate-capable stack. Plain OFTP is
  realistic for MVS<->MVS, lab/hobby use, private networks (VPN/ENX) and for partners who agree to it
  bilaterally.

### 9.2 Certification
- [TC] v2.4 (2020) lists tests that cover: SSID level 5 and level downgrade, U-file transfer, EERP,
  restart (optional), CMS 3DES/AES encryption, CMS compression, CMS signing, file description, large
  file (>100 MB), reason texts, signed EERP/NERP, TLS 1.2, OFTP authentication, automatic certificate
  exchange, TSL/CRL handling, DHE/PFS denial, client certificate validation. History 2.3.1: "the
  previously optional test cases are mandatory now"; all file formats U, T, F128, V44 are mandatory.
- The search-result description of the document says it describes the tests software "must
  successfully pass in order to obtain the certificate of 'OFTP2 Tested Software'" (secondary).
- **=> Odette OFTP2 certification is not reachable without TLS, RSA, X.509 and CMS.** An OFTP2
  wire-compatible implementation without them is still legitimate per RFC 5024.

### 9.3 Open-source / test partners
| Implementation | Lang / license | Levels | Plain 3305 | Notes |
|---|---|---|---|---|
| mendelson OFTP2 Community Edition (SourceForge) | Java, GPLv2, updated 2026-04-08 | **2.0 only**: "It does NOT work with any OFTP 1.x partner station" | yes, listens 3305 (no SSL) and 6619 | full features (TLS 1.0-1.3, CMS, signing, routing). The most practical level-5 test partner. |
| Accord Odette OFTP2 (SourceForge) | Java, AGPL, last update 2013-04-09 | 1.3, 1.4, 2.0 | presumably (lists SSL as one transport) [not tested] | framework, "File restart and compression" |
| twdes/odette (DEModette) | C#, EUPL 1.1 | SSIDLEV '2' and '5' only -- rejects '1' and '4' with ESID 10, as responder and as initiator (code: `version != Rev13 && version != Rev20`) | yes | buffer compression implemented (encoder compresses runs > 4); no CMS/gateway. Both roles simply adopt the peer's SDEB/credit (own default 99999), so a small SDEB offered by MVS is honoured. |
| thomsmits/oftp2-client | Go, BSD-3, moved to Codeberg (GitHub archived 2026-03) | 2.0 client | ? | client only |
| elgohr/go-oftp2 | Go, MIT | 2.0, incomplete (SSID, SSRM, SFID, SFPA/SFNA) | -- | hobby |
| nuveo/OFTP | Java, MIT, archived 2021 | ? | ? | |
| carthago/odette4j | Java, no license file, last commit 2021 | 2.0 command classes | ? | |
| /n software IPWorks EDI OFTPServer/Client | commercial (trial) | 1.2, 1.3, 1.4, 2.0 | plaintext is the default (UseSSL=false) | useful to test levels 1/2/4 |
| Other commercial (Cleo, Lobster, OS4X, Seeburger, TBT400 ...) | commercial | various | configurable | not evaluated |

No open-source C implementation was found. Leads to a "neubaukeramik oftp2 Java lib" and an
"OpenOFTP" could not be located.

---

## 10. Recommendation

### 10.1 Feature matrix
Feasibility column assumes MVS 3.8j with plain sockets, no TLS/RSA/X.509/CMS, SHA-256 + Blowfish
+ base64 + old zlib. Effort: S = days, M = 1-3 weeks, L = months, XL = not realistic.

| Feature | 1.3 | 1.4 | 2.0 | Feasible on MVS 3.8j? | Effort |
|---|---|---|---|---|---|
| STH framing over TCP, port 3305 | Y | Y | Y | yes | S |
| SSRM/SSID, level/buffer/credit/mode negotiation | Y | Y | Y (+SSIDAUTH) | yes | S |
| Speaker/Listener state machine, CD, ESID, RTR, timers | Y | Y | Y | yes (timer values are ours to choose) | M |
| SFID/SFPA/SFNA/EFID/EFPA/EFNA (level-dependent layouts) | 128/10/4/22/2/3 | same, new dates | 165+/18/7+/35/2/6+ | yes; needs 64-bit counters (`long long` in cc370) | S per extra level |
| DATA subrecords, F/V/U/T mapping, EBCDIC<->ASCII per file | Y | Y | Y | yes; T needs CRLF<->record mapping | M |
| Buffer compression (SSIDCMPR) | Y | Y | Y | yes, trivial RLE | S |
| Restart (SSIDREST) | Y | Y | Y | yes, needs persistent state + checkpointable data sets | M |
| EERP (unsigned) | Y | Y (CCYY dates) | Y (+`00 00 00 00` tail) | yes, mandatory, needs persistent queue | S-M |
| NERP | -- | Y | Y (+text, tail) | yes | S |
| UTF-8 text/description fields | -- | -- | Y | yes as opaque bytes, send length 0 | S |
| File size > 4 GB / 13-17 digit counts | -- | -- | Y | parse yes; such files do not exist on 3.8j DASD | S |
| Special logic (X.25 async) | X.25 only | X.25 only | X.25 only | not needed on TCP (SSIDSPEC=N) | -- |
| TLS (6619) | -- | -- | Y | **no** native stack. Possible via an external TLS gateway (stunnel/HAProxy on the Hercules host or a DMZ box) -- IG s1.6 D.1 explicitly allows "TLS managed by a gateway in a DMZ" | XL native / S as gateway |
| Secure authentication SECD/AUCH/AURP | -- | -- | Y | no (RSA private-key decrypt of CMS EnvelopedData + X.509 lookup) | XL |
| CMS file encryption/signing (SFIDSEC/CIPH/ENV) | -- | -- | Y | not in-session; files can be carried opaquely and processed off-host | XL native / S pass-through |
| CMS file compression (SFIDCOMP=1, zlib, RFC 3274 CompressedData) | -- | -- | Y | conditionally: fixed-shape DER wrapper + inflate/deflate; zlib needs ~32 KB window inflate, ~256 KB deflate -- tight but possible; old port is a risk | M-L |
| Signed EERP/NERP | -- | -- | Y | no; send unsigned (permitted) | XL |
| Certificate exchange files | -- | -- | Y | no (nothing to do with them) | -- |
| Odette OFTP2 certification | -- | -- | Y | **no** (requires TLS 1.2, CMS, auth, cert exchange) | -- |

### 10.2 Which level first -- argument
1. The SSID is 61 octets at every level and the responder sees the initiator's level before
   answering, so one engine with a per-session `level` that selects field layouts covers 1.3, 1.4
   and 2.0. The deltas are field widths (dates, counts) and opaque 2.0 tails (text-length + text,
   hash/signature length pairs, SFID security fields at fixed offsets 155-164). Implementing
   level 5 "with everything off" costs only those layouts on top of 1.4 -- small.
2. What is lost by having no TLS/RSA is the same at every level: confidentiality, certificates,
   signed receipts, certification. Falling back to 1.4/1.3 buys no security or acceptance; it only
   removes partners.
3. The accessible test partners argue for level 5: mendelson CE (most complete, free, plain 3305)
   speaks **only** level 5; twdes speaks 2 and 5 (not 4); level 4 is testable only against Accord
   (AGPL, unmaintained since 2013) or a commercial trial (/n software).
4. Legacy 1.x is what remaining non-TLS partners (ENX, MVS<->MVS, old installations) speak, and
   RFC/IG require graceful downgrade -- so levels 1/2/4 must be supported as responder anyway.

Recommended order:
- **Phase 1 (core, plain TCP):** STB framing, SSID negotiation for levels 1/2/4/5 (answer the
  lowest; SSIDAUTH=N, SSIDSPEC=N), full state machine with credit handling (incl. "no EFID in
  OPOWFC", CDT when listener credit reaches 0), SFID/SFPA/SFNA/EFID/EFPA/EFNA/CD/ESID/EERP/RTR with
  per-level layouts, F/V/U/T mapping with per-file EBCDIC<->ASCII option, unsigned EERP with a
  persistent EERP queue, NERP at level >= 4. Refuse 2.0 file services with SFNA 15/16/18/19 (or
  accept SFIDENV=1 files as opaque pass-through, configurable). Always send SFIDSIGN=N, DESCL=0,
  reason-text length 0.
- **Phase 2:** buffer compression (both directions), restart with persistent checkpoints,
  duplicate detection, routing/fan-out EERP tracking if the server is to act as an intermediate.
- **Phase 3 (optional):** CMS CompressedData + zlib (SFIDCOMP=1) if a partner asks for it;
  document a TLS-gateway deployment (e.g. stunnel terminating 6619 in front of the MVS 3305
  listener, with the gateway holding the certificate and enforcing client certificates) for
  partners that require TLS.
- **Not planned:** SECD/AUCH/AURP, CMS signing/encryption, signed EERP/NERP, special logic,
  certification.

Test plan pointers: [TC] 1.3.1 (level 5, SDEB 2048, CRED 99, CMPR N, REST Y, AUTH N), 1.3.2
(downgrade to <= 4), 1.4/1.5 (U file, EERP), 1.6 (restart), 2.7 (>100 MB), 2.8 (rejection with
reason text), plus F128, V44 and T files (the mandatory test formats) -- run against mendelson CE on
3305 for level 5 and against a 1.x-capable peer for levels 2/4.

### 10.3 Open items to settle in the design (not answered by the specs)
- Inactivity timer values per state (RFC: "activity timers for all situations where a response is
  required", no numbers; Appendix C's T1 45 s / T2 7 s apply to X.25 special logic only).
- Handling of SSIDLEV '3' or > 5 offered by a peer.
- SDEB below a 2.0 command's natural size (long UTF-8 texts): truncate text to fit.
- Policy for incoming SFIDSIGN=Y (accept and send unsigned EERP vs. refuse).
- Default code page mapping (CP037 <-> ISO 8859-1) and per-partner override for F/V/T.
