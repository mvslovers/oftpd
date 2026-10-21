#import "../bookmaster/bookmaster.typ": *

= The OFTP Command <ref-cmd>

#idx("OFTP", "utility")#idx("utility")
The #cmd("OFTP") utility is the way users and administrators work with
OFTPD from TSO or from a batch job. It does not touch the spool or the
partner table itself\; it sends each command to the OFTPD monitor, which
carries it out and answers. The monitor must be running, and
#cmd("OFTPPRM") must have the #cmd("SVC") keyword, because the utility
reaches the monitor through that SVC. The commands are #cmd("SEND"),
#cmd("LIST"), #cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"), #cmd("RELEASE"),
#cmd("RETRY"), #cmd("CALL"), #cmd("PARTNERS"), #cmd("PARTNER") and
#cmd("PURGE").

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
the command on the next line. Columns 73 to 80 are ignored. A line holds at
most 20 words, and the utility ignores the rest. The utility writes each
command with the prefix #cmd("OFTP") before its answer. It cuts the echo into
lines of at most 72 columns, and ends a line that continues with a hyphen, as
#cmd("SYSIN") does. In the echo the value of #cmd("PASSWORD") and of
#cmd("OURPASSWORD") is replaced by eight asterisks, whatever its length. The
utility does not echo a command from the PARM string or from TSO. If
#cmd("SYSIN") holds no command, the utility writes #cmd("OFTP001E").

Operands are written #cmd("KEYWORD(value)") or #cmd("KEYWORD=value"). Keywords
and values are not case sensitive: the utility turns them to upper case before
it sends them. Single quotes around a value are removed. A keyword has up to 15
characters and a value up to 79, and the whole request, as the utility builds
it, up to 255 characters. If an operand or the request is longer, the utility
writes #cmd("OFTP003E"). OFTPD takes no value that contains a blank. A
word without a value is refused with #cmd("OFTP007E"), except #cmd("XLATE")
and #cmd("HOLD"), which stand for #cmd("Y"). The first lone word of a command
is the main operand: after #cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"),
#cmd("RELEASE") and #cmd("RETRY") the transfer key, after #cmd("CALL") and
#cmd("PARTNER") the partner, and after #cmd("PURGE") the number of days.

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
#cmd("CALL"), #cmd("PURGE") and the commands that change the partner table:
#cmd("PARTNER ADD"), #cmd("CHANGE"), #cmd("ENABLE"), #cmd("DISABLE") and
#cmd("DELETE"). Every user may use #cmd("PARTNERS"), #cmd("PARTNER LIST") and
#cmd("PARTNER SHOW"), but #cmd("PARTNER SHOW") hides two fields from a user who
is not an administrator (see @cmd-partner). A user who is not an administrator
and tries a restricted command gets the answer #cmd("needs ADMIN= in OFTPPRM")
and the return code 8. The utility itself has no #cmd("REFRESH") command\; the
ISPF dialog offers it to an administrator, and the operator has
#cmd("F OFTPD,REFRESH").

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
any\; and the last reason or text. If the receive rule of a file has a
#cmd("JOB="), a further line gives the result: #cmd("job submitted JOBnnnnn (JES job id)") or
#cmd("job not submitted:") and the reason (see @cfg-job-report).

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
sends changed. The partner of a send is the partner of the session, so a send
to a partner that is reached through another one belongs to the other one. The
name of a partner must be that of an enabled partner. #cmd("HOLD") takes the
sends that are queued or waiting, and #cmd("RELEASE") the sends that are held.
A send in a session is left as it is.

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
calls. The last line gives the number of partners. The list does not say
whether OFTPD reads a partner from #cmd("OFTPPTN") or keeps it by command\;
#cmd("PARTNER SHOW") does (@cmd-partner). #cmd("PARTNER LIST") is the same
command.

== PARTNER <cmd-partner>

#idx("PARTNER")#idx("partner table", "maintenance")#idx("partner kept by command")
#screen(```
OFTP PARTNER LIST
OFTP PARTNER SHOW name
OFTP PARTNER ADD name ODETTEID(id) [keyword(value)]...
OFTP PARTNER CHANGE name [keyword(value)]...
OFTP PARTNER ENABLE name
OFTP PARTNER DISABLE name
OFTP PARTNER DELETE name
```)

The command maintains the partner table while OFTPD runs. #cmd("ADD") and
#cmd("CHANGE") write the partner to the spool, so that the partner is in the
table again when OFTPD is started. This book calls it a partner kept by command.
It stands in the same table of 32 places as the partners of #cmd("OFTPPTN"), and
OFTPD uses it at once, without a #cmd("REFRESH"). @cfg-layers describes the
two layers of the table.

The name of the partner is the lone word after the subcommand, or
#cmd("PARTNER(name)"). It has 1 to 8 characters: the letters A to Z, the digits,
and the characters #cmd("\#"), #cmd("@"), #cmd("$"), #cmd("_") and
#cmd("-"). A partner cannot be renamed.

#deflist(width: 1.35in,
  [#cmd("LIST")], [Shows the table, as #cmd("PARTNERS") does (@cmd-partners).
    The utility ignores further operands.],
  [#cmd("SHOW")], [Shows the entry of one partner (@cmd-partner-show).],
  [#cmd("ADD")], [Adds a partner. #cmd("ODETTEID") is required.],
  [#cmd("CHANGE")], [Changes the keywords that the command names. All other
    values of the partner stay as they are.],
  [#cmd("ENABLE"), #cmd("DISABLE")], [Are #cmd("CHANGE") with
    #cmd("ENABLED(Y)") and #cmd("ENABLED(N)").],
  [#cmd("DELETE")], [Removes a partner that is kept by command.],
)

Only an administrator may use #cmd("ADD"), #cmd("CHANGE"), #cmd("ENABLE"),
#cmd("DISABLE") and #cmd("DELETE").

=== The Keywords of ADD and CHANGE <cmd-partner-kw>

The keywords are those of an entry of #cmd("OFTPPTN"), with the same meaning
(see @cfg-ptn). @cmd-partner-tab gives the allowed values and what #cmd("ADD")
uses for a keyword that the command leaves out. Where a value is #cmd("Y") or
#cmd("N"), #cmd("YES") and #cmd("NO") are allowed too.

#tab(caption: [The keywords of PARTNER ADD and PARTNER CHANGE])[
  #table(columns: (1.3in, 1.9in, 1fr),
    [Keyword], [Value], [Default for ADD],
    [#cmd("ODETTEID")], [Up to 25 characters.], [None. Required.],
    [#cmd("PASSWORD")], [Up to 8 characters.], [None.],
    [#cmd("OURPASSWORD")], [Up to 8 characters.], [None.],
    [#cmd("OURID")], [Up to 25 characters.], [The #cmd("ODETTEID") of
      #cmd("OFTPPRM").],
    [#cmd("HOST")], [Up to 63 characters.], [None.],
    [#cmd("PORT")], [1 to 65535.], [3305.],
    [#cmd("LEVEL")], [5, 4, 2 or 1.], [5.],
    [#cmd("SDEB")], [128 to 8192.], [The #cmd("SDEB") of #cmd("OFTPPRM").],
    [#cmd("CREDIT")], [1 to 999.], [The #cmd("CREDIT") of #cmd("OFTPPRM").],
    [#cmd("MODE")], [#cmd("S"), #cmd("R") or #cmd("B").], [#cmd("B").],
    [#cmd("COMPRESS")], [#cmd("Y") or #cmd("N").], [#cmd("N").],
    [#cmd("ENABLED")], [#cmd("Y") or #cmd("N").], [#cmd("Y").],
    [#cmd("CALL")], [#cmd("Y") or #cmd("N").], [#cmd("Y").],
    [#cmd("ADDRESS")], [Up to 15 characters.], [None.],
    [#cmd("XLATE")], [#cmd("Y") or #cmd("N").], [#cmd("N").],
    [#cmd("VIA")], [Up to 8 characters.], [None.],
    [#cmd("SESSIONS")], [1 to 16.], [1.],
  )
] <cmd-partner-tab>

The command differs from an entry of #cmd("OFTPPTN") in these points.

- A request is all or nothing. A keyword that is unknown, or a value that is not
  valid, makes OFTPD refuse the whole request, and nothing changes. In
  #cmd("OFTPPTN") OFTPD disables the partner and reads the rest of the member.
- A value cannot contain a blank or a comma. OFTPD takes either as the end of
  the value, and refuses the rest as a word that is not
  #cmd("KEYWORD=VALUE"). The answer gives the place of the word, counting from
  1, with the partner name as the first word. It never shows the word, because
  it can be part of a password.
- The utility turns the request to upper case. A #cmd("HOST") given by command
  is stored in upper case. In #cmd("OFTPPTN") OFTPD keeps the case of the host.
- An empty value, as in #cmd("HOST()") or #cmd("PASSWORD()"), removes the value
  of #cmd("PASSWORD"), #cmd("OURPASSWORD"), #cmd("HOST"), #cmd("ADDRESS") and
  #cmd("VIA"). For #cmd("OURID") it brings back the default. For any other
  keyword an empty value is not valid.
- A keyword that is given twice takes the value that comes last.
- OFTPD checks the finished entry before it stores it, and refuses it instead of
  disabling the partner. The Odette ID must not be the one of another partner,
  whether that one is enabled or not. A partner named by #cmd("VIA") must exist,
  be enabled and have no #cmd("VIA") of its own. A partner that others are
  reached through cannot have a #cmd("VIA") itself.
- The request is at most 255 characters long (see @cmd-run). A partner with many
  long values does not fit one #cmd("ADD") then. Add it with the keywords that it
  needs, and give the others in a #cmd("CHANGE").

=== What ADD Does <cmd-partner-add>

#cmd("ADD") creates the partner with the values it is given and the defaults of
@cmd-partner-tab for the others, and writes it to the spool. The partner is
enabled unless the command says #cmd("ENABLED(N)"). OFTPD writes the message
#cmd("OFTP243I") and answers #cmd("partner name added").

#cmd("ADD") is refused, and the table does not change, in these cases.

- The name is in the table already, whether the partner is enabled or disabled,
  kept by command or in #cmd("OFTPPTN"). The answer is
  #cmd("partner exists: use CHANGE").
- The Odette ID belongs to another partner, enabled or not. The answer is
  #cmd("ODETTEID is partner name's already"), with the name of that partner.
- The table is full, that is, all 32 places are in use. The answer is
  #cmd("the partner table is full").
- The spool cannot be written. The answer is #cmd("spool not available").

An enabled partner without a #cmd("PASSWORD") accepts every caller that gives its
Odette ID. #cmd("ADD") and #cmd("CHANGE") carry out the command in this case, but
warn (@cmd-partner-nopw).

=== What CHANGE Does <cmd-partner-change>

#cmd("CHANGE") starts from the entry the partner has, applies the keywords of the
command, and stores the result. If the result is the entry that the partner has,
OFTPD changes nothing, writes no message and answers
#cmd("partner name unchanged"). Otherwise it writes the entry to the spool, then
takes it into the table, and writes the message #cmd("OFTP244I"). The answer is
#cmd("partner name changed"). The command also ends the wait that follows a
call that failed.

If the partner is in #cmd("OFTPPTN") only, the first #cmd("CHANGE") stores the
whole entry in the spool. The partner is then kept by command, and its entry
replaces the one of #cmd("OFTPPTN"), which stays as it is. #cmd("PARTNER SHOW")
says so, and #cmd("REFRESH") does not take the entry of #cmd("OFTPPTN") back.

While a session with the partner runs, or OFTPD is calling the partner, only
#cmd("ENABLED") may change. A #cmd("CHANGE") that changes anything else is
refused with #cmd("the partner has a session now, try again").

OFTPD does not disable a partner while an enabled partner is reached through it.
The answer is #cmd("partner name is reached through it: disable that first"),
with the name of the partner that has the #cmd("VIA"). Disable that partner
first.

=== What DELETE Does <cmd-partner-delete>

#cmd("DELETE") removes a partner that is kept by command from the spool and the
table, and frees its place. OFTPD writes the message #cmd("OFTP245I") and answers
#cmd("partner name deleted"). The command goes in these steps and stops at the
first that fails.

+ A partner that is only in #cmd("OFTPPTN") cannot be deleted. The answer is
  #cmd("the partner is in OFTPPTN: remove it there, then REFRESH").
+ A partner with a session, or one that OFTPD is calling, is refused with
  #cmd("the partner has a session now, try again").
+ A partner that another partner is reached through is refused with
  #cmd("partner name is reached through it"), whether or not that other partner
  is enabled.
+ OFTPD disables an enabled partner, and writes this to the spool.
+ OFTPD counts the transfers in the spool that name the partner as the partner,
  as the one it is reached through, or as the origin, in every state, the
  finished ones too. If there are any, the answer is #cmd("n transfers name
  partner name: PURGE or DELETE them first"), and #cmd("(disabled)") follows the
  name if the command disabled the partner. The partner stays disabled. Delete
  the transfers with #cmd("DELETE") or #cmd("PURGE"), and give the command again.
+ OFTPD deletes the partner from the spool and the table.

If the partner changes while the count runs, the answer is
#cmd("the partner was changed meanwhile, try again"). If the spool cannot be
read or written, it is #cmd("spool not available"), and the partner may be
disabled.

If the partner is also in #cmd("OFTPPTN"), the entry of #cmd("OFTPPTN") takes
its place. OFTPD does this with a refresh of the partner table (see
@cfg-refresh), and the answer ends with #cmd("the OFTPPTN entry is back"). If
OFTPD cannot read #cmd("OFTPPTN"), it ends with #cmd("OFTPPTN cannot be read, its
entry is back after the next REFRESH").
The refresh also reads the receive rules again, as the operator command does
(chapter “Receive Rules”).

=== SHOW <cmd-partner-show>

#cmd("PARTNER SHOW") gives the entry of one partner in the lines of
@cmd-partner-show-fig. Every user may use it.

#fig(caption: [The answer to PARTNER SHOW])[
  #screen(```
PARTNER=MAC1 ODETTEID=O0000000000MAC1 OURID=O0000000000MYMVS
HOST=192.0.2.10 PORT=3305 LEVEL=5 MODE=B
ENABLED=Y CALL=Y SESSIONS=1 COMPRESS=N XLATE=N
SDEB=4096 CREDIT=99 ADDRESS= VIA=
PASSWORD=SET OURPASSWORD=NONE
source: kept by command; 0 sessions now
partner
  ```)
] <cmd-partner-show-fig>

OFTPD never shows a password. #cmd("PASSWORD") and #cmd("OURPASSWORD") say
#cmd("SET") or #cmd("NONE"). To a user who is not an administrator, #cmd("OURID")
and #cmd("ADDRESS") show a hyphen. #cmd("CALL") shows the value that was given.
It does not say that the partner has a host to call. The source line says
#cmd("OFTPPTN"), #cmd("kept by command") or #cmd("kept by command, overrides
OFTPPTN"), followed by the number of sessions with the partner now. The last
line is the final answer.

=== The Warning for a Missing Password <cmd-partner-nopw>

#idx("PASSWORD", "missing")
If #cmd("ADD") or #cmd("CHANGE") leaves an enabled partner without a
#cmd("PASSWORD"), OFTPD carries out the command and adds a warning. This
includes a #cmd("CHANGE") of any other keyword, and #cmd("ENABLE"), of such a
partner. The utility writes two lines and ends with the return code 4.

#screen(```
WARNING: without a PASSWORD every peer that knows the Odette ID of the partner can log on
partner PARTNER2 added, but it has no PASSWORD
```)

The warning has no message id. The second line reads #cmd("changed") in place
of #cmd("added") for #cmd("CHANGE"). Give the partner a password with
#cmd("PARTNER CHANGE name PASSWORD(pw)"), or disable the partner.

=== Answers and Return Codes <cmd-partner-rc>

@cmd-partner-refuse gives what the answer says, without the number that OFTPD
puts in front. The return codes are those of @cmd-rc-tab.

#tab(caption: [Answers of the PARTNER command that refuse or warn])[
  #table(columns: (2.6in, 0.4in, 1fr),
    [Answer], [RC], [Cause],
    [#cmd("needs ADMIN= in OFTPPRM")], [8], [The user is not an administrator.],
    [#cmd("PARTNER= is a name of 1 to 8 letters, digits, # @ $ _ -")], [8], [The
      name is missing, is longer than 8 characters or has another character.],
    [#cmd("no such partner")], [12], [#cmd("SHOW"), #cmd("CHANGE") or
      #cmd("DELETE") of a name that is not in the table. The utility writes it
      with #cmd("OFTP010E").],
    [#cmd("partner exists: use CHANGE")], [8], [#cmd("ADD") of a name that is
      in the table.],
    [#cmd("the partner table is full")], [8], [#cmd("ADD") with all 32 places in
      use.],
    [#cmd("unknown keyword k")], [8], [The keyword is not one of
      @cmd-partner-tab.],
    [#cmd("invalid value for k")], [8], [The value is outside the limits of
      @cmd-partner-tab.],
    [#cmd("word n is not KEYWORD=VALUE")], [8], [A blank or a comma in a value
      cut it short.],
    [#cmd("a word is longer than 99 characters")], [8], [A word of the request
      is too long.],
    [#cmd("a partner cannot be renamed")], [8], [The command gives a
      #cmd("PARTNER=") with another name.],
    [#cmd("ODETTEID is required")], [8], [The entry has no Odette ID.],
    [#cmd("ODETTEID is partner x's already")], [8], [Another partner has the
      Odette ID.],
    [#cmd("VIA names no enabled direct partner")], [8], [The partner named by
      #cmd("VIA") is not in the table, is disabled, has a #cmd("VIA") itself, or
      is the partner itself.],
    [#cmd("partner x is reached through it: it cannot have a VIA")], [8],
      [Partner x has a #cmd("VIA") that names this partner.],
    [#cmd("partner x is reached through it: disable that first")], [8],
      [#cmd("CHANGE") disables a partner that the enabled partner x is reached
      through.],
    [#cmd("partner x is reached through it")], [8], [#cmd("DELETE") of a partner
      that partner x is reached through.],
    [#cmd("n transfers name partner x: PURGE or DELETE them first")], [8],
      [#cmd("DELETE") of a partner that transfers in the spool name.],
    [#cmd("the partner is in OFTPPTN: remove it there, then REFRESH")], [8],
      [#cmd("DELETE") of a partner that is not kept by command.],
    [#cmd("the entry is too long")], [8], [The entry does not fit a spool
      record.],
    [#cmd("the partner has a session now, try again")], [4], [#cmd("CHANGE")
      of more than #cmd("ENABLED"), or #cmd("DELETE"), of a partner with a
      session.],
    [#cmd("the partner was changed meanwhile, try again")], [4],
      [#cmd("DELETE") found the partner changed during its count of the
      transfers.],
    [#cmd("spool not available")], [4], [OFTPD has no spool, or could not read
      or write it. A change that fails to write changes nothing.],
    [#cmd("partner x added, but it has no PASSWORD")], [4], [Done, with a
      warning (@cmd-partner-nopw).],
  )
] <cmd-partner-refuse>

The utility itself refuses #cmd("PARTNER") without a subcommand or with an
unknown one with the message #cmd("OFTP002E"), and a subcommand without a name
with the message #cmd("OFTP004E").

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
      meanwhile\; the partner has a session\; the partner changed meanwhile\;
      OFTPD is busy\; the spool or storage is not available\; a partner was
      added or changed, but it has no password.],
    [8], [An error in the command or in its data set: an operand is wrong or
      too long, the data set is not found, not allowed or cannot be opened,
      the state does not allow the change, it is not your transfer, the user
      is not an administrator, the command is unknown, or OFTPD refuses a
      partner command (@cmd-partner-refuse).],
    [12], [Severe: the partner is unknown, not enabled or cannot be called,
      or OFTPD is not reached or does not answer completely.],
  )
] <cmd-rc-tab>

With the return code 12 the utility writes the cause with the message
#cmd("OFTP010E"). The messages #cmd("OFTP001E"), #cmd("OFTP002E"),
#cmd("OFTP003E"), #cmd("OFTP004E") and #cmd("OFTP007E") are written by the
utility itself. They are described in the appendix “Messages”. The answers of
the #cmd("PARTNER") command are in @cmd-partner-refuse.

The answers of OFTPD that are not shown above are plain text. They include
the following.

#deflist(width: 2.6in,
  [#cmd("no such transfer")], [The key is not in the spool, or it is not the key
    of a transfer. The commands that take a key reach transfers only\; a key that
    does not begin with #cmd("T") gets this answer, whatever else the spool
    holds under it.],
  [#cmd("not your transfer")], [The transfer belongs to another user.],
  [#cmd("needs ADMIN= in OFTPPRM")], [The command is for administrators, and
    the user is not one.],
  [#cmd("unknown or disabled partner")], [The partner of #cmd("SEND") is not in
    the table or is not enabled. #cmd("unknown partner") is the same answer to
    #cmd("HOLD") or #cmd("RELEASE") with #cmd("PARTNER"), and
    #cmd("not a partner that can be called") the answer to #cmd("CALL") for a
    partner that is unknown, not enabled or has #cmd("CALL=N"). With a partner
    that is reached through another, the answer
    #cmd("the partner it is reached through is disabled") can come.],
  [#cmd("data set not found")], [The data set does not exist, or its name is
    not valid.],
  [#cmd("not authorized: ...")], [The user may not read the data set, or may
    not delete it with #cmd("DISP(DELETE)"). The refusal is also written to the
    console as message #cmd("OFTP238W").],
  [#cmd("name longer than 26, give VFN=")], [The default virtual file name is
    too long.],
  [#cmd("only a send can be held, released or retried")], [The transfer is a
    receive.],
  [#cmd("only a queued or waiting send can be held")], [#cmd("HOLD") for a send
    in another state.],
  [#cmd("not held")], [#cmd("RELEASE") for a send that is not held.],
  [#cmd("already queued")], [#cmd("RETRY") for a send that is queued.],
  [#cmd("transfer is active, try later")], [The transfer is in a session.],
  [#cmd("transfer changed meanwhile, try again")], [A session or another command
    changed the transfer between the read and the write.],
  [#cmd("give KEY= or PARTNER= (PARTNER=* for all)")], [#cmd("HOLD") or
    #cmd("RELEASE") without a key and without a partner.],
  [#cmd("DISP=DELETE takes a whole data set, not a member")], [#cmd("SEND")
    with #cmd("DISP(DELETE)") and a member.],
  [#cmd("data set cannot be opened")], [#cmd("SEND") found the data set, but
    OFTPD could not open it to read its first record.],
  [#cmd("KEEPDAYS=0 keeps everything: say DAYS=n")], [#cmd("PURGE") without
    #cmd("DAYS") when #cmd("KEEPDAYS") is 0.],
  [#cmd("spool not available")], [OFTPD could not use the spool.],
)
