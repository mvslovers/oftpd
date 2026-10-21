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
the command on the next line. Columns 73 to 80 are ignored. From
#cmd("SYSIN") the utility reads at most 20 words of a command, and does not
report any that follow. The
utility writes each command with the prefix #cmd("OFTP") before its answer. If
#cmd("SYSIN") holds no command, the utility writes #cmd("OFTP001E").

Operands are written #cmd("KEYWORD(value)") or #cmd("KEYWORD=value"). Keywords
and values are not case sensitive. Single quotes around a value are removed.
After #cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"), #cmd("RELEASE")
and #cmd("RETRY") a lone word is taken as the transfer key, after
#cmd("CALL") as the partner, and after #cmd("PURGE") as the number of days.
After #cmd("PARTNER") and its sub-command a lone word is the partner.

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
#cmd("CALL") and #cmd("PURGE"), and the sub-commands of #cmd("PARTNER") that
change the partner table. Every user may use #cmd("PARTNERS"),
#cmd("PARTNER LIST") and #cmd("PARTNER SHOW"). The utility itself has no
#cmd("REFRESH") command\; the ISPF dialog offers it to an administrator.

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

== PARTNER <cmd-partner>

#idx("PARTNER")#idx("partner table", "maintenance")
#screen(```
OFTP PARTNER LIST
OFTP PARTNER SHOW name
OFTP PARTNER ADD name ODETTEID(id) [operand ...]
OFTP PARTNER CHANGE name [operand ...]
OFTP PARTNER ENABLE name
OFTP PARTNER DISABLE name
OFTP PARTNER DELETE name
```)

The command shows and maintains the partner table of the running OFTPD, with
no restart and no edit of #cmd("OFTPPTN"). A partner that is added or changed
with it is kept by command: OFTPD stores it in the spool, and it is there
after the next start. @cfg-cmd explains how these partners and the entries
of #cmd("OFTPPTN") live together. The name of the partner is the first word
after the sub-command. It has 1 to 8 letters, digits or the characters
#cmd("# @ $ _ -").

- #cmd("LIST") shows the partner table as the command #cmd("PARTNERS") does,
  see @cmd-partners.
- #cmd("SHOW") shows the entry of one partner, and where it comes from.
- #cmd("ADD") puts a new partner in the table.
- #cmd("CHANGE") changes the operands that are given\; the others keep their
  value.
- #cmd("ENABLE") and #cmd("DISABLE") are a #cmd("CHANGE") of the one operand
  #cmd("ENABLED").
- #cmd("DELETE") takes a partner out of the table and deletes it from the
  spool.

Only an administrator may use #cmd("ADD"), #cmd("CHANGE"), #cmd("ENABLE"),
#cmd("DISABLE") and #cmd("DELETE"). #cmd("LIST") and #cmd("SHOW") are open to
every user.

=== The Operands of ADD and CHANGE <cmd-partner-ops>

The operands are the keywords of an entry of #cmd("OFTPPTN"), written
#cmd("KEYWORD(value)") or #cmd("KEYWORD=value"). @cfg-ptn describes each one:
#cmd("ODETTEID"), #cmd("PASSWORD"), #cmd("OURPASSWORD"), #cmd("OURID"),
#cmd("HOST"), #cmd("PORT"), #cmd("LEVEL"), #cmd("MODE"), #cmd("COMPRESS"),
#cmd("CALL"), #cmd("SESSIONS"), #cmd("ADDRESS"), #cmd("VIA"),
#cmd("XLATE"), #cmd("SDEB"), #cmd("CREDIT") and #cmd("ENABLED"). The rules and
the defaults are the same. A partner cannot be renamed.

- #cmd("ADD") needs #cmd("ODETTEID"). An operand left out takes its default.
- With #cmd("CHANGE") an operand that is left out keeps its value. A value in
  empty parentheses, such as #cmd("HOST()"), is accepted and stores an empty
  value, which clears the host, the address, the via or a password.
- Write every operand with its value. A lone word after the name, such as
  #cmd("COMPRESS") without parentheses, is read as the partner name once more
  and refused with the answer #cmd("a partner cannot be renamed").
- OFTPD turns the whole request into upper case. A password and a
  #cmd("HOST") that are given with the command are stored in upper case,
  although a #cmd("HOST") in #cmd("OFTPPTN") keeps its case.

The utility sends the command to OFTPD in one request of at most 255
characters. If the operands do not fit, the utility writes #cmd("OFTP003E").
Then change the partner in two commands.

OFTPD checks the entry before it stores anything. It refuses an entry that has
no Odette ID, an Odette ID that another partner has, a #cmd("VIA") that does
not name an enabled direct partner, a #cmd("VIA") for a partner through which
others are reached, and any value that @cfg-ptn does not allow. If the stored
form of the entry would not fit a record of the spool, it refuses that too.
The answer begins with the code 508 and names the reason. Nothing is changed
when a command is refused.

A partner that has a session, or that OFTPD is calling, can be enabled and
disabled, but nothing else of it can be changed\; #cmd("CHANGE") is refused
with the code 450 and can be repeated later. #cmd("CHANGE") is also refused
with the code 512 if it would disable a partner through which an enabled
partner is reached (see @cfg-via). A #cmd("CHANGE") that would change nothing is
answered with #cmd("partner name unchanged").

If the partner is enabled after #cmd("ADD"), #cmd("CHANGE") or #cmd("ENABLE")
and has no #cmd("PASSWORD"), the command is carried out, but the answer ends
with #cmd(", but it has no PASSWORD") (the code 210), and a line before it
warns that every peer that knows the Odette ID of the partner can log on. If the
partner has an #cmd("ADDRESS"), the line says instead that every peer that
calls from that address and knows the Odette ID can log on. Set a password with
#cmd("CHANGE"). @cfg-nopw explains what a partner without a password allows.

The warning is given when the command stores an entry that is enabled and has
no #cmd("PASSWORD"). #cmd("ENABLE") is a #cmd("CHANGE") of #cmd("ENABLED"), so
it warns when it enables such a partner. There is no warning when the partner
has a #cmd("PASSWORD") after the command, when it is disabled, and when the
command changes nothing (#cmd("unchanged")). For a partner with a #cmd("VIA")
the warning is only a hint, because that partner is never matched at log-on
(see @cfg-via). A partner of the member #cmd("OFTPPTN") without
#cmd("PASSWORD") gets no warning, because reading the member is not a command.

=== SHOW <cmd-partner-show>

#idx("PARTNER", "SHOW")
#cmd("SHOW") writes these lines. Each of the first five has the keywords and
the values that the partner has now, with the defaults filled in\: the name
with #cmd("ODETTEID") and #cmd("OURID")\; #cmd("HOST"), #cmd("PORT"),
#cmd("LEVEL") and #cmd("MODE")\; #cmd("ENABLED"), #cmd("CALL"),
#cmd("SESSIONS"), #cmd("COMPRESS") and #cmd("XLATE")\; #cmd("SDEB"),
#cmd("CREDIT"), #cmd("ADDRESS") and #cmd("VIA")\; and #cmd("PASSWORD") and
#cmd("OURPASSWORD"). The passwords are never shown\: each one reads
#cmd("SET") or #cmd("NONE"). For a user who is not an administrator,
#cmd("OURID") and #cmd("ADDRESS") read #cmd("-"), and the lines keep their
shape. This hides only an #cmd("OURID") that differs from the default, the
Odette ID of OFTPD itself, because that one is shown to every user in the
status of the monitor. The other fields stay visible to every user\; only #cmd("PARTNER SHOW")
hides these two. The #cmd("SHOW") of a transfer (@cmd-show) gives the
Odette IDs the transfer carries to every user who may see that transfer. The
sixth line starts with #cmd("source:") and tells where the partner comes from\: #cmd("OFTPPTN"), #cmd("kept by command"),
or #cmd("kept by command, overrides OFTPPTN"), followed by the number of
sessions the partner has now. The answer ends with #cmd("partner").

=== DELETE <cmd-partner-delete>

#idx("PARTNER", "DELETE")
#cmd("DELETE") applies to a partner that is kept by command. OFTPD refuses it
in these cases, checking them in this order.

- The partner is only in #cmd("OFTPPTN") (code 513). Remove its entry from the
  member, and use #cmd("REFRESH").
- The partner has a session, or OFTPD is calling it (code 450). Repeat the
  command later.
- Another partner is reached through it (#cmd("VIA")), enabled or not
  (code 512). Delete that partner, or change its #cmd("VIA"), first.
- A transfer in the spool names the partner (code 512): as its partner, as the
  partner behind which the file goes (#cmd("VIA")), or as the partner a relayed
  file came from. The answer gives the number of transfers. Delete them with
  #cmd("DELETE") or #cmd("PURGE") first. A transfer counts in any state,
  finished ones included.

If the partner is enabled, OFTPD disables it before it looks through the
spool, so that nothing new is queued for it or accepted from it meanwhile.
When a transfer in the spool names the partner, the partner stays disabled.
If it was enabled before, the answer says so with #cmd("(disabled)") after
the name of the partner. Enable it again if it should work.

OFTPD does not hold the partner table while it looks through the spool, so
that sessions go on. Afterwards it checks the partner again. If another command
replaced the partner, enabled it, gave it a session, or made it the
#cmd("VIA") of another partner meanwhile, nothing is deleted, and the answer
is #cmd("the partner was changed meanwhile, try again") (code 450). Repeat the
command. If the spool cannot be read to the end, the answer is
#cmd("spool not available") (code 451), and the partner stays disabled if
OFTPD disabled it.

If the partner was kept by command and has an entry of the same name in
#cmd("OFTPPTN") as well, deleting it brings that entry back at once, and the
answer ends with #cmd("the OFTPPTN entry is back"). OFTPD does this with a
full refresh, the same as #cmd("F OFTPD,REFRESH"), not by reading that one
entry\: it reads #cmd("OFTPPTN") and the receive rules again and writes
#cmd("OFTP016I") and #cmd("OFTP024I"). Every change in the member since the last
start or refresh takes effect for all partners, and the receive rules are
replaced. If OFTPD cannot read #cmd("OFTPPTN") then, the partner is deleted all
the same, no refresh happens, and the answer ends with
#cmd("OFTPPTN cannot be read, its entry is back after the next REFRESH").

=== Answers and Messages <cmd-partner-ans>

The answer of OFTPD is the last line of the command. The utility and the
dialog do not show the code at its start, so a refused command is found by its
text. @cmd-partner-tab lists the codes that the #cmd("PARTNER") command uses,
and the text that goes with each\; in these texts #cmd("name") stands for the
partner of the command, #cmd("other") for another partner, and #cmd("n") for a
number.

#tab(caption: [Answers of the PARTNER command])[
  #table(columns: (0.5in, 1.7fr, 1.3fr),
    [Code], [Text the utility prints], [Meaning],
    [200], [#cmd("partner name added") \
      #cmd("partner name changed") \
      #cmd("partner name unchanged") \
      #cmd("partner name deleted") (with the endings above) \
      #cmd("partner") at the end of a #cmd("SHOW")],
      [Done.],
    [210], [#cmd("partner name added, but it has no PASSWORD") \
      #cmd("partner name changed, but it has no PASSWORD")],
      [Done. The partner is enabled and has no #cmd("PASSWORD").],
    [450], [#cmd("the partner has a session now, try again") \
      #cmd("the partner was changed meanwhile, try again")],
      [The partner has a session, or OFTPD is calling it\; or, for a
      #cmd("DELETE"), the partner changed while OFTPD looked through the
      spool.],
    [451], [#cmd("spool not available")],
      [Nothing was changed, except that a partner that was being deleted may
      have been disabled.],
    [501], [#cmd("no such partner")], [The partner is not in the table.],
    [507], [#cmd("needs ADMIN= in OFTPPRM")],
      [The user is not an administrator.],
    [508], [The reason, for example #cmd("unknown keyword token") \
      #cmd("invalid value for token") \
      #cmd("word n is not KEYWORD=VALUE") \
      #cmd("ODETTEID is required") \
      #cmd("the entry is too long")],
      [The partner name or an operand is not valid, or the entry is too long.
      A word that is not #cmd("KEYWORD=VALUE") is often the second half of a
      value with a blank in it\; #var("n") is its place, not the word.],
    [509], [#cmd("partner exists: use CHANGE")],
      [The partner exists\; #cmd("ADD") is refused.],
    [512], [#cmd("partner other is reached through it") \
      #cmd("partner other is reached through it: disable that first") \
      #cmd("n transfers name partner name: PURGE or DELETE them first") \
      (with #cmd("(disabled)") after the name, if the partner was enabled
      before)],
      [Another partner is reached through this one, or transfers in the spool
      name it.],
    [513], [#cmd("the partner is in OFTPPTN: remove it there, then REFRESH")],
      [The partner is in #cmd("OFTPPTN") only, and cannot be deleted by
      command.],
    [514], [#cmd("the partner table is full")],
      [The table holds 32 partners, from both sources.],
  )
] <cmd-partner-tab>

The return code of the utility follows the code\: 0 for 200, 4 for 210, 450 and
451, 12 for 501, and 8 for every other code.

OFTPD writes a message to the console and the log for each change it makes\:
#cmd("OFTP243I") for an added partner, #cmd("OFTP244I") for a changed one
and #cmd("OFTP245I") for a deleted one. They name the partner and the user,
never a password. A command that is refused, or that changes nothing, writes
none.

=== The Passwords <cmd-partner-pw>

#idx("PARTNER", "passwords")#idx("password")
A password given with #cmd("ADD") or #cmd("CHANGE") needs care.

- It is stored in the spool in clear text, as it is in #cmd("OFTPPTN"). Protect
  the spool data set like the member of #cmd("OFTPPTN"), and so its backups.
- It is visible where it is typed: in the JCL or the #cmd("SYSIN") data that
  holds the command, and on the screen at a terminal. When the utility reads
  the commands from #cmd("SYSIN") it echoes each one with the prefix
  #cmd("OFTP"), and writes eight stars in place of the value of
  #cmd("PASSWORD") and #cmd("OURPASSWORD"), whatever its length. That masks the
  echo only. The mask hides one word\: the value ends at the first blank that
  is not inside quotes. A password written as one word (#cmd("PASSWORD(x)"),
  #cmd("PASSWORD=x")) is hidden, and so is one in quotes
  (#cmd("PASSWORD('x y')")), with or without blanks between the keyword and
  the value. Unquoted, #cmd("PASSWORD(ab cd)") is echoed as
  #cmd("PASSWORD(******** cd)")\: the rest of the password stands in
  #cmd("SYSPRINT"). The utility then refuses the command (#cmd("OFTP007E")),
  but the echo has been written by then. The echo looks for the text
  #cmd("PASSWORD") in any case and anywhere in the line, so a keyword that
  ends in it is masked as well. A long command is echoed in lines of at most
  72 columns, each one but the last ending in #cmd(" -").
- The request passes through a slot in common storage (CSA) on its way to
  OFTPD. OFTPD wipes the request from the slot as soon as it has taken it,
  and the slot is wiped again, request and answer, when it is released. Until
  then, a program that can read common storage could read the password.
- An operand that is not #cmd("KEYWORD=VALUE") is named in the answer by its
  place, as #cmd("word n is not KEYWORD=VALUE"), never by the word itself,
  because that can be the second half of a password with a blank in it. A
  value cannot hold a blank or a comma, not even in quotes, because OFTPD
  reads the request word by word\: #cmd("PASSWORD('a b')") reaches it as
  #cmd("PASSWORD=A B"), and the command is refused with code 508 and return
  code 8, changing nothing. #var("n") counts from the first word after
  #cmd("ADD") or #cmd("CHANGE"), in the order in which the utility sends the
  words, and the partner name counts as a word where you wrote it. In
  #cmd("PARTNER ADD ACME PASSWORD('a b')") the word #cmd("B") is word 3\; in
  #cmd("PARTNER ADD PASSWORD('a b') ACME") it is word 2.
- #cmd("SHOW") and the ISPF dialog show whether a password is set, never the
  password. The messages #cmd("OFTP243I") and #cmd("OFTP244I") do not carry it.

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
    [4], [Two meanings. The command was not carried out and may be tried
      again\: there is no
      such transfer\; the transfer is in a session\; the transfer changed
      meanwhile\; OFTPD is busy\; the spool
      or storage is not available. Or it was carried out with a warning
      (the code 210)\: the data set is empty, or the partner has no
      #cmd("PASSWORD"). The text of the last line of the answer says which.],
    [8], [An error in the command or in its data set: an operand is wrong or
      too long, the data set is not found, not allowed or cannot be opened,
      the state does not allow the change, it is not your transfer, the user
      is not an administrator, or the command is unknown.],
    [12], [Severe: the partner is unknown, not enabled or cannot be called,
      or OFTPD is not reached or does not answer completely.],
  )
] <cmd-rc-tab>

With the return code 12 the utility writes the cause with the message
#cmd("OFTP010E"). The messages #cmd("OFTP001E"), #cmd("OFTP002E"),
#cmd("OFTP003E") and #cmd("OFTP004E") are written by the utility itself. They are described in
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
