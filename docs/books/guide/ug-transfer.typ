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

Put #cmd("OFTPD.ISPPLIB") last in the #cmd("ISPPLIB") concatenation. It is
received with the block size 3120, which is smaller than that of the panel
libraries of ISPF. In a test, ISPF with #cmd("OFTPD.ISPPLIB") in front of a
library of the block size 19040 ended at READY without a screen\; with the
library behind the large one it worked.

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
  [3 Partners], [Shows the partner table as OFTPD has it, with the name, the
    Odette ID, the state, the busy sessions, the level and where the partner is
    reached. An administrator can also maintain the table here (see below).],
  [4 Configure], [Lets an administrator edit the member of the partner table.
    When the edit ends, OFTPD reads the member again. The partners kept by
    command are not read again.],
  [5 Monitor], [Shows the Odette ID, the port, the SVC, the sessions and the
    spool of the monitor.],
  [X Exit], [Leaves the dialog.],
)

=== Maintaining the Partners in the Dialog <xfer-dialog-ptn>

#idx("ISPF dialog", "partners")
Option 3 does with panels what the command #cmd("PARTNER") does (see
@ptn-cmd). Every change goes to OFTPD as a request, and the answer of OFTPD is
shown as the message of the panel. A user who is not an administrator can look
at the table, but OFTPD refuses a change.

- To add a partner, type its name into the field *New partner* and press Enter.
  If the partner is in the table already, the message says so\; use M to change
  it. Otherwise the entry panel follows.
- In front of a partner, type one of these letters\: *M* to modify it, *E* to
  enable it, *X* to disable it, *D* to delete it, *C* to call it now.

The entry panel has the keywords of an entry of the member: the Odette ID, the
two passwords, our own ID, the host, the port, the address, the level, the
mode, compression, call, sessions, translation, via and enabled. A new partner
starts with level 5, mode B, no compression, call and enabled, one session and
no translation. For a partner that is there, the panel shows its values. It shows
the passwords as *(set)* or *(none)*, never as text. A blank password keeps the
password as it is\; type a new one to replace it. Another field that you leave
as it is stays as it is, and a blank Host, Address or Via is cleared. Enter
saves, and END leaves the panel without saving. If OFTPD refuses the change, the
reason is on the panel, and you can correct the entry. On an 80-column screen
the message line has 80 columns, and a longer answer is cut\; the answer of a refused
#cmd("DELETE") is one of these. The command #cmd("OFTP PARTNER DELETE") gives
the whole answer.

The letter *D* shows a panel that asks you to confirm. Enter deletes the
partner, and END leaves it. OFTPD refuses it under the same rules as the
command: no session, no transfer in the spool, and no partner reached through
it. The deleted definition, with its passwords, cannot be got back.

== Tasks for an Administrator <xfer-admin>

A user named with #cmd("ADMIN") in the settings may also use the following.

- #cmd("OFTP CALL partner") calls a partner at once, as the operator command
  does.
- #cmd("OFTP PURGE DAYS(n)") deletes the finished transfers older than n days from
  the spool. Without #cmd("DAYS") it uses the #cmd("KEEPDAYS") of the settings.
  Only the spool is cleaned\; no data set is deleted.
- #cmd("OFTP LIST") shows all transfers, including the files received.
- #cmd("OFTP PARTNER ADD"), #cmd("CHANGE"), #cmd("ENABLE"), #cmd("DISABLE") and
  #cmd("DELETE") maintain the partner table without an edit of the member (see
  @ptn-cmd).
- Changes to all transfers, whoever queued them.
