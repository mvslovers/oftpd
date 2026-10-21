#import "../bookmaster/bookmaster.typ": *

= Receiving Files <ug-receive>

#idx("received files")
To receive a file you need do nothing but have the partner in the partner table.
When the partner calls and offers a file, OFTPD receives it into a data set,
and when the file is complete it sends the receipt to the partner. This chapter
shows what you see of it, where the data set is, what it looks like, and how an
administrator finds the transfer. The examples are the sessions of
@intro-examples, with the roles turned round: the partner MAC1 sends two files
to the monitor MVS1.

== What the Log Shows <recv-log>

The monitor that receives writes the call, the file and the end of the session
to its log. The lines are of the class P, so they are in the log only, not on
the console.

#screen(raw(read("../ex/ug-transfer/receive-log.txt")))

Read the lines from the top.

- #cmd("OFTP205P") says that a call has come. The address is that of the caller,
  not yet a partner: OFTPD does not know who it is until the session begins.
- #cmd("OFTP290P session with MAC1") is the session after the log-on, with what
  the two sides agreed on. From here on every line carries the name of the
  partner.
- #cmd("OFTP222P") says that the file #cmd("REPORT.FROM.MAC1") has been received
  as the data set #cmd("OFTP.IN.REPORT.FROM.MAC1"), with the key #cmd("T0000011"),
  and that the receipt is on its way.
- #cmd("OFTP203P") ends the session. This time the partner ended it
  (#cmd("PARTNER")), and the counts say that this side received one file
  (#cmd("0 OUT 1 IN")).

The line #cmd("OFTP290P … received, 0 records") shows no record count for a text
file\; read it in #cmd("OFTP222P").

== The Name and the Attributes of the Data Set <recv-dsn>

OFTPD names the data set after the virtual file name that the partner gives:
the prefix #cmd("INPREFIX") of the settings (#cmd("OFTP.IN") in the examples),
then the virtual file name, changed so that it fits the rules for data set names.
The file #cmd("REPORT.FROM.MAC1") became #cmd("OFTP.IN.REPORT.FROM.MAC1"). If the
data set exists, OFTPD adds the transfer key as a further qualifier. The
Reference gives the exact rules. If you want other names, or want a file to
replace the one before, write receive rules (@ug-rules).

The attributes of the data set come from the file description that the partner
sends. These are the two data sets that MAC1 sent, a text file and a file of
fixed records, with their contents.

#screen(raw(read("../ex/ug-transfer/received-from.txt")))

The text file is variable blocked with a record length of 255, and the lines of
the file are the records. The second
file is fixed, 80 bytes to a record, as its sender had it. The three lines of the
payroll file are the same as on the partner's system, because the data of a file
that is not text is stored as it comes, unless the partner has #cmd("XLATE=Y")
in the partner table, or a receive rule says #cmd("XLATE=Y"). Then OFTPD
translates it from ISO 8859-1 to EBCDIC. Text is always translated.

The same holds the other way round. This is the file that @ug-transfer sent,
seen on the partner's system.

#screen(raw(read("../ex/ug-transfer/received.txt")))

The started task of OFTPD creates the data set. OFTPD does not delete a data set
that it has received, except that a receive rule with #cmd("REPLACE=Y") replaces
the data set of an earlier file (@ug-rules). Who may read the data set depends on
the protection of the data sets that begin with the prefix.

== Finding the Transfer <recv-list>

A received file belongs to nobody, so only an administrator sees it in
#cmd("OFTP LIST"). Its state is #cmd("RECEIVED") until OFTPD has sent the
receipt to the partner, and #cmd("CONFIRMED") after it. The session of the
example is over, so both files are in the state #cmd("CONFIRMED"). HERC01 may
ask for them, because HERC01 is in #cmd("ADMIN") of the settings.

#screen(raw(read("../ex/ug-transfer/recv-list.txt")))

#cmd("SHOW") names the data set, the Odette IDs, the number of records, and in
the last line the state in words, here that the file has been received and that
the receipt (EERP) has been sent. The owner is a dash, since nobody queued the
transfer. A list for the state #cmd("RECEIVED") is empty once the receipts have
gone\; use it to find files whose receipt is still owed.

A file that OFTPD refuses, for instance because the data set cannot be created
or because the file is for another Odette ID, is not kept in the spool\; the
reason is in the log of the monitor (messages #cmd("OFTP220E") and
#cmd("OFTP223W")), and the sender sees the refusal in its transfer.

== A Job After Receipt <recv-job>

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
