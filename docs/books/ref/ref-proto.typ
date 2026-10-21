#import "../bookmaster/bookmaster.typ": *

= The Protocol and the Data <ref-proto>

#idx("protocol")
This chapter is the reference for what the commands of the other chapters turn
into on the wire, and what becomes of the records of a file on the way. It
holds the tables that the _OFTPD User's Guide_ leaves out because a user
does not need them to do a task: the commands of the protocol, the levels, the
formats of a file, the reason codes and the return codes. The names are those
of the protocol, which RFC 5024 describes for OFTP 2.0\; this chapter says only
what OFTPD does with them. OFTPD speaks OFTP 2.0 with every security feature
switched off, and drops to 1.4 or 1.3 as the partner needs.

== From Command to Protocol <proto-cmd>

#idx("protocol", "commands of OFTPD")
Most commands of #cmd("OFTP"), of the console and of the ISPF dialog are
requests to the monitor, and nothing goes to a partner because of them.
@proto-cmd-tab says which ones do, and which protocol commands they bring
about. The protocol commands are listed in @proto-list.

#tab(caption: [What a command brings about on the wire])[
  #table(columns: (1.5in, 1fr),
    [Command], [Protocol commands],
    [#cmd("OFTP SEND"), the send panel of the dialog], [None at once: the
      transfer is queued. When a session with the partner is open, OFTPD sends
      #cmd("SFID") for the file, the #cmd("DATA") buffers and #cmd("EFID"). The
      partner answers #cmd("SFPA") or #cmd("SFNA"), and #cmd("EFPA") or
      #cmd("EFNA"). The partner later sends #cmd("EERP"), the receipt, or
      #cmd("NERP"), and OFTPD answers with #cmd("RTR").],
    [#cmd("OFTP CALL"), #cmd("F OFTPD,CALL")], [OFTPD opens the connection to
      the partner and is the caller. The partner sends #cmd("SSRM"), OFTPD
      answers with its #cmd("SSID"), and the partner with its own, which fixes
      the level, the buffer, the credit and the mode. OFTPD has the turn first
      and sends the queued files as for #cmd("SEND"). When it has no more, it
      gives the turn with #cmd("CD"), answers what the partner sends, and ends
      the session with #cmd("ESID") 00 when the partner has nothing either.],
    [A call from a partner], [OFTPD sends #cmd("SSRM"), reads the #cmd("SSID")
      of the partner and answers with its own. For each file the partner offers
      it reads #cmd("SFID") and answers #cmd("SFPA") or #cmd("SFNA"), reads the
      #cmd("DATA") and #cmd("EFID"), and answers #cmd("EFPA") (never asking for
      the turn in it) or #cmd("EFNA"). Once the partner has given the turn with
      #cmd("CD"), OFTPD sends the owed #cmd("EERP") for each received file, and
      the partner answers #cmd("RTR").],
    [#cmd("OFTP RETRY")], [None at once. The transfer is queued again and goes,
      from its first record, in the next session with the partner.],
    [#cmd("OFTP LIST"), #cmd("SHOW"), #cmd("HOLD"), #cmd("RELEASE"),
      #cmd("DELETE"), #cmd("PURGE"), #cmd("PARTNER"), #cmd("PARTNERS"), the
      console commands #cmd("DISPLAY"), #cmd("HOLD"), #cmd("RELEASE"),
      #cmd("PURGE"), #cmd("REFRESH"), and the other panels of the dialog],
      [None. They read or change the spool and the partner table of the
      monitor.],
    [#cmd("F OFTPD,TRACE")], [None. It makes the sessions write the commands
      they send and receive to the log (see @proto-list).],
  )
] <proto-cmd-tab>

A file that OFTPD relays for another partner (#cmd("RELAY=Y") in
#cmd("OFTPPRM"), see @cfg-relay) is received like any file and then sent on
like a queued one. The partner that it came from gets the receipt only when the
final recipient has sent it. OFTPD sends a #cmd("NERP") of its own only for a
relayed file that it cannot send on, because the next partner refused it or the
attempts ran out\; a file of its own that fails has no #cmd("NERP"), since
nobody is waiting for one.

== The Protocol Commands <proto-list>

#idx("protocol", "trace")
These are the commands that OFTPD sends and receives, in the names the trace
writes (see "Reading the Log" in the chapter "Operating OFTPD" of the _OFTPD
User's Guide_). The number after the name in a trace line is the length of the
command in octets, counted without the four octets of the stream header. The
lengths below are those of OFTPD, which sends no description text and no
signature. The speaker is the side that has the turn, the listener the other.

#tab(caption: [The commands of the protocol])[
  #table(columns: (0.55in, 0.5in, 0.85in, 0.8in, 1fr),
    [Name], [Id], [From], [Length 1.3 and 1.4, 2.0], [Used for],
    [#cmd("SSRM")], [#cmd("I")], [called side], [19, 19], [Start session ready
      message: "ODETTE FTP READY".],
    [#cmd("SSID")], [#cmd("X")], [both], [61, 61], [Start session: Odette ID,
      password, level, buffer size, credit, mode, compression.],
    [#cmd("SFID")], [#cmd("H")], [speaker], [128, 165], [Start file: virtual
      file name, date, time, user data, destination, origin, format, record
      length, size.],
    [#cmd("SFPA")], [#cmd("2")], [listener], [10, 18], [Start file, positive
      answer, with the restart position (OFTPD always gives 0).],
    [#cmd("SFNA")], [#cmd("3")], [listener], [4, 7], [Start file, negative
      answer: reason code and whether to try again.],
    [#cmd("DATA")], [#cmd("D")], [speaker], [], [An exchange buffer of the
      file. The trace shows only how many buffers and octets.],
    [#cmd("CDT")], [#cmd("C")], [listener], [3, 3], [Set credit: the
      listener allows the next window of buffers.],
    [#cmd("EFID")], [#cmd("T")], [speaker], [22, 35], [End file: the number of
      records and of octets.],
    [#cmd("EFPA")], [#cmd("4")], [listener], [2, 2], [End file, positive
      answer. #cmd("cd Y") asks the speaker to give the turn.],
    [#cmd("EFNA")], [#cmd("5")], [listener], [3, 6], [End file, negative
      answer: reason code.],
    [#cmd("EERP")], [#cmd("E")], [speaker], [106, 110], [End to end response:
      the receipt of the final recipient.],
    [#cmd("NERP")], [#cmd("N")], [speaker], [128, 135], [Negative end
      response, from level 1.4: the file was not delivered, with a reason
      code.],
    [#cmd("RTR")], [#cmd("P")], [listener], [1, 1], [Ready to receive: the
      answer to #cmd("EERP") and #cmd("NERP").],
    [#cmd("CD")], [#cmd("R")], [speaker], [1, 1], [Change direction: the
      speaker gives the turn to the other side.],
    [#cmd("ESID")], [#cmd("F")], [either], [4, 7], [End session, with a reason
      code. 00 is a normal end.],
    [#cmd("SECD"), #cmd("AUCH"), #cmd("AURP")], [#cmd("J"), #cmd("A"), #cmd("S")], [both], [],
      [Secure authentication, level 2.0 only. OFTPD never sends them: a partner
      that asks for authentication in its #cmd("SSID") is answered with
      #cmd("ESID") 12 (@proto-reas).],
  )
] <proto-list-tab>

A session runs in this order: #cmd("SSRM"), then #cmd("SSID") from the caller
and from the called side. After that the speaker (the caller to begin with)
sends files, with #cmd("SFID"), #cmd("DATA"), #cmd("EFID") and the answers of
the listener, and the receipts, with #cmd("EERP") and #cmd("RTR"). It gives the
turn with #cmd("CD"). The speaker ends the session with #cmd("ESID") 00, but
only right after a #cmd("CD") that the other side has sent unasked, which means
that the other side has nothing to send. The trace of a whole session is in the
chapter "Operating OFTPD" of the _OFTPD User's Guide_.

== Protocol Levels <proto-level>

#idx("protocol", "levels")
The level is agreed in the #cmd("SSID"): each side names the highest it speaks,
and the lower one is used. #cmd("LEVEL") of the partner table sets the highest
that OFTPD offers a partner (@cfg-ptn). @proto-level-tab gives the values, and
what changes with them.

#tab(caption: [Protocol levels])[
  #table(columns: (0.7in, 0.7in, 1fr),
    [SSIDLEV], [Release], [What OFTPD does],
    [5], [OFTP 2.0], [The default. The #cmd("SFID") has 165 octets, with the
      security fields all set to off and no description\; the numbers in
      #cmd("SFPA") and #cmd("EFID") are wider\; #cmd("SFNA"), #cmd("EFNA"),
      #cmd("ESID") and #cmd("NERP") end with a length and a text for the
      reason\; OFTPD sends the length 000. The buffer must be at least 165
      octets, otherwise the session ends with the reason 10.],
    [4], [OFTP 1.4], [The #cmd("SFID") has 128 octets, with the date as
      #cmd("CCYYMMDD") and the time as #cmd("HHMMSScccc"). #cmd("NERP") and the
      reason code 14 exist.],
    [2], [OFTP 1.3], [As 1.4, but the date and the time of the #cmd("SFID") have
      six digits each (#cmd("YYMMDD"), #cmd("HHMMSS")), and there is no
      #cmd("NERP"): one that would go to such a partner is not sent, and the
      transfer is closed.],
    [1], [OFTP 1.2], [Spoken as 1.3. A partner that announces 3, which does not
      exist, is also treated as 1.3.],
  )
] <proto-level-tab>

The #cmd("SSID") has 61 octets at every level, so that the called side can read
it before it knows which level is used. All other commands then have the layout
of the level that was agreed, with the lengths of @proto-list-tab.

OFTPD does not take part in what only level 2.0 has: signing, encryption,
compression of files and secure authentication. A file that asks for one of
them is refused with the reason code 15, 16, 18 or 19 (@proto-reas).
Compression of the exchange buffers (#cmd("COMPRESS")) is not that: it is a
feature of the session, and OFTPD offers it if the partner table says so.

== Formats of a File <proto-fmt>

#idx("file formats")
The #cmd("SFID") names the format of the file: #cmd("F") fixed records,
#cmd("V") variable records, #cmd("U") unstructured (a stream of octets), or
#cmd("T") text. How the records of an MVS data set become the data on the wire,
and the other way round, depends on the format, as the two tables show. A
translation, where the tables mention one, is between CP037 (EBCDIC) and ISO
8859-1, character for character.

#tab(caption: [A data set that OFTPD sends])[
  #table(columns: (1.3in, 0.7in, 1fr),
    [Data set], [Format], [On the wire],
    [#cmd("RECFM=F") or #cmd("FB")], [#cmd("F")], [The record length is that of
      the data set. Each record is a record on the wire, as it is, blanks
      included.],
    [#cmd("RECFM=V") or #cmd("VB")], [#cmd("V")], [The record length is the
      #cmd("LRECL") minus 4, the length of the record descriptor word. Each
      record goes without its descriptor word.],
    [Any other, for instance #cmd("RECFM=U")], [#cmd("U")], [The record length
      is 0. The records of the data set follow one another as a stream, with no
      line ends.],
    [Any, with #cmd("FORMAT(T)")], [#cmd("T")], [The record length is 0. Each
      record is a line, translated, with CR LF at its end. In a record of a
      fixed data set the blanks at its end are cut off first.],
  )
] <proto-fmt-send-tab>

The data of #cmd("F"), #cmd("V") and #cmd("U") goes as the data set has it,
unless #cmd("XLATE") is given on the #cmd("SEND"): then OFTPD translates it
from EBCDIC. A text file is always translated. The #cmd("SFID") also carries an
estimate of the size in kilobytes, from the tracks the data set uses\; for a
member of a partitioned data set it is 0, which means "not known".

#tab(caption: [A file that OFTPD receives])[
  #table(columns: (0.7in, 1.5in, 1fr),
    [Format], [Data set that OFTPD allocates], [What becomes of the data],
    [#cmd("F")], [#cmd("RECFM=FB"), #cmd("LRECL") as the partner says (1 to
      32760).], [The stream is cut into records of that length. The
      end-of-record marks of the sender are only counted. A rest at the end
      that is shorter than a record makes the file fail with the reason 10.],
    [#cmd("V")], [#cmd("RECFM=VB"), #cmd("LRECL") the partner's plus 4 (1 to
      32752).], [Each record gets its record descriptor word. A record longer
      than the partner announced makes the file fail with the reason 05.],
    [#cmd("U")], [#cmd("RECFM=U"), the block size #cmd("UBLKSIZE").], [The
      stream is cut into blocks of that size, the last one shorter.],
    [#cmd("T")], [#cmd("RECFM=VB"), #cmd("LRECL=TLRECL").], [Lines are cut at
      CR LF, which is removed, and translated. A line longer than
      #cmd("TLRECL") minus 4 makes the file fail with the reason 05. A last
      line with no CR LF is kept.],
  )
] <proto-fmt-recv-tab>

For #cmd("F"), #cmd("V") and #cmd("U") the data is translated to EBCDIC only if
the partner table has #cmd("XLATE=Y") for the partner, or the receive rule that
chose the name has it (@rcv-kw)\; text always is. The space, the unit and the
volume of the data set are described in @cfg-alloc, and its name in
@cfg-dsn.

At the end of a file OFTPD compares what the partner announced in #cmd("EFID")
with what it has received: for #cmd("F") and #cmd("V") the number of records
(reason 10), and for every format the number of octets (reason 11). For a file
of the format #cmd("U") or #cmd("T") the #cmd("EFID") carries no record count,
and so the line #cmd("OFTP290P … sent, 0 records") of the session is not a
fault\; the number of records of such a transfer is in #cmd("OFTP211P") and
#cmd("OFTP222P"). The octets of a text file are those of the file on the wire,
with the CR LF of each line.

== Reason Codes <proto-reas>

#idx("reason codes")
A refusal or the end of a session carries a reason code of two digits, the
codes of the protocol. #cmd("OFTP SHOW") writes the reason of a transfer with
the side that gave it\: #cmd("partner") for a code that the partner sent,
#cmd("local") for one that OFTPD gave. The messages #cmd("OFTP203P") (the
session), #cmd("OFTP212P") (a refused file) and #cmd("OFTP220E") (a data set
that could not be allocated) name the code as well. The tables list the codes
that OFTPD sends, and when. When a partner sends another code, OFTPD logs it as
it comes.

#tab(caption: [ESID: reasons for ending a session])[
  #table(columns: (0.4in, 1.7in, 1fr),
    [Code], [Meaning], [When OFTPD sends it],
    [00], [Normal end], [The session is done.],
    [01], [Command not recognized], [A command whose first octet is NUL.],
    [02], [Protocol violation], [Any other command than the state allows, a
      #cmd("DATA") beyond the credit, a restart position in #cmd("SFPA") that
      OFTPD never asked for, or a buffer of #cmd("DATA") that is damaged
      inside.],
    [03], [Odette ID not known], [The caller's Odette ID is not in the table,
      the partner is disabled or reached through another partner (#cmd("VIA")),
      or it calls from another address than its #cmd("ADDRESS")
      (#cmd("OFTP201W")). When OFTPD is the caller: the answer comes from
      another Odette ID than the partner's.],
    [04], [Password wrong], [The password in the #cmd("SSID") is not the one of
      the table.],
    [06], [Invalid data in a command], [A field that cannot be read, a command
      of the wrong length, or an answer that does not decode.],
    [07], [Buffer size error], [The stream header is not right, or the length
      of a command is outside the buffer.],
    [08], [Resources not available], [The partner has as many sessions running
      as #cmd("SESSIONS") allows (#cmd("OFTP202W")), or OFTPD has no storage
      for a file. The partner may try again later.],
    [09], [Time out], [Nothing came within #cmd("TIMEOUT") seconds.],
    [10], [Mode or capabilities not compatible], [The level, the buffer, the
      credit, the compression or the mode (#cmd("MODE")) of the two sides do
      not match, or at level 5 the buffer is under 165 octets.],
    [12], [Secure authentication not compatible], [The #cmd("SSID") asks for
      authentication, which OFTPD does not offer.],
    [99], [Unspecified], [OFTPD could not build a command, or could not read
      the data set it sends. When the connection is lost, 99 is only what the
      log records, as there is nobody to send it to.],
  )
] <proto-esid-tab>

#tab(caption: [SFNA and EFNA: reasons for refusing a file])[
  #table(columns: (0.4in, 1.7in, 0.8in, 1fr),
    [Code], [Meaning], [Command], [When OFTPD sends it],
    [01], [Invalid file name], [SFNA], [The virtual file name cannot be made
      into a data set name (see @cfg-dsn), or a receive rule makes a name that
      is not valid (@rcv-name, #cmd("OFTP224W")).],
    [02], [Invalid destination], [SFNA], [The file is not for OFTPD's Odette
      ID, and OFTPD has no route for it, or the route would send it back to
      the partner it came from (#cmd("OFTP223W")).],
    [05], [Record length not supported], [SFNA, EFNA], [A fixed or variable
      file with a record length outside 1 to 32760 (V: 32752)\; or, in
      #cmd("EFNA"), a record or a text line longer than the data set allows.],
    [06], [File too big], [SFNA], [A number in the #cmd("SFID") does not fit
      in 32 bits.],
    [10], [Invalid record count], [EFNA], [The record count of the partner is
      not what OFTPD received, or a fixed file ends inside a record.],
    [11], [Invalid byte count], [EFNA], [The octet count of the partner is not
      what OFTPD received.],
    [12], [Access method failure], [SFNA, EFNA], [The data set cannot be
      allocated, written, closed or renamed, or a rule with #cmd("REPLACE=Y")
      meets a name that is not a sequential data set (#cmd("OFTP226E"),
      #cmd("OFTP234W")).],
    [13], [Duplicate file], [SFNA], [The same file (origin, name, date and
      time) was received before\; then the receipt is owed once more. Or a
      receive rule with #cmd("REPLACE=N") meets a data set of that name
      (#cmd("OFTP225W")), or the name with the transfer key added exists as
      well.],
    [14], [File direction refused], [SFNA], [The partner table says
      #cmd("MODE=S"), so that OFTPD only sends, and the partner offers a file.
      Below level 1.4 the code is 99.],
    [15], [Cipher suite not supported], [SFNA], [The #cmd("SFID") names a
      cipher suite.],
    [16], [Encrypted file not allowed], [SFNA], [The file is encrypted, or in a
      CMS envelope.],
    [18], [Compression not allowed], [SFNA], [The file is compressed.],
    [19], [Signed file not allowed], [SFNA], [The file is signed.],
    [99], [Unspecified], [SFNA], [The spool could not take the transfer, or
      there is no storage.],
  )
] <proto-sfna-tab>

A code that is in neither table is not sent by OFTPD. An #cmd("SFNA") also says
whether the sender should try again. OFTPD answers #cmd("Y") when the spool or
the data set could not be had and may be had later, and when a rule with
#cmd("REPLACE=N") meets an existing data set\; for the other refusals it answers
#cmd("N"). An #cmd("EFNA") never says to try again. When a partner refuses a
file of OFTPD with #cmd("Y"), the transfer waits and is tried again\; if the
attempts run out it is held (see the section "Trying Again" of the _OFTPD
User's Guide_). With #cmd("N"), or after an #cmd("EFNA"), the transfer is
#cmd("REJECTED").

== Return Codes of the Programs <proto-rc>

#idx("return codes")
#cmd("OFTP") has the return codes of @cmd-rc-tab. The monitor and the dialog
have those of @proto-rc-tab.

#tab(caption: [Return codes of the monitor and of the dialog])[
  #table(columns: (1.1in, 0.6in, 1fr),
    [Program], [Code], [Meaning],
    [#cmd("OFTPD"), the monitor], [0], [The monitor ended after the console
      command #cmd("P OFTPD") (or #cmd("STOP")).],
    [], [8], [The monitor did not start: no storage or communication area
      (#cmd("OFTP090E"), #cmd("OFTP091E")), #cmd("OFTPPRM") cannot be opened or
      has no #cmd("ODETTEID") (#cmd("OFTP011E"), #cmd("OFTP012E")), the spool
      cannot be opened or its partner records cannot be read
      (#cmd("OFTP020E"), #cmd("OFTP019E")), or the monitor cannot start its
      tasks. The message names the cause.],
    [#cmd("OFTPI"), the dialog], [0], [The dialog ended.],
    [], [8], [The dialog did not start: no storage, or it does not run under
      ISPF (#cmd("OFTPI001E")).],
  )
] <proto-rc-tab>
