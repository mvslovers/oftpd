#import "../bookmaster/bookmaster.typ": *

= Naming the Files You Receive <ug-rules>

#idx("receive rules")
Without rules, OFTPD names every file that it receives after its virtual file
name, under the prefix #cmd("INPREFIX"). That is enough for one partner and a
few files. A _receive rule_ lets you choose the name of the data set by the
partner and the virtual file name, replace the data set of an earlier file,
put the new data set on a volume of your choice, and say how much space it
gets. The rules are the member #cmd("OFTPRCV"), which OFTPD reads when it
starts and at every refresh. This chapter writes two rules and shows what
OFTPD does with them\; the output is from a test of OFTPD 1.0.0 with two
monitors on one system, the sender called MAC1 and the receiver MVS1. The
chapter “Receive Rules” of the _OFTPD Reference_ lists every keyword, every
symbol and every message.

== Before you start

- The partner that sends the files is in the partner table of the receiving
  monitor. A rule names its partners, and OFTPD does not use a member with a
  partner in it that it does not know.
- The started task has the DD #cmd("OFTPRCV"). The sample procedure has it and
  points it at the member #cmd("OFTPRCV0"). If you keep an older procedure,
  see @inst-upgrade.
- You have copied #cmd("OFTPRCV0") from #cmd("OFTPD.SAMPLIB") to the library
  that holds your other members. The sample is only comments, and so changes
  nothing until you add a rule.

== Two rules

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

== Telling OFTPD

OFTPD reads the member at the start, after the partner table, and again at
#cmd("F OFTPD,REFRESH"). It writes how many rules it has read.

#screen(raw(read("../ex/ug-rules/start-log.txt")))

If the number is not what you expect, or the line is not there, see
“A mistake in the member” below.

== The first files

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

== When the data set exists already

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

== A mistake in the member

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

== Files that a rule refuses

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

== When a transfer breaks off

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
