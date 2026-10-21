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
- The #cmd("XLATE=Y") in the entry of a partner in the partner table does not
  change what OFTPD sends. It decides how OFTPD stores the files that it
  receives from that partner (see @xfer-recv).

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

#screen(```
OFTP LIST
T0000012 SEND QUEUED    MAC1     REPORT.TXT      IBMUSER.REPORT.TEXT
T0000013 RECV CONFIRMED MAC1     DAILY           OFTP.IN.DAILY
2 transfers
```)

Each line shows the key, whether it is a send or a receive, the state, the
partner, the virtual file name and the data set. The example is what an
administrator sees\; a user sees only the transfers the user queued. A send
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
exact rules. If you want other names, or want a file to replace the one before,
write receive rules (@xfer-rules).

The attributes of the data set come from the file description that the partner
sends. A text file is received as variable blocked records, with the record
length #cmd("TLRECL"). Fixed, variable and undefined files keep their format,
and their data is stored as it comes, unless the partner has #cmd("XLATE=Y") in
the partner table (or a receive rule says #cmd("XLATE=Y")). Then OFTPD translates
it from ISO 8859-1 to EBCDIC. Text is always translated. The data set is created by the started task of OFTPD, and OFTPD never deletes
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

== Receive Rules <xfer-rules>

#idx("receive rules")
Without rules, OFTPD names every file that it receives after its virtual file
name, under the prefix #cmd("INPREFIX"). That is enough for one partner and a
few files. A _receive rule_ lets you choose the name of the data set by the
partner and the virtual file name, replace the data set of an earlier file,
put the new data set on a volume of your choice, and say how much space it
gets. The rules are the member #cmd("OFTPRCV"), which OFTPD reads when it
starts and at every refresh. This section writes two rules and shows what
OFTPD does with them\; the output is from a test of OFTPD 1.0.0 with two
monitors on one system, the sender called MAC1 and the receiver MVS1. The
chapter “Receive Rules” of the _OFTPD Reference_ lists every keyword, every
symbol and every message.

=== Before you start

- The partner that sends the files is in the partner table of the receiving
  monitor. A rule names its partners, and OFTPD does not use a member with a
  partner in it that it does not know.
- The started task has the DD #cmd("OFTPRCV"). The sample procedure has it and
  points it at the member #cmd("OFTPRCV0"). If you keep an older procedure,
  see @inst-upgrade.
- You have copied #cmd("OFTPRCV0") from #cmd("OFTPD.SAMPLIB") to the library
  that holds your other members. The sample is only comments, and so changes
  nothing until you add a rule.

=== Two rules

#screen(```
* OFTPRCV0 - where the files of the partners go
RULE=ORDERS  FROM=MAC1  VFN=ORDERS.*
             DSN=ACCT.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
RULE=DAILY   FROM=*     VFN=DAILY
             DSN=ACCT.IN.%ID.DAILY  REPLACE=Y  VOLSER=WORK01  SPACE=30
```)

A rule begins with #cmd("RULE=") and a name, and the keywords after it belong
to it until the next #cmd("RULE="). A rule may run over several lines. The two
rules say this.

- A file from MAC1 whose virtual file name begins with #cmd("ORDERS.") goes to
  a data set named after the day and the time at which it arrived. The parts
  that begin with a percent sign are _symbols_\; OFTPD puts a value in their
  place.
- A file with the virtual file name #cmd("DAILY"), from any partner, goes to
  #cmd("ACCT.IN.") followed by the name of the partner and #cmd("DAILY"). If
  that data set exists, the new file replaces it. The data set is created on the
  volume #cmd("WORK01"), and when the sender does not say how large the file is,
  with 30 tracks.
- A file that no rule matches is received as it would be without rules.

OFTPD takes the first rule from the top that matches the partner and the
virtual file name, so write the narrow rules first. The first part of every
#cmd("DSN=") is fixed text, here #cmd("ACCT"). The virtual file name comes from
the partner, and the partner must not be able to choose the high-level
qualifier of your data sets\; OFTPD does not accept a rule that would let it.

=== Telling OFTPD

OFTPD reads the member at the start, after the partner table, and again at
#cmd("F OFTPD,REFRESH"). It writes how many rules it has read.

#screen(raw(read("../ex/ug-rules/start-log.txt")))

If the number is not what you expect, or the line is not there, see
“A mistake in the member” below.

=== The first files

The sender queues four files. It makes no difference to the sender that the other
side has rules.

#screen(raw(read("../ex/ug-rules/send.txt")))

The receiving monitor writes a line to its log for each file, with the name of
the data set that it made. (These lines have the class P: they are in the log
only, not on the console.)

#screen(raw(read("../ex/ug-rules/recv-log.txt")))

Take the lines one by one.

- #cmd("ORDERS.JAN") matched the rule #cmd("ORDERS"). The name has the date and
  the time at which the file arrived on this system, 23:56:09 on 20 October 2026.
- The first #cmd("DAILY") matched the rule #cmd("DAILY"). No data set of that
  name existed, so there was nothing to replace.
- The second #cmd("DAILY") found the data set and replaced it, which
  #cmd("OFTP229I") says. The data set now holds the three records of the
  second file\; the records of the first are gone.
- #cmd("OTHER.FILE") matched no rule and got the name that it would get without
  rules: the prefix #cmd("OFTP.IN") and the virtual file name.

An administrator sees the received files in #cmd("OFTP LIST") on the receiving
side, and #cmd("OFTP SHOW") names the data set. The rule that made the name does
not appear there.

#screen(raw(read("../ex/ug-rules/recv-list.txt")))

The rule #cmd("DAILY") said #cmd("VOLSER=WORK01"). In this test the settings of
the monitor had #cmd("VOLSER=WORK00"), and the catalog shows where each data set
went.

#screen(raw(read("../ex/ug-rules/listcat.txt")))

=== When the data set exists already

Without #cmd("REPLACE=Y"), a file whose data set exists is refused. In the test
the rule #cmd("DAILY") was changed to #cmd("REPLACE=N"), a rule for another name
was added, and the operator gave the command #cmd("F OFTPD,REFRESH"). Then the
sender queued #cmd("DAILY") again. The log of the receiver says

#screen(raw(read("../ex/ug-rules/refuse-n.txt")))

and the sender shows the refusal with the reason code 13.

#screen(raw(read("../ex/ug-rules/refuse-n-show.txt")))

The reason code 13 means that the file is a duplicate of one that exists, and
the answer says that the sender may try again. The transfer is therefore
#cmd("WAITING"), and OFTPD sends it again after its retry interval (@ops-retry).
It was refused again a minute later, and #cmd("SHOW") then counted 2 attempts.
This goes on until you delete the data set on the receiving system, change the
rule, or delete the transfer on the sending system (#cmd("OFTP DELETE key")).

With #cmd("REPLACE=Y") OFTPD does not touch the old data set until the new file
has arrived and has been closed. A transfer that breaks off therefore does not
cost you the old file. The Reference describes the steps, the backup that OFTPD
makes while it replaces, and what to do if the system stops in the middle.

=== A mistake in the member

OFTPD reads all of the member before it uses any of it. If a single line is
wrong, it uses none of the member. At a refresh the rules that were in force
stay in force\; at the start, OFTPD has no rules. Here the operator added two
lines to the member.

#screen(```
RULE=BAD FROM=NOBODY VFN=X DSN=%VFN
BOGUS=1
```)

The log says

#screen(raw(read("../ex/ug-rules/errors.txt")))

The rule would get its high-level qualifier from the virtual file name
(#cmd("%VFN")), which is not allowed, and #cmd("BOGUS") is not a keyword. The
line numbers are those of the member. OFTPD does not check #cmd("FROM=") of a
rule that already has an error in one of its lines, so #cmd("NOBODY") is not
reported yet. When #cmd("DSN=") is mended and #cmd("BOGUS=1") is gone, the next
error comes out, for #cmd("NOBODY") is not in the partner table.

#screen(raw(read("../ex/ug-rules/errors-from.txt")))

After a refresh with such a message, the old rules still decide. A file sent
then was named by them. So read the log after every change of the member, and
look for #cmd("OFTP024I"), which says that the new rules are in force.

If the DD #cmd("OFTPRCV") is not there, or names a member that does not exist,
OFTPD starts without rules and says nothing. Compare the number in
#cmd("OFTP024I") with the number of rules that you wrote.

=== Files that a rule refuses

A rule builds the name from what the partner sends, and the name may not come
out right. In the test a rule had #cmd("DSN=ACCT.IN.%Q2"), the second part of
the virtual file name, and the partner sent #cmd("X.9ABC").

#screen(raw(read("../ex/ug-rules/refuse-name.txt")))

A qualifier of a data set name cannot begin with a digit. OFTPD refuses the file
with the reason code 01, which means that the name is not valid, and tells the
sender not to try again. The transfer on the sender is #cmd("REJECTED").

#screen(raw(read("../ex/ug-rules/refuse-name-show.txt")))

Unlike the naming without rules, a rule does not clean the virtual file name up.
It uses it as it is. The Reference lists all refusals with their reason codes.

=== When a transfer breaks off

A file arrives into a temporary data set, and only a complete file gets its name.
In a test the sender was cancelled while a large file was arriving for the rule
#cmd("DAILY"), with a data set to replace. The log of the receiver shows the
lost connection and nothing else.

#screen(raw(read("../ex/ug-rules/break-log.txt")))

There was no message on the console. Afterwards the old data set had the same
extent and the same records as before, no temporary data set was left, and the
spool of the receiver no longer held the transfer. When the sender was started
again, it had the transfer in the state #cmd("QUEUED") and called the receiver.

If the receiving monitor is stopped by a cancel while it receives, the partial
files are discarded in the same way when it starts again, and the log says how
many (#cmd("OFTP040P")). A stop in the middle of a replace has messages of its
own. The Reference has them, in “After a Stop in the Middle of a Replace”.

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
