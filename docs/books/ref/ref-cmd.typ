#import "../bookmaster/bookmaster.typ": *

= The OFTP Command <ref-cmd>

#idx("OFTP", "utility")#idx("utility")
The #cmd("OFTP") utility is the way users and administrators work with
OFTPD from TSO or from a batch job. It does not touch the spool or the
partner table itself\; it sends each command to the OFTPD monitor, which
carries it out and answers. The monitor must be running, and
#cmd("OFTPPRM") must have the #cmd("SVC") keyword, because the utility
reaches the monitor through that SVC.

== Running the Utility <cmd-run>

The utility runs in three ways.

- As a TSO command: #cmd("OFTP command"). The answer is written to the
  terminal.
- In a batch job with the command in the PARM string, #cmd("EXEC
  PGM=OFTP,PARM='command'"). The answer goes to #cmd("SYSPRINT").
- In a batch job without PARM, with the commands in #cmd("SYSIN").
  The answer goes to #cmd("SYSPRINT").

The OFTPD load library must be in the STEPLIB, or in the link list.

In #cmd("SYSIN") there is one command to a line. A line with an asterisk in
column 1 is a comment. A hyphen as the last character of a line continues
the command on the next line. Columns 73 to 80 are ignored. The utility
writes each command with the prefix #cmd("OFTP") before its answer. If
#cmd("SYSIN") holds no command, the utility writes #cmd("OFTP001E").

Operands are written #cmd("KEYWORD(value)") or #cmd("KEYWORD=value"). Keywords
and values are not case sensitive. Single quotes around a value are removed.
After #cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"), #cmd("RELEASE")
and #cmd("RETRY") a lone word is taken as the transfer key, after
#cmd("CALL") as the partner, and after #cmd("PURGE") as the number of days.

#screen(```
OFTP SHOW T0000012
OFTP CALL MAC1
OFTP LIST PARTNER(MAC1) STATUS(HELD)
```)

In the command formats, square brackets enclose an operand that may be left
out, braces enclose a choice of which one must be taken, and a bar separates
the choices. Words in lower case stand for a value that you supply.

== Who May Do What <cmd-auth>

#idx("authority")
OFTPD knows the user ID of the caller by itself. A user sees and changes only
the transfers that the user queued. An administrator, a user named by an
#cmd("ADMIN") keyword of #cmd("OFTPPRM"), sees and changes every transfer,
including received files, which have no owner. Only an administrator may use
#cmd("CALL") and #cmd("PURGE"). The utility itself has no #cmd("REFRESH")
command\; the ISPF dialog offers it to an administrator.

== SEND <cmd-send>

#idx("SEND")
#screen(```
OFTP SEND PARTNER(name) DSN(data.set[(member)]) [VFN(name)]
     [FORMAT(T)] [XLATE] [HOLD] [USER(text)] [DISP(KEEP|DELETE)]
     [STRTDATE(*|*+n|ddd)] [STRTTIME(hh:mm)] [SENDWIN(hh:mm-hh:mm)]
```)

#cmd("SEND") puts a file in the spool to be sent to a partner.

#deflist(width: 1.35in,
  [#cmd("PARTNER")], [The name of the partner, as in the partner table. It
    must be enabled. A partner that is reached through another partner is
    allowed\; the file then goes in a session with that other partner,
    addressed to the Odette ID of the partner named.],
  [#cmd("DSN")], [The sequential data set, or the member of a partitioned data
    set, to send. Give the full data set name. The user must be allowed to
    read it. A data set that is empty is queued all the same, with a warning.],
  [#cmd("VFN")], [The virtual file name that the partner sees, up to 26
    characters. Default: the member name, or else the data set name. If that is
    longer than 26 characters, #cmd("VFN") is required.],
  [#cmd("FORMAT")], [#cmd("T") sends the file as text. Without it the file is
    sent in the format of its records.],
  [#cmd("XLATE")], [Translates the data between EBCDIC and ASCII.],
  [#cmd("HOLD")], [Queues the file in the state held. It is not sent until
    it is released.],
  [#cmd("USER")], [A text of up to 8 characters, without blanks, that goes with
    the file to the partner as the user data.],
  [#cmd("DISP")], [#cmd("DELETE") deletes the data set after the partner has
    confirmed the delivery. It takes a whole data set, not a member, and the
    user must be allowed to delete it. Default #cmd("KEEP").],
  [#cmd("STRTDATE")], [The day before which the file is not sent: #cmd("*") for
    now, #cmd("*+n") for n days from now (n from 0 to 9), or a day of the
    year #cmd("ddd") from 001 to 366. A day that has passed this year means
    that day next year.],
  [#cmd("STRTTIME")], [The time of day before which the file is not sent, as
    #cmd("hh:mm"). Without #cmd("STRTDATE") it is today. Without
    #cmd("STRTTIME") the day starts at the time of the command.],
  [#cmd("SENDWIN")], [A window, #cmd("hh:mm-hh:mm"), outside of which the file
    is not sent. The window can wrap over midnight. The two times must
    differ. #cmd("00:00-24:00") means no window. A send that is already in a
    session is not stopped when the window ends.],
)

The answer gives the key of the new transfer, which is the name to use in
later commands, as in @cmd-send-ex.

#fig(caption: [The answer to SEND])[
  #screen(```
T0000012 queued for MAC1 as BATCH.TEST2
```)
] <cmd-send-ex>

With a partner that is reached through another, the answer reads
#cmd("for name via partner"). With #cmd("HOLD") it says #cmd("queued and held").
For an empty data set it ends with #cmd("-- the data set is empty") and the
return code is 4.

The times are the local time of the system. A file that is queued, or that
waits for a retry, is sent when its start time has come and the time of day
is inside the window.

== LIST <cmd-list>

#idx("LIST")
#screen(```
OFTP LIST [PARTNER(name)] [STATUS(state)] [MINE]
```)

#cmd("LIST") shows the transfers in the spool, in key order, sends and
receives. #cmd("PARTNER") selects one partner, which also matches a partner
that is reached through another. #cmd("STATUS") selects the transfers whose
state starts with the given text, for example #cmd("STATUS(HELD)") or
#cmd("STATUS(DELIV)"). The states are in @cmd-states. #cmd("MINE") shows only
your own transfers\; for a user who is not an administrator this is always
so.

Each line gives the key, #cmd("SEND") or #cmd("RECV"), the state, the
partner, the virtual file name and the data set. The last line gives the
number of transfers.

#fig(caption: [The answer to LIST])[
  #screen(```
T0000012 SEND QUEUED    MAC1     BATCH.TEST2     IBMUSER.OFTPD.SAMPLE
T0000013 RECV CONFIRMED MAC1     REPORT          OFTP.IN.REPORT
2 transfers
```)
]

#tab(caption: [The states of a transfer])[
  #table(columns: (1.2in, 1fr),
    [State], [Meaning],
    [#cmd("QUEUED")], [A send, waiting for a session with the partner.],
    [#cmd("ACTIVE")], [In a session now.],
    [#cmd("WAITING")], [A send that failed and waits for the next attempt.],
    [#cmd("HELD")], [A send that an operator or user holds, or that OFTPD gave
      up after #cmd("MAXRETRY") attempts.],
    [#cmd("SENT")], [A send that is complete. The delivery notification from the
      partner is outstanding.],
    [#cmd("DELIVERED")], [The partner confirmed the delivery.],
    [#cmd("REJECTED")], [The partner refused the file, or refused it with a
      negative receipt. A file that OFTPD was relaying is also rejected when
      the attempts are used up.],
    [#cmd("FAILED")], [OFTPD could not open the data set to be sent (message
      #cmd("OFTP210E")).],
    [#cmd("RECEIVED")], [A file was received. The delivery notification is not
      yet sent.],
    [#cmd("CONFIRMED")], [A file was received and the delivery notification
      sent.],
  )
] <cmd-states>

The states #cmd("DELIVERED"), #cmd("CONFIRMED"), #cmd("REJECTED") and
#cmd("FAILED") are the finished ones. They are the ones that #cmd("PURGE") and
#cmd("KEEPDAYS") remove from the spool.

== SHOW <cmd-show>

#idx("SHOW")
#screen(```
OFTP SHOW key
```)

#cmd("SHOW") gives the details of one transfer: the direction, state, partner
and owner\; the virtual file, its date and time, format and record length\;
the data set\; the Odette IDs of the originator and the destination, the
records and the attempts\; the start time and the window, if there are
any\; and the last reason or text.

== DELETE, HOLD, RELEASE and RETRY <cmd-change>

#idx("DELETE")#idx("HOLD")#idx("RELEASE")#idx("RETRY")
#screen(```
OFTP DELETE key
OFTP HOLD key
OFTP RELEASE key
OFTP RETRY key
OFTP HOLD PARTNER({name|*}) [VFN({name|prefix*})]
OFTP RELEASE PARTNER({name|*}) [VFN({name|prefix*})]
```)

These commands change one transfer, given by its key.

#deflist(width: 1.1in,
  [#cmd("DELETE")], [Removes the transfer from the spool. It does not delete
    any data set. A transfer in a session cannot be deleted.],
  [#cmd("HOLD")], [Holds a send that is queued or waiting.],
  [#cmd("RELEASE")], [Queues a held send again, with the count of attempts at
    0.],
  [#cmd("RETRY")], [Queues a send again, from any state but queued. A send
    in the state #cmd("SENT") is sent once more, so the partner sees the
    file twice.],
)

#cmd("HOLD") and #cmd("RELEASE") with #cmd("PARTNER") change all the sends to
one partner, or to all partners with #cmd("*"), that the user may change.
#cmd("VFN") narrows them to one virtual file name, or to the names that start
with a prefix when it ends with an asterisk. The answer gives the number of
sends changed.

A change fails if the transfer is in a session (try again later), or has
changed meanwhile. #cmd("HOLD"), #cmd("RELEASE") and #cmd("RETRY") apply to
sends only.

== CALL <cmd-call>

#idx("CALL")
#screen(```
OFTP CALL partner
```)

For an administrator only. The command makes OFTPD call the partner now,
as the operator command #cmd("CALL") does. The answer says that the call is
requested, not that it succeeded.

== PARTNERS <cmd-partners>

#idx("PARTNERS")
#screen(```
OFTP PARTNERS
```)

The command shows the partner table: for each partner the name, the Odette
ID, #cmd("ENABLED") or #cmd("DISABLED"), #cmd("BUSY") if a session runs and
otherwise #cmd("IDLE"), the protocol level, and the host, or #cmd("via") and
the other partner, or #cmd("(not called)") for a partner that OFTPD never
calls. The last line gives the number of partners.

== PURGE <cmd-purge>

#idx("PURGE")
#screen(```
OFTP PURGE [DAYS(n)]
```)

For an administrator only. The command deletes the finished transfers from
the spool whose last change is older than #cmd("DAYS") days, from 0 to 9999.
Without #cmd("DAYS") it uses #cmd("KEEPDAYS") of #cmd("OFTPPRM"), and it
refuses if that is 0. #cmd("PURGE DAYS(0)") deletes every finished transfer,
however recent. The answer gives the number of transfers deleted. No data set
is deleted.

== Return Codes <cmd-rc>

#idx("return codes")
The utility ends with one of the return codes of @cmd-rc-tab. When the
commands come from #cmd("SYSIN"), it ends with the highest.

#tab(caption: [Return codes of the OFTP utility])[
  #table(columns: (0.8in, 1fr),
    [Code], [Meaning],
    [0], [The command was carried out.],
    [4], [A warning, or refused for now: the data set is empty\; there is no
      such transfer\; the transfer is in a session\; the transfer changed
      meanwhile\; OFTPD is busy\; the spool or storage is not available.],
    [8], [An error in the command or in its data set: an operand is wrong or
      too long, the data set is not found, not allowed or cannot be opened,
      the state does not allow the change, it is not your transfer, the user
      is not an administrator, or the command is unknown.],
    [12], [Severe: the partner is unknown, not enabled or cannot be called,
      or OFTPD is not reached or does not answer completely.],
  )
] <cmd-rc-tab>

With the return code 12 the utility writes the cause with the message
#cmd("OFTP010E"). The messages #cmd("OFTP001E"), #cmd("OFTP002E") and
#cmd("OFTP003E") are written by the utility itself. They are described in
the appendix “Messages”.

The answers of OFTPD that are not shown above are plain text. They include
the following.

#deflist(width: 2.6in,
  [#cmd("no such transfer")], [The key is not in the spool.],
  [#cmd("not your transfer")], [The transfer belongs to another user.],
  [#cmd("needs ADMIN= in OFTPPRM")], [The command is for administrators, and
    the user is not one.],
  [#cmd("unknown or disabled partner")], [The partner of #cmd("SEND") is not in
    the table or is not enabled.],
  [#cmd("data set not found")], [The data set does not exist, or its name is
    not valid.],
  [#cmd("not authorized: ...")], [The user may not read the data set, or may
    not delete it with #cmd("DISP(DELETE)"). The refusal is also written to the
    console as message #cmd("OFTP238W").],
  [#cmd("name longer than 26, give VFN=")], [The default virtual file name is
    too long.],
  [#cmd("only a send can be held, released or retried")], [The transfer is a
    receive.],
  [#cmd("transfer is active, try later")], [The transfer is in a session.],
  [#cmd("spool not available")], [OFTPD could not use the spool.],
)
