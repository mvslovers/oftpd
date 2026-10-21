#import "../bookmaster/bookmaster.typ": *

= Operating and Troubleshooting <ug-ops>

#idx("operation")
This chapter is for the people who keep OFTPD running. It follows a transfer
from start to end, says what OFTPD does when something goes wrong, how it
cleans up after itself, and what to look at when a transfer does not arrive.
The commands themselves are in the _OFTPD Reference_, the chapters “The OFTP
Command” and “Operator Commands”, and the messages in its appendix
“Messages”.

== The Life of a Transfer <ops-life>

#idx("transfer", "states")
The spool holds one record for each transfer, and the state in that record tells
where the transfer stands. @ops-life-fig shows the ways a transfer can go. A
send is a file that is sent to a partner, a receive a file that a partner sent.

#fig(caption: [The states of a transfer])[
  #screen(```
Send:     QUEUED --> ACTIVE --> SENT --> DELIVERED

          ACTIVE --> WAITING    an attempt failed
          WAITING --> ACTIVE    the next attempt, after the wait
          WAITING --> HELD      the attempts are used up
          HELD --> QUEUED       RELEASE
          ACTIVE --> REJECTED   the partner refused the file
          SENT --> REJECTED     negative receipt
          ACTIVE --> FAILED     the data set cannot be opened

Receive:  ACTIVE --> RECEIVED --> CONFIRMED
  ```)
] <ops-life-fig>

The states are explained in the chapter “The OFTP Command” of the Reference.
Three of them need an operator's attention: #cmd("HELD") needs somebody to
decide, #cmd("REJECTED") needs an answer from the partner, and #cmd("FAILED")
needs the data set to be put right. A transfer in the state #cmd("QUEUED") or
#cmd("WAITING") is dealt with by OFTPD itself.

The finished states are #cmd("DELIVERED"), #cmd("CONFIRMED"), #cmd("REJECTED")
and #cmd("FAILED"). Only these are ever removed from the spool (see
@ops-keep).

== Retries <ops-retry>

#idx("RETRY")#idx("MAXRETRY")
When a call or a transfer fails, OFTPD does not give up at once. The transfer
goes to the state #cmd("WAITING"), and OFTPD tries again after a wait. The wait
is the #cmd("RETRY") of the settings, in seconds, multiplied by the number of
attempts made so far, and never more than a day. With the default of 300 seconds
the waits are 5, 10, 15 minutes and so on. The same interval is the pause before
OFTPD calls a partner again after a call that failed.

After #cmd("MAXRETRY") attempts (10 by default) OFTPD stops trying, and writes
#cmd("OFTP215W"). The transfer is then #cmd("HELD"). It stays in the spool until
somebody decides, with one of the following.

- #cmd("OFTP RELEASE key") or #cmd("F OFTPD,RELEASE partner") queues it again, and
  counts its attempts from 0. Do this when the cause is removed, for instance when
  the partner is back.
- #cmd("OFTP DELETE key") removes it.

A partner that refuses a call because it has no free session (reason 08) is a
different case, as it says only that the partner is busy. OFTPD calls it again
after a short pause of its own, between 5 and 60 seconds, and does not count it
as a failed attempt (message #cmd("OFTP218P")). Two OFTPD monitors that call each
other can meet this way, and it sorts itself out.

If you have set #cmd("MAXRETRY=0"), OFTPD never gives up and never holds a file.

== Holding Files <ops-hold>

#idx("HOLD")#idx("RELEASE")
To stop the sends to a partner for a while, for instance when its system is
down for maintenance, hold them.

#screen(```
F OFTPD,HOLD MAC1
F OFTPD,RELEASE MAC1
```)

#cmd("HOLD") changes the transfers that are queued or waiting to the state held,
so that OFTPD does not call for them. A send that is already in a session is not
stopped. Files queued after the hold are queued as usual, so hold again, or hold
them one by one. #cmd("RELEASE") queues the held files again. #cmd("ALL") in
place of the partner means every partner. OFTPD answers with the number of
transfers it changed (#cmd("OFTP233I")).

A user does the same for a single transfer or for his own sends with
#cmd("OFTP HOLD") and #cmd("OFTP RELEASE").

== Housekeeping <ops-keep>

#idx("KEEPDAYS")#idx("housekeeping")#idx("PURGE")
A transfer stays in the spool when it is finished, so that you can see what
became of it. OFTPD removes it after #cmd("KEEPDAYS") days (7 by default) since
its last change. It does this once a day, the first time when it has just
started. The message #cmd("OFTP235I") says how many transfers it deleted, and it
is written only when there were some. #cmd("KEEPDAYS=0") keeps the transfers for
ever.

You can do it by hand at any time.

#screen(```
F OFTPD,PURGE 30
OFTP PURGE DAYS(30)
```)

The first is for the operator, the second for an administrator. Both delete the
finished transfers older than 30 days. #cmd("PURGE 0") deletes all finished
transfers, however recent.

Housekeeping deletes records of the spool, nothing else. The data sets of the
files that were received, and the data sets that were sent, are never deleted by
OFTPD. It is your job to clean up the data sets under #cmd("INPREFIX"), and the
data sets that you sent if you did not use #cmd("DISP(DELETE)"). A transfer that
is not finished (queued, waiting, held, sent, received) is not deleted by
housekeeping, however old. A file that is #cmd("SENT") without the receipt ever
coming stays until somebody deletes it, so look at the old ones now and then.

Size the spool for what arrives and leaves in the days you keep it. A spool that
is full makes requests fail with a spool error. #cmd("F OFTPD,D Q") lists the
spool, and #cmd("OFTP LIST") does it for a user.

== Stopping and Starting; Recovery <ops-recover>

#idx("recovery")
A planned stop with #cmd("P OFTPD") lets the sessions end. At the next start
everything that is in the spool goes on.

If the monitor ends abnormally, because it abends or because it is cancelled, a
session may have been in the middle of a file. At the next start OFTPD sets this
right and says so with message #cmd("OFTP040P"):

- A send that was in a session is put back to #cmd("WAITING"), and is sent again
  from the start.
- A receive that was not finished is discarded. The partly received data set is
  deleted and the transfer disappears from the spool. The partner has to send
  the file again, and as it has not had the receipt, it will.

The message #cmd("OFTP021W") says that the spool was not closed properly and that
OFTPD verified it. The messages of the previous run (#cmd("OFTP098E")) tell what
the abend was. Nothing else is needed.

If the monitor cannot give the SVC entry back (#cmd("OFTP098E") with the text
#cmd("NOT RESTORED"), #cmd("OFTP036W")), the SVC number stays reserved until the
next IPL. The next start then does not take the entry again (#cmd("OFTP033E"))
and runs without the cross-memory service. In that case, set another #cmd("SVC")
in the settings, or IPL.

== Reading the Log <ops-log>

#idx("log")
The log tells the story of every transfer. The lines below, shown without the
date and time that start each line, follow a file that is
queued and sent. The first line is the user's request, the others the call, the
session, and the two results: the partner has the file (#cmd("SENT")), and later
the receipt arrives (#cmd("DELIVERED")).

#screen(```
OFTP230P T0000012 QUEUED FOR MAC1: IBMUSER.REPORT.TEXT AS REPORT.TXT
OFTP206P MAC1: CALLING AT 192.0.2.10 PORT 3305
OFTP290P MAC1: session with MAC1: level 5, buffer 4096, credit 99, mode B
OFTP211P MAC1: T0000012 REPORT.TXT SENT, 120 RECORDS
OFTP213P MAC1: T0000012 REPORT.TXT DELIVERED
OFTP203P MAC1: SESSION ENDED, ESID 00 (PARTNER), FILES 1 OUT 0 IN
```)

A file that arrives leaves a shorter story.

#screen(```
OFTP205P 192.0.2.10: CALL RECEIVED
OFTP290P MAC1: session with MAC1: level 5, buffer 4096, credit 99, mode B
OFTP222P MAC1: T0000013 DAILY RECEIVED AS OFTP.IN.DAILY, 40 RECORDS
OFTP203P MAC1: SESSION ENDED, ESID 00 (PARTNER), FILES 0 OUT 1 IN
```)

The receipt for the file is sent in this session or the next one. The line
#cmd("OFTP203P") counts the files and responses of the session. When something
goes wrong, the line before the failure is usually the best clue. For a closer
look, switch the trace on for the partner with #cmd("F OFTPD,TRACE ON name").

== Troubleshooting <ops-trouble>

#idx("troubleshooting")
@ops-trouble-tab lists what you may see, what it means and what to do. The
messages are in the Reference.

#tab(caption: [Symptoms and what to do about them])[
  #table(columns: (1.7in, 1fr),
    [What you see], [Cause and action],
    [The utility says #cmd("OFTPD not reached: not running, or without SVC=")
      (#cmd("OFTP010E"))], [The monitor is not running, or the settings have no
      #cmd("SVC"), or the monitor is not authorized (#cmd("OFTP005W"),
      #cmd("OFTP035W")), or the SVC number was taken by something else
      (#cmd("OFTP033E")). The log of the start says which. See @ug-monitor.],
    [The utility says #cmd("needs ADMIN= in OFTPPRM")], [The command is for an
      administrator. Name the user with #cmd("ADMIN") in the settings and restart
      the monitor.],
    [The utility says #cmd("not authorized")], [The security system does not let
      the user read the data set (#cmd("OFTP238W")). Give the user access, or let
      a user who has it queue the file.],
    [A send stays #cmd("QUEUED")], [Several causes. The partner has no
      #cmd("HOST"), or has #cmd("CALL=N"): the file waits for the partner's call.
      The partner is disabled. The start time or the window of the send is not
      yet reached (#cmd("OFTP SHOW key")). As many sessions run as
      #cmd("MAXSESS") or #cmd("SESSIONS") allow. Check with #cmd("F OFTPD,D P"),
      and try #cmd("F OFTPD,CALL name").],
    [A send is #cmd("WAITING")], [Attempts fail. Look in the log for the partner:
      #cmd("OFTP207P") means it cannot be reached, #cmd("OFTP212P") that it
      refused the file, #cmd("OFTP290W") that the identification failed,
      #cmd("OFTP203P") with an ESID gives the reason the session ended.],
    [A send is #cmd("HELD") (#cmd("OFTP215W"))], [The attempts were used up. Remove
      the cause, then release it (see @ops-retry).],
    [A send is #cmd("REJECTED")], [The partner refused the file, or sent a
      negative receipt. #cmd("OFTP SHOW key") gives the reason code of the
      partner (#cmd("OFTP212P"), #cmd("OFTP213P")). Ask the partner what it means,
      put the cause right, and send again with #cmd("OFTP RETRY key").],
    [A send is #cmd("FAILED") (#cmd("OFTP210E"))], [OFTPD could not open the data
      set. Check the name, and that it is not in use. Then #cmd("OFTP RETRY key").],
    [A send stays #cmd("SENT")], [The partner has not sent its receipt. Ask the
      partner. If the receipt came for a file OFTPD does not know, it was ignored
      (#cmd("OFTP214P")).],
    [A partner is refused when it calls (#cmd("OFTP290W"), #cmd("OFTP201W"))],
      [Its Odette ID is not in the table, its password is wrong, or it calls from
      another address than #cmd("ADDRESS"). See @ug-partners.],
    [A partner is refused with the reason 08 (#cmd("OFTP202W"), #cmd("OFTP204W"))],
      [It has as many sessions as #cmd("SESSIONS") allows, or OFTPD has as many as
      #cmd("MAXSESS"). Raise the limit if it is regular.],
    [A received file is not in the list], [A received file belongs to nobody, so
      only an administrator sees it. Look at the data sets under
      #cmd("INPREFIX").],
    [A file is not received (#cmd("OFTP220E"), #cmd("OFTP221E"),
      #cmd("OFTP223W"))], [The data set could not be created or renamed, or the file
      is for another Odette ID. Check the space and the name, and #cmd("RELAY")
      for files in transit.],
    [A change to a partner does not take effect], [Partners change with
      #cmd("F OFTPD,REFRESH"). The settings change only after a restart. If the
      table is full (#cmd("OFTP015W")), take partners out of the member and
      refresh again\; a partner that a transfer in the spool still names stays in
      the table until that transfer is deleted.],
    [A file is received, but no job runs (#cmd("OFTP247W"))], [The receive rule of
      the file has a #cmd("JOB="), and the job was not submitted. The message gives
      the reason, for instance a member that is missing or empty, a line that is
      too long after the symbols went in, or a member that was changed while it
      was read. The file is kept and was acknowledged.
      #cmd("OFTP SHOW key") shows the same reason. Correct the member\; the next
      file uses the corrected one.],
    [A line of the members is reported (#cmd("OFTP010W"))], [Correct the line. The
      partner it belongs to is disabled until then.],
    [The messages are on the console only (#cmd("OFTP022W"),
      #cmd("OFTP023E"))], [The log cannot be used. See @ug-monitor.],
    [A session ends with an abend (#cmd("OFTP209E"))], [OFTPD cleans up and goes
      on. The transfer in the session counts as a failed attempt. Keep the dump
      and report it.],
    [The monitor does not stop (#cmd("OFTP096W"))], [A session does not end. If it
      does not on its own, CANCEL the monitor, and see @ops-recover.],
  )
] <ops-trouble-tab>
