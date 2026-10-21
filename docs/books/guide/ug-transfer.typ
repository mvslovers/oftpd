#import "../bookmaster/bookmaster.typ": *

= Sending Files <ug-transfer>

#idx("OFTP", "utility")
Users send files and look at the transfers with the #cmd("OFTP") utility, which
runs under TSO and in batch, or with the ISPF dialog (@ug-dialog). Both pass the
request to the monitor. The monitor must be running, and its settings must have
the keyword #cmd("SVC"), because the utility reaches the monitor through that
SVC. This chapter follows one file from the command to the receipt, and then the
other ways of sending. @ug-receive is the other direction. The chapter “The OFTP
Command” of the _OFTPD Reference_ describes every command and operand.

The examples are the sessions of @intro-examples. The user HERC01 works on the
monitor MVS1 and sends to the partner MAC1.

== Sending a File <xfer-send>

#idx("SEND")
To send a file you need two things: the name of the partner, as it is in the
partner table, and the data set. The data set is a sequential data set, or a
member of a partitioned data set. You must be allowed to read it.

The data set of the example is a text file of five lines.

#screen(raw(read("../ex/ug-transfer/report-text.txt")))

Under TSO you enter the command at the READY prompt, or in option 6 of ISPF.
In batch, the same command goes into the SYSIN of the utility, without the word
#cmd("OFTP").

#screen(```
//SEND     EXEC PGM=OFTP
//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
SEND PARTNER(MAC1) DSN('HERC01.REPORT.TEXT') VFN(REPORT.TXT) -
     FORMAT(T)
/*
```)

The utility does not send anything itself. It queues the file in the spool and
tells you the key of the transfer. This is the output of the job, in #cmd("SYSPRINT").

#screen(raw(read("../ex/ug-transfer/send.txt")))

The utility echoes the command with the word #cmd("OFTP") in front, and answers
with the key #cmd("T0000001") and the virtual file name. Keep the key\; every
other command takes it.

The monitor sends the file when it has a session with the partner. If the
partner has a host, as MAC1 has, the monitor calls it. If it has not, the file
waits until the partner calls. The log of the monitor shows the whole way, from
the queueing to the receipt.

#screen(raw(read("../ex/ug-transfer/send-log.txt")))

Read the log from the top.

- #cmd("OFTP230P") queues the transfer. #cmd("OFTP206P") is the call to the
  partner at the address of its entry.
- #cmd("OFTP290P session with MAC1") says what the two sides agreed on: level 5,
  a buffer of 4096 bytes and a credit of 99 buffers.
- #cmd("OFTP211P") says that the file has been sent, with the number of
  records. #cmd("OFTP213P") says that the partner has confirmed it: its receipt
  has come.
- #cmd("OFTP203P") ends the session. ESID 00 is a normal end, and
  #cmd("OURS") says that this side ended it.

For a text file the line #cmd("OFTP290P … sent") shows no record count\; the
count is in #cmd("OFTP211P"). The output is from a send that had been waiting
for no time at all\; a send to a partner that is not there is shown in @ops-down.

The return code of the utility is 0 when the file is queued, 4 when the data set
is empty (the file is queued all the same), and 8 or more when the command is
refused. The Reference lists the return codes. These are the refusals that you
meet most: a partner that is not in the table or is disabled (12), a data set
that is not there (8), a virtual file name that is too long (8), and a key that
is not in the spool (4). The batch job prints the return code of each command.

#screen(raw(read("../ex/ug-transfer/errors.txt")))

Give the full name of the data set. The utility does not add your TSO prefix.
For a member, put it in parentheses: #cmd("DSN('HERC01.LIB.DATA(MEMBER)')").

=== What the partner sees

#idx("virtual file name")
The partner does not see the name of your data set, but the virtual file name,
which you give with #cmd("VFN"). It is up to 26 characters. If you leave it out,
OFTPD takes the member name, or else the name of the data set, and refuses the
command if that is longer than 26 characters. Agree the virtual file names with
the partner\; the partner builds the name of its data set from them.

How the data is sent depends on the options.

- Without #cmd("FORMAT(T)") the file is sent in the format of its records, fixed,
  variable or undefined, and the data is sent as it is.
- With #cmd("FORMAT(T)") the file is sent as text, one line to a record. The
  characters are translated from EBCDIC to ISO 8859-1, and the lines are ended
  with a carriage return and a line feed, as a PC expects them. Use it for files
  that a person or a PC program reads.
- #cmd("XLATE") translates the data of a file that is not text in the same way.
  Use it for fixed or variable files that contain characters and go to a system
  that does not use EBCDIC. Do not use it for binary data.
- The #cmd("XLATE=Y") in the entry of a partner in the partner table does not
  change what OFTPD sends. It decides how OFTPD stores the files that it
  receives from that partner (see @ug-receive).

The example so far sent a text file. A payroll file of three fixed records of 80
bytes is sent in the format of its records, as it is, and a second time with
#cmd("XLATE"), translated to ISO 8859-1.

#screen(raw(read("../ex/ug-transfer/payroll-data.txt")))

#screen(raw(read("../ex/ug-transfer/send-fb.txt")))

Both transfers get a key of their own. @xfer-follow shows the format that each
one has in the spool, and @ug-receive shows what the partner made of them.

=== Sending later, or holding a file

A file can wait before it is sent.

- #cmd("HOLD") queues the file held. It is not sent until you release it with
  #cmd("OFTP RELEASE").
- #cmd("STRTDATE") and #cmd("STRTTIME") give the earliest time of the send, in
  the local time of the system. For example, #cmd("STRTDATE(*+1) STRTTIME(02:00)")
  is two o'clock tomorrow.
- #cmd("SENDWIN(22:00-06:00)") lets the file be sent only between those times of
  the day. The window may wrap over midnight.
- #cmd("DISP(DELETE)") deletes the data set after the partner has confirmed that
  it has the file. Use it only for a whole data set, and only if you may delete
  it. If the partner refuses the file, the data set is kept.

== Following a Transfer <xfer-follow>

#idx("LIST")#idx("SHOW")
#cmd("OFTP LIST") shows your transfers, and #cmd("OFTP SHOW key") shows one in
detail. An administrator sees all of them\; #cmd("LIST MINE") limits the list
to one's own.

#screen(raw(read("../ex/ug-transfer/list2.txt")))

Each line of #cmd("LIST") shows the key, whether it is a send or a receive, the
state, the partner, the virtual file name and the data set. The virtual file name
has a column of 26 characters. #cmd("SHOW") shows the transfer in full: the
format and the record length of the file (#cmd("format F, LRECL 80") for the
payroll file, #cmd("format T") for the text), the Odette IDs of the two sides,
the number of records and attempts, and in the last line the state in words,
here that the partner's receipt (EERP) has arrived. The first three files of
the examples are all #cmd("DELIVERED"). A send goes through these states.


#tab(caption: [The states of a send])[
  #table(columns: (1.3in, 1fr),
    [State], [What it means],
    [#cmd("QUEUED")], [Waiting for a session with the partner.],
    [#cmd("ACTIVE")], [Being sent now.],
    [#cmd("SENT")], [The partner has the file. Its receipt is outstanding.],
    [#cmd("DELIVERED")], [The partner confirmed the delivery. This is the good
      end.],
    [#cmd("WAITING")], [An attempt failed, and the next one is waiting. See
      @ops-retry.],
    [#cmd("HELD")], [On hold, by you or an operator, or because the attempts
      were used up.],
    [#cmd("REJECTED")], [The partner refused the file or its delivery. The
      reason is in #cmd("SHOW").],
    [#cmd("FAILED")], [OFTPD could not open the data set. The file is not tried
      again by itself\; correct the cause and use #cmd("RETRY").],
  )
]

Only #cmd("DELIVERED") says that the partner has the file. A file in the state
#cmd("SENT") has been sent, but the confirmation has not come yet.

You can change a transfer that is yours.

#deflist(width: 1.4in,
  [#cmd("HOLD key")], [Holds a send that is queued or waiting.],
  [#cmd("RELEASE key")], [Queues a held send again, and begins to count the
    attempts from 0.],
  [#cmd("RETRY key")], [Queues a send again, from any state but queued,
    for instance after a refusal. A send that was already sent is sent a second
    time.],
  [#cmd("DELETE key")], [Removes the transfer from the spool. The data set is
    not deleted. A transfer in a session cannot be deleted.],
)

A command with #cmd("PARTNER") in place of the key, #cmd("OFTP HOLD PARTNER(MAC1)") for example, changes all your sends to that partner.
The next chapters show these commands at work: a send that is held and
released, and one that waits for a partner that is not there, are in @ug-ops.
