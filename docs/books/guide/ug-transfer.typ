#import "../bookmaster/bookmaster.typ": *

= Sending and Receiving Files <ug-transfer>

#idx("OFTP", "utility")
Users send files and look at the transfers with the #cmd("OFTP") utility, which
runs under TSO and in batch, or with the ISPF dialog. Both pass the request to
the monitor. The monitor must be running, and its settings must have the
keyword #cmd("SVC"), because the utility reaches the monitor through that SVC.
This chapter shows the common tasks. The chapter “The OFTP Command” of the
_OFTPD Reference_ describes every command and operand.

== Sending a File <xfer-send>

#idx("SEND")
To send a file you need two things: the name of the partner, as it is in the
partner table, and the data set. The data set is a sequential data set, or a
member of a partitioned data set. You must be allowed to read it.

Under TSO:

#screen(```
OFTP SEND PARTNER(MAC1) DSN('IBMUSER.REPORT.TEXT') VFN(REPORT.TXT) FORMAT(T)
```)

In batch, the same command goes into the SYSIN of the utility:

#screen(```
//SEND     EXEC PGM=OFTP
//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
SEND PARTNER(MAC1) DSN('IBMUSER.REPORT.TEXT') -
     VFN(REPORT.TXT) FORMAT(T)
/*
```)

The utility does not send anything itself. It queues the file in the spool and
tells you the key of the transfer.

#screen(```
T0000012 queued for MAC1 as REPORT.TXT
```)

The monitor sends the file when it has a session with the partner. If the
partner has a host, the monitor calls it. If it has not, the file waits until
the partner calls. The return code of the utility is 0 when the file is queued,
4 when the data set is empty (the file is queued all the same), and 8 or more
when the command is refused. The Reference lists the return codes.

Give the full name of the data set. The utility does not add your TSO prefix.
For a member, put it in parentheses: #cmd("DSN('IBMUSER.LIB.DATA(MEMBER)')").

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
- The partner can have #cmd("XLATE=Y") in its entry of the partner table, which
  does the same for every file of that partner.

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

#screen(raw(read("../ex/ug-transfer/list.txt")))

Each line shows the key, whether it is a send or a receive, the state, the
partner, the virtual file name and the data set. The virtual file name has a
column of 26 characters. The example is what an administrator sees on the
receiving system\; a user sees only the transfers the user queued. A send
goes through these states.

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

== Receiving Files <xfer-recv>

#idx("received files")
To receive a file you need do nothing but have the partner in the partner table.
When the partner calls and offers a file, OFTPD receives it into a data set,
and when the file is complete it sends the receipt to the partner.

OFTPD names the data set after the virtual file name that the partner gives:
the prefix #cmd("INPREFIX") of the settings (#cmd("OFTP.IN") by default), then
the virtual file name, changed so that it fits the rules for data set names. A
file sent as #cmd("DAILY") becomes #cmd("OFTP.IN.DAILY"). If the data set exists,
OFTPD adds the transfer key as a further qualifier. The Reference gives the
exact rules.

The attributes of the data set come from the file description that the partner
sends. A text file is received as variable blocked records, with the record
length #cmd("TLRECL"). Fixed, variable and undefined files keep their format.
The data set is created by the started task of OFTPD, and OFTPD never deletes
it. Who may read it depends on the protection of the data sets that begin with
the prefix.

A received file belongs to nobody, so only an administrator sees it in
#cmd("OFTP LIST"). Its state is #cmd("RECEIVED") until OFTPD has sent the
receipt to the partner, and #cmd("CONFIRMED") after it. A file that OFTPD
refuses, for instance because the data set cannot be created or because the file
is for another Odette ID, is not kept in the spool\; the reason is in the log of
the monitor (messages #cmd("OFTP220E") and #cmd("OFTP223W")).

#idx("received files", "job after receipt")
A receive rule can also start a job when a file has arrived: with
#cmd("JOB=ACCT.JCL(ORDERS)") OFTPD reads that member, puts the name of the
data set, the virtual file name, the partner and the transfer key into it, and
gives it to the internal reader. The receipt to the partner does not wait for the
job. Two things to know before you use it. The job does not run as the OFTPD
started task but, with RAKF, as the default user of RAKF, so it may do what
that user may do. And the virtual file name comes from the partner\; it must never
decide by itself what the job touches or runs. Both are explained in the section
"A Job After Receipt" of the _OFTPD Reference_, which also lists the symbols and
the reasons for which no job is submitted (message #cmd("OFTP247W")).

== The ISPF Dialog <xfer-dialog>

#idx("ISPF dialog")#idx("OFTPI")
The dialog does the same as the utility, with panels. It needs its panels in the
#cmd("ISPPLIB") allocation and its module in the #cmd("ISPLLIB") allocation,
because it loads #cmd("ISPLINK") from there. This ISPF has no #cmd("LIBDEF"),
so add the libraries where your system allocates them, usually in the CLIST that
starts ISPF.

- #cmd("OFTPD.ISPPLIB") to the #cmd("ISPPLIB") allocation,
- #cmd("OFTPD.LINKLIB") to the #cmd("ISPLLIB") allocation.

Start the dialog with

#screen(```
SELECT CMD(OFTPI) NEWAPPL(OFTP)
```)

You can put that into a menu of your own. Add a line for the option to the
body of the menu panel, and #cmd("O,'CMD(OFTPI) NEWAPPL(OFTP)'") to its
#cmd("TRANS") statement, where you replace #cmd("O") by the number of the
option.

The main panel has these options.

#deflist(width: 1.5in,
  [1 Send], [Queues a data set for a partner. You give the partner, the data
    set, and if you want the virtual file name, text, translation, hold and
    user data. A question mark in the partner field shows the partners to pick
    one from.],
  [2 Transfers], [Lists the transfers, which you can limit to a partner, a
    state, or your own. Type a letter in front of a line: S to show it, H to
    hold, R to release, T to retry, D to delete.],
  [3 Partners], [Shows the partner table. C in front of a partner calls it now,
    which an administrator may do.],
  [4 Configure], [Lets an administrator edit the member of the partner table.
    When the edit ends, OFTPD reads the member again.],
  [5 Monitor], [Shows the Odette ID, the port, the SVC, the sessions and the
    spool of the monitor.],
  [X Exit], [Leaves the dialog.],
)

== Tasks for an Administrator <xfer-admin>

A user named with #cmd("ADMIN") in the settings may also use the following.

- #cmd("OFTP CALL partner") calls a partner at once, as the operator command
  does.
- #cmd("OFTP PURGE DAYS(n)") deletes the finished transfers older than n days from
  the spool. Without #cmd("DAYS") it uses the #cmd("KEEPDAYS") of the settings.
  Only the spool is cleaned\; no data set is deleted.
- #cmd("OFTP LIST") shows all transfers, including the files received.
- Changes to all transfers, whoever queued them.
