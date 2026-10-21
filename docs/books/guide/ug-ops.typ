#import "../bookmaster/bookmaster.typ": *

= Operating OFTPD <ug-ops>

#idx("operation")
This chapter is for the people who keep OFTPD running: the operator at the
console and the administrator named in the settings. It starts from what you
want to do: stop the files to a partner for a while, find out why a file does not
go, make a file go again, clean up the spool, read the log, get through a
restart. Every task has a session from the examples of @intro-examples. The
sessions of this chapter use the partner MAC1 that @ug-partners set up, and the
monitor that sends is the one on MVS1.

Three kinds of people give commands, and what they may do differs.

- An *ordinary user* sees and changes his own transfers with #cmd("OFTP") (see
  @ug-transfer).
- An *administrator* is a user named with #cmd("ADMIN") in the settings. He may
  see and change the transfers of everybody, call a partner, clean the spool and
  maintain the partner table (see @ops-admin).
- The *operator* uses the console, #cmd("F OFTPD,…"), and needs no entry
  anywhere. A command from the console is the operator's own, so it acts on
  all transfers.

The commands are described in full in the _OFTPD Reference_, in the chapters
“The OFTP Command” and “Operator Commands”, and the messages in its appendix
“Messages”.

== Where a Transfer Stands <ops-life>

#idx("transfer", "states")
The spool holds one record for each transfer, and the state in that record tells
where the transfer stands. #cmd("OFTP LIST") shows it in the fourth column, and
#cmd("OFTP SHOW") adds a line in words. @ops-life-fig shows the ways a transfer
can go. A send is a file that is sent to a partner, a receive a file that a
partner sent.

#fig(caption: [The states of a transfer])[
  #screen(```
Send:     QUEUED --> ACTIVE --> SENT --> DELIVERED

          ACTIVE --> WAITING    an attempt failed
          WAITING --> ACTIVE    the next attempt, after the wait
          WAITING --> HELD      the attempts are used up
          QUEUED, WAITING --> HELD   HOLD
          HELD --> QUEUED       RELEASE
          any but QUEUED --> QUEUED  RETRY
          ACTIVE --> REJECTED   the partner refused the file
          SENT --> REJECTED     negative receipt
          ACTIVE --> FAILED     the data set cannot be opened

Receive:  ACTIVE --> RECEIVED --> CONFIRMED
  ```)
] <ops-life-fig>

Three of the states need somebody: #cmd("HELD") needs somebody to decide,
#cmd("REJECTED") needs an answer from the partner, and #cmd("FAILED") needs the
data set to be put right. A transfer in the state #cmd("QUEUED") or
#cmd("WAITING") is dealt with by OFTPD itself. A transfer that is
#cmd("ACTIVE") is in a session now, and cannot be changed until the session is over.

The finished states are #cmd("DELIVERED"), #cmd("CONFIRMED"), #cmd("REJECTED")
and #cmd("FAILED"). Only these are ever removed from the spool (see
@ops-keep).

== Stopping the Files to a Partner <ops-hold>

#idx("HOLD")#idx("RELEASE")
To stop the sends to a partner for a while, for instance when its system is
down for maintenance, hold them. A user holds his own sends to the partner, and
an administrator or the operator those of everybody. This is the user's view: a
file is queued, held, and listed.

#screen(raw(read("../ex/ug-ops/hold3.txt")))

#cmd("HOLD") changes the sends of the partner that are queued or waiting to the
state #cmd("HELD"), so that OFTPD does not call for them, and says how many it
changed. A send that is already in a session is not stopped. To hold one file
only, give its key instead of the partner, as in #cmd("OFTP HOLD T0000015"),
or narrow the hold with #cmd("VFN") to one virtual file name or to the names
that begin with a prefix, as in #cmd("OFTP HOLD PARTNER(MAC1) VFN(REPORT*)").

A hold covers the files that exist now. A file queued afterwards is queued as
usual, so hold again, or hold the files one by one. Here the hold finds nothing,
and the next send goes on its way.

#screen(raw(read("../ex/ug-ops/hold.txt")))

#cmd("RELEASE") queues the held files again, and counts their attempts from 0.

#screen(raw(read("../ex/ug-ops/release3.txt")))

The operator does the same at the console, and sees the answer there. The
operator may name #cmd("ALL") in place of the partner, which means every
partner. A partner that does not exist is refused. The same session shows the
housekeeping of @ops-keep.

#screen(raw(read("../ex/ug-ops/console-cmds.txt")))

The log of the monitor has the lines #cmd("OFTP233P") for what the utility did,
and the console shows #cmd("OFTP233I") for the operator's.

== A Partner That Is Not There <ops-down>

#idx("partner not reached")
A file for a partner whose system is down is not lost. This is the file of
@ug-transfer sent to MAC1 while the monitor on MAC1 is stopped. The send
works, since it only queues the file.

#screen(raw(read("../ex/ug-ops/down-send.txt")))

OFTPD calls MAC1, and the call fails. The list shows that the file is waiting,
and #cmd("SHOW") says why.

#screen(raw(read("../ex/ug-ops/down-list.txt")))

The other files in the list are those that were delivered earlier. The log has the
story, with the date and time of each line. The call at 17:23:18 failed after five
seconds. HERC01 then held the file, and a little later asked for it to be sent
at once.

#screen(raw(read("../ex/ug-ops/notreached-log.txt")))

Nothing needs to be done while a file is #cmd("WAITING"): OFTPD calls again after
a wait that grows with every attempt, and the file goes as soon as the partner
answers. @ops-retry says how long it tries. If you know that the partner will not
be back for some time, hold its files (@ops-hold) so that OFTPD does not call
in vain.

== Trying Again <ops-retry>

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

- #cmd("OFTP RELEASE key"), #cmd("OFTP RELEASE PARTNER(name)") or
  #cmd("F OFTPD,RELEASE partner") queues it again, and
  counts its attempts from 0. Do this when the cause is removed, for instance when
  the partner is back.
- #cmd("OFTP RETRY key") does the same for one transfer, whatever state it is
  in, except that it is already queued or in a session. It also sends a file
  again that has been sent or delivered, so the partner sees it twice, and a
  file that was rejected or failed once the cause is put right.
- #cmd("OFTP DELETE key") removes it.

This is #cmd("RETRY") on the file of @ops-down.

#screen(raw(read("../ex/ug-ops/release2.txt")))

The transfer is queued again with no attempts, and the state line says
#cmd("queued again"). #cmd("RELEASE") has nothing to do after that.

A partner that refuses a call because it has no free session (reason 08) is a
different case, as it says only that the partner is busy. OFTPD calls it again
after a short pause of its own, between 5 and 60 seconds, and does not count it
as a failed attempt (message #cmd("OFTP218P")). Two OFTPD monitors that call each
other can meet this way, and it sorts itself out.

If you have set #cmd("MAXRETRY=0"), OFTPD never gives up and never holds a file.

== Tasks for an Administrator <ops-admin>

#idx("administrator")
A user named with #cmd("ADMIN") in the settings may also use the following.
A user who is not named there gets the answer #cmd("needs ADMIN= in OFTPPRM").

- #cmd("OFTP CALL partner") calls a partner at once, as the operator command
  does. The log shows what comes of it.

  #screen(raw(read("../ex/ug-ops/call-log.txt")))

  Here the partner had no file to send and nothing was queued, so the session
  ended with no files in either direction.
- #cmd("OFTP PURGE DAYS(n)") deletes the finished transfers older than n days from
  the spool. Without #cmd("DAYS") it uses the #cmd("KEEPDAYS") of the settings.
  Only the spool is cleaned\; no data set is deleted (see @ops-keep).
- #cmd("OFTP LIST") shows all transfers, including the files received (see
  @recv-list).
- #cmd("OFTP PARTNER ADD"), #cmd("CHANGE"), #cmd("ENABLE"), #cmd("DISABLE") and
  #cmd("DELETE") maintain the partner table without an edit of the member (see
  @ptn-cmd), and the ISPF dialog does the same from panels (see @ug-dialog).
- #cmd("OFTP HOLD"), #cmd("RELEASE"), #cmd("RETRY") and #cmd("DELETE") act on
  the transfers of everybody, not only on his own.

== Cleaning Up <ops-keep>

#idx("KEEPDAYS")#idx("housekeeping")#idx("PURGE")
A transfer stays in the spool when it is finished, so that you can see what
became of it. OFTPD removes it after #cmd("KEEPDAYS") days (7 by default) since
its last change. It does this once a day, the first time when it has just
started. The message #cmd("OFTP235I") says how many transfers it deleted, and it
is written only when there were some. #cmd("KEEPDAYS=0") keeps the transfers for
ever.

You can do it by hand at any time, as the console session of @ops-hold has
shown\; the first #cmd("PURGE") there found nothing older than 30 days, and the
second, with 0, deleted all five finished transfers.

#screen(```
F OFTPD,PURGE 30
OFTP PURGE DAYS(30)
```)

The first is for the operator, the second for an administrator. Both delete the
finished transfers older than 30 days. #cmd("PURGE 0") deletes all finished
transfers, however recent.

Housekeeping deletes records of the spool, nothing else. The data sets of the
files that were received, and the data sets that were sent, are never deleted by
housekeeping. It is your job to clean up the data sets under #cmd("INPREFIX"), and the
data sets that you sent if you did not use #cmd("DISP(DELETE)"). A transfer that
is not finished (queued, waiting, held, sent, received) is not deleted by
housekeeping, however old. A file that is #cmd("SENT") without the receipt ever
coming stays until somebody deletes it, so look at the old ones now and then.

Size the spool for what arrives and leaves in the days you keep it. A spool that
is full makes requests fail with a spool error. #cmd("F OFTPD,D Q") lists the
spool, and #cmd("OFTP LIST") does it for a user. In #cmd("OFTP110I") the second
column says whether it is a send (S) or a receive (R), and the third is the
state by its first letter, except that J is #cmd("REJECTED"): Q queued, A active,
W waiting, H held, S sent, D delivered, F failed, R received, C confirmed.

== Reading the Log <ops-log>

#idx("log")
The log tells the story of every transfer. @xfer-follow shows the log of a file
that is sent, and @recv-log that of a file that is received. When something goes
wrong, the line before the failure is usually the best clue. The lines of the
class P are in the log only\; a message with the class I, W or E is on the
console too.

For a closer look, switch the trace on. #cmd("TRACE ON") with a partner traces
the sessions of that partner, without one all sessions, and #cmd("TRACE OFF") ends
it. #cmd("F OFTPD,D P") shows which partners are traced.

#screen(raw(read("../ex/ug-ops/trace-on.txt")))

The next send to MAC1 then writes every command of the protocol to the log, in
the order it went. Read this session from the top. Lines with #cmd("SEND") are
what OFTPD sent, lines with #cmd("RECV") what the partner sent, and the number
after the name of the command is its length in octets.

#screen(raw(read("../ex/ug-ops/trace-log.txt")))

- #cmd("SSID") is the log-on, one from each side, with the Odette ID, the level
  of the protocol, the size of the buffer and the credit that each side offers.
  The line #cmd("session with MAC1") that follows is what the two sides agreed
  on.
- #cmd("SFID") offers the file to the partner, with its virtual file name and the
  format of the records. #cmd("SFPA") is the partner's yes, with the point at
  which to begin (0 for a new file).
- #cmd("DATA") is the file itself. The trace shows only how many buffers and
  octets went, never the data. #cmd("EFID") ends the file, with the number of
  records and octets, and #cmd("EFPA") is the partner's yes.
- #cmd("EERP") is the receipt, the partner's proof that the file is in its hands.
  It is why the file turns #cmd("DELIVERED") in the line #cmd("OFTP213P").
- #cmd("RTR"), #cmd("CD") and #cmd("ESID") end the session. #cmd("ESID") carries
  the reason, 00 for a normal end.

The Reference has a table of these protocol commands and what each is for.

The trace shows no password and no data, so a log with the trace on may be sent to the partner
or to the maintainers. Switch it off when you have seen what you wanted
to see\; it makes the log long.

#screen(raw(read("../ex/ug-ops/trace-off.txt")))

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
- If the receive was replacing a data set (#cmd("REPLACE=Y")) and the stop came
  between the two renames, OFTPD puts the old data set back, or tells you to
  delete its backup (#cmd("OFTP239I"), #cmd("OFTP248W")). The chapter “Receive Rules” of the Reference describes this.

The message #cmd("OFTP021W") says that the spool was not closed properly and that
OFTPD verified it. The messages of the previous run (#cmd("OFTP098E")) tell what
the abend was. Nothing else is needed.

If the monitor cannot give the SVC entry back (#cmd("OFTP098E") with the text
#cmd("NOT RESTORED"), #cmd("OFTP036W")), the SVC number stays reserved until the
next IPL. The next start then does not take the entry again (#cmd("OFTP033E"))
and runs without the cross-memory service. In that case, set another #cmd("SVC")
in the settings, or IPL.


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
    [A file is refused because of a receive rule (#cmd("OFTP224W"),
      #cmd("OFTP225W"), #cmd("OFTP234W"))], [The name that the rule makes is not
      a valid data set name, or the data set exists and the rule has
      #cmd("REPLACE=N"), or the data set is not a plain sequential one and the
      rule has #cmd("REPLACE=Y"). The chapter “Receive Rules” of the Reference explains each.],
    [A rule has no effect (#cmd("OFTP025E"))], [The member has errors and was not
      used. #cmd("OFTP010W") names each line. After a refresh the old rules stay.
      Correct the member and refresh again.],
    [A change to a partner does not take effect], [A change in the member
      #cmd("OFTPPTN") needs #cmd("F OFTPD,REFRESH"). The settings change only
      after a restart. A partner kept by command is not changed by the member:
      it wins by name, and only the #cmd("PARTNER") command changes it (see
      @ptn-cmd). If the table is full (#cmd("OFTP015W")), delete a partner
      that is kept by command or take an entry out of the member, and use
      #cmd("REFRESH")\; a partner that a transfer in the spool still names stays
      in the table until that transfer is deleted.],
    [A #cmd("PARTNER") command is refused], [The answer, the last line of the
      output, says why. #cmd("needs ADMIN= in OFTPPRM") means that the user is
      not an administrator\; #cmd("the partner has a session now, try again")
      means that a partner with a session cannot be changed or deleted\; and
      #cmd("transfers name partner") means that a partner with
      transfers in the spool cannot be deleted\; and
      #cmd("the partner was changed meanwhile, try again") means that the
      partner changed while OFTPD looked through the spool, so repeat the
      command. The Reference lists the
      answers, in the chapter “The OFTP Command”.],
    [A file is received, but no job runs (#cmd("OFTP247W"))], [The receive rule of
      the file has a #cmd("JOB="), and the job was not submitted. The message gives
      the reason, for instance a member that is missing or empty, a line that is
      too long after the symbols went in, or a member that was changed while it
      was read. The file is kept and was acknowledged.
      #cmd("OFTP SHOW key") shows the same reason. Correct the member\; the next
      file uses the corrected one.],
    [A line of the members is reported (#cmd("OFTP010W"))], [Correct the line. In
      #cmd("OFTPPRM") or #cmd("OFTPPTN") the partner it belongs to is disabled
      until then\; a line in error in #cmd("OFTPRCV") stops the whole member from
      being used.],
    [The messages are on the console only (#cmd("OFTP022W"),
      #cmd("OFTP023E"))], [The log cannot be used. See @ug-monitor.],
    [A session ends with an abend (#cmd("OFTP209E"))], [OFTPD cleans up and goes
      on. The transfer in the session counts as a failed attempt. Keep the dump
      and report it.],
    [The monitor does not stop (#cmd("OFTP096W"))], [A session does not end. If it
      does not on its own, CANCEL the monitor, and see @ops-recover.],
  )
] <ops-trouble-tab>
