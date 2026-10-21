#import "../bookmaster/bookmaster.typ": *

= Partners and Configuration <ug-partners>

#idx("partner")#idx("configuration")
OFTPD sends files to partners and receives files from them. A partner is a
system that speaks OFTP, and OFTPD only talks to partners it knows. This
chapter shows how to describe your own system and its partners, with examples
for the cases that come up most, and how to check that the partner is set up
right. Every keyword is described in full in the chapter “Configuration and
the Partner Table” of the _OFTPD Reference_.

== What You Agree With a Partner <ptn-agree>

Two systems can exchange files only if each is set up to accept the other. Agree
the following with the owner of the partner system before you write the
entries. @ptn-agree-tab lists the items and where each one goes.

#tab(caption: [What to agree with a partner])[
  #table(columns: (1.4in, 1fr),
    [Item], [Where it goes in OFTPD],
    [The Odette ID of each system], [Yours is #cmd("ODETTEID") in
      #cmd("OFTPPRM"). The partner's is #cmd("ODETTEID") in its entry of
      #cmd("OFTPPTN"). If the partner knows you by another Odette ID than the
      one in #cmd("OFTPPRM"), put it in #cmd("OURID") of the entry.],
    [The password the partner presents to you], [#cmd("PASSWORD") in the
      entry.],
    [The password you present to the partner], [#cmd("OURPASSWORD") in the
      entry.],
    [Who calls whom], [#cmd("HOST") and #cmd("PORT") in the entry, if you
      call the partner. #cmd("PORT") in #cmd("OFTPPRM") is where the partner
      calls you.],
    [The protocol level], [#cmd("LEVEL") in the entry. Use the default 5
      unless the partner speaks an older level only.],
    [The number of sessions at a time], [#cmd("SESSIONS") in the entry.],
    [The names of the files], [The virtual file name, chosen by the sender.],
  )
] <ptn-agree-tab>

The two sides are the mirror of each other. What one side calls its password
is the other side's #cmd("OURPASSWORD"), and the other way round.

OFTPD reads the values of the members as upper case, so a password is in upper
case. Make sure the partner accepts it in upper case.

Agree on the number of sessions too. A call that would go over the limit of the
other side is refused with the reason 08, and made again after a short pause.

== The Settings of Your System <ptn-prm>

The member #cmd("OFTPPRM") describes your own system. A first version needs
very little. The sample that comes with OFTPD, #cmd("OFTPPRM0"), is a good start.

#screen(```
* OFTPPRM0 - settings of the monitor
ODETTEID=O0000000000MYMVS
PORT=3305 SVC=237
ADMIN=IBMUSER
INPREFIX=OFTP.IN TMPPREFIX=OFTP.TMP
```)

- #cmd("ODETTEID") is the only keyword that must be there. The monitor does not
  start without it.
- #cmd("PORT") is the port on which partners call you. 3305 is the usual port of
  OFTP.
- #cmd("SVC") is needed for the #cmd("OFTP") utility and the dialog. Without it
  they cannot reach the monitor.
- #cmd("ADMIN") names a user who may see and change every transfer and may call
  a partner. Repeat the keyword for each administrator. Received files belong
  to nobody, so only an administrator sees them.
- #cmd("INPREFIX") and #cmd("TMPPREFIX") are the beginnings of the names of the
  data sets in which files are received.

The Reference describes the other keywords: the limits on sessions, buffers and
retries, the days that a finished transfer stays in the spool, and the
allocation of received data sets. Change one when you have a reason to. The
monitor reads #cmd("OFTPPRM") only when it starts.

== A Partner You Call and That Calls You <ptn-both>

This is the most common case, and the sample #cmd("OFTPPTN0") shows it. An entry
is a list of keywords. It starts with #cmd("PARTNER="), the name by which you and
your operators know the partner. The name has up to 8 characters.

#screen(```
PARTNER=MAC1 ODETTEID=O0000000000MAC1
  PASSWORD=SECRET1 OURPASSWORD=SECRET2
  HOST=192.0.2.10 PORT=3305 LEVEL=5
```)

The entry says: the partner is called MAC1 and is known to the protocol as
#cmd("O0000000000MAC1"). When it calls, it must give the password
#cmd("SECRET1"). When you call it, at the host and the port given, you give the
password #cmd("SECRET2"). The #cmd("HOST") may be a dotted address or a name\;
OFTPD looks a name up.

The entry can run over several lines, as long as the lines are within columns 1
to 72. A line with an asterisk in column 1 is a comment.

The partner enters the same two passwords the other way round, and your Odette
ID, which is the #cmd("ODETTEID") of your #cmd("OFTPPRM"). If it runs OFTPD, its
entry for you has #cmd("SECRET2") as #cmd("PASSWORD") and #cmd("SECRET1") as
#cmd("OURPASSWORD").

Because the entry has a #cmd("HOST"), OFTPD calls the partner when a file waits
for it. Because the partner may also call, any call that comes with the right
Odette ID and password is accepted.

== A Partner That Only Calls You <ptn-calls>

Some partners are behind a firewall, or have a changing address, so that you
cannot call them. Leave out the #cmd("HOST"). OFTPD then never calls the partner,
and answers its calls. Files that you queue for the partner wait in the spool,
and go out in the session when the partner calls.

#screen(```
PARTNER=HOME1 ODETTEID=O0000000000HOME1
  PASSWORD=SECRET3 OURPASSWORD=SECRET4
  ADDRESS=198.51.100.7
```)

#cmd("ADDRESS") makes OFTPD accept the partner only from that address. A caller
that gives this Odette ID from another address is refused (message
#cmd("OFTP201W")). Use it when the partner has a fixed address.

If the partner has a host but you never want to call it, say #cmd("CALL=N")
instead.

== A Partner You Only Send To, or Only Receive From <ptn-mode>

The keyword #cmd("MODE") limits what a partner may do, seen from your side.
#cmd("MODE=S") only sends to the partner, and refuses the files it offers.
#cmd("MODE=R") only receives. The default #cmd("MODE=B") does both.

#screen(```
PARTNER=ARCHIVE ODETTEID=O0000000000ARCH
  PASSWORD=SECRET5 OURPASSWORD=SECRET6
  HOST=archive.example.org MODE=S
```)

== A Partner Behind a Clearing Centre <ptn-via>

#idx("VIA")#idx("RELAY")
Some partners are reached through a third system, a clearing centre, that
passes the files on. Describe the clearing centre as a partner with a host and
passwords. Describe the final partner with #cmd("VIA="), naming the clearing
centre. The final partner needs no host and no passwords of its own.

#screen(```
PARTNER=CENTRE ODETTEID=O0000000000CENT
  PASSWORD=SECRET7 OURPASSWORD=SECRET8
  HOST=centre.example.org
PARTNER=FARAWAY ODETTEID=O0000000000FARA VIA=CENTRE
```)

A file that you send to FARAWAY goes in a session with CENTRE, addressed to the
Odette ID of FARAWAY. The clearing centre has to know that Odette ID. Only one
level is allowed: the partner named in #cmd("VIA") must itself be a direct
partner.

If your own system is to be the clearing centre for other systems, set
#cmd("RELAY=Y") in #cmd("OFTPPRM"). OFTPD then accepts the files that are
addressed to the Odette IDs of its partners and passes them on unchanged. Without
#cmd("RELAY=Y") it refuses a file for any Odette ID but its own.

== Putting the Partners in Use <ptn-use>

Put the finished members in the library that the started task reads, by
default #cmd("SYS2.PARMLIB"), with the members #cmd("OFTPPRM0") and
#cmd("OFTPPTN0"). The procedure picks other members through its parameters.
Then start the monitor (see @ug-monitor).

A change to the partner table does not need a restart. Edit #cmd("OFTPPTN"),
and enter

#screen(```
F OFTPD,REFRESH
```)

at the console, or, as an administrator, choose the option to configure in the ISPF dialog,
which reads the member again when the edit ends. OFTPD adds new partners and
replaces every partner that is still in the member by its entry. A partner that
you took out of the member leaves the table and frees its place\; if a transfer
in the spool still names it or a session of it is running, OFTPD disables it
instead, and a later refresh takes it out. A refresh does not touch a partner
that is kept by command (see @ptn-maint): its entry stays, whatever the member
says. OFTPD answers with message #cmd("OFTP016I"), which gives the number of
partners enabled in the whole table, and the number of new, disabled and
removed partners of the member.

The table has 32 places. If it is full, OFTPD writes #cmd("OFTP015W") and does
not add the partner\; remove a partner from the member and refresh again. A
partner that stays in the table because a transfer names it needs that
transfer deleted first. Keep the member to at most 32 partners in any
case\; a further #cmd("PARTNER=") is refused, but the keywords after it still go
to the last partner.

A change to #cmd("OFTPPRM") needs a restart.

== Maintaining Partners While OFTPD Runs <ptn-maint>

#idx("partner kept by command")#idx("PARTNER")
You can also add, change, disable and delete a partner while OFTPD runs, without
an edit of #cmd("OFTPPTN") and without a refresh. An administrator does this
with the #cmd("OFTP PARTNER") command or in the ISPF dialog. OFTPD keeps a
partner that you add or change this way in its spool, so the partner is there
after a restart too. This guide calls it a partner kept by command.

=== The Member or the Command <ptn-maint-which>

The partner table has two layers. The member #cmd("OFTPPTN") is the first. The
partners kept by command are the second. Both share the 32 places of the table.
If a name is in both, the entry kept by command is the partner, and OFTPD
ignores the entry of the member. The _OFTPD Reference_ describes the layers in
the chapter “Configuration and the Partner Table”.

Use the member for the partners that you set up once and keep with the rest of
your configuration. Use the command for a change that cannot wait for an edit,
such as a new password that the partner has just agreed, or a partner to switch
off for a day. A refresh does not touch a partner kept by command, and an edit
of the member does not change one. To change such a partner, use the command
again.

A partner that is only in the member changes by command too. The first change
copies the whole entry to the spool, and from then on the partner is kept by
command. The member stays as it was.

=== Adding a Partner <ptn-maint-add>

Give the name of the partner and its Odette ID. Add the other keywords of the
entry as you need them. They are the keywords of the member, with the same
values. This job adds a partner that calls you and that you call.

#screen(```
//PARTNER  EXEC PGM=OFTP
//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
PARTNER ADD PARTNER2 ODETTEID(O0000000000PARTNER2) -
     PASSWORD(SECRET7) OURPASSWORD(SECRET8) HOST(192.0.2.20)
/*
```)

OFTPD answers #cmd("partner PARTNER2 added") and writes message
#cmd("OFTP243I") to the console and the log. The partner is in use at once. A
keyword that you leave out takes its default, as in the member. The partner is
enabled, unless you say #cmd("ENABLED(N)").

If you give a name or an Odette ID that another partner has, or the table is
full, OFTPD refuses the command and changes nothing. A request is at most 255
characters long. If the keywords do not fit, add the partner with the ones it
needs and give the others in a #cmd("PARTNER CHANGE").

#note[A partner without a #cmd("PASSWORD") accepts every caller that knows its
Odette ID. If you add or change an enabled partner without one, OFTPD does the
work, and tells you in two lines: #cmd("WARNING: without a PASSWORD every peer
that knows the Odette ID of the partner can log on"), and
#cmd("partner PARTNER2 added, but it has no PASSWORD"). The utility ends with the
return code 4. These lines have no message id.]

=== Changing a Partner <ptn-maint-change>

Name the partner and the keywords to change. The others stay as they are.

#screen(```
OFTP PARTNER CHANGE PARTNER2 PASSWORD(NEWPASS1)
OFTP PARTNER CHANGE PARTNER2 HOST()
```)

An empty value, as in #cmd("HOST()"), removes the value of #cmd("PASSWORD"),
#cmd("OURPASSWORD"), #cmd("HOST"), #cmd("ADDRESS") or #cmd("VIA"). For
#cmd("OURID") it brings back the default. If nothing changes, OFTPD says
#cmd("partner PARTNER2 unchanged"). Otherwise it answers
#cmd("partner PARTNER2 changed") and writes #cmd("OFTP244I").

While a session with the partner runs, or OFTPD is calling it, the command can
change only #cmd("ENABLED"). Any other change is refused with the answer
#cmd("the partner has a session now, try again")\; give the command again when
the session has ended. OFTPD does not wait for you and does not change half of
the entry.

=== Disabling and Enabling a Partner <ptn-maint-ena>

#screen(```
OFTP PARTNER DISABLE PARTNER2
OFTP PARTNER ENABLE PARTNER2
```)

A disabled partner stays in the table with all its values. OFTPD refuses its
calls, does not call it, and takes no new send for it. Both commands work while
a session runs. A partner that other partners are reached through (see
@ptn-via) cannot be disabled until you have disabled those partners.

=== Deleting a Partner <ptn-maint-delete>

#screen(```
OFTP PARTNER DELETE PARTNER2
```)

The command removes the partner from the spool and the table, and frees its
place. The definition, with the passwords, is gone. OFTPD writes
#cmd("OFTP245I"). OFTPD refuses the command in these cases.

- A session with the partner runs, or OFTPD is calling it. Try again later.
- Another partner is reached through it. Delete or change that partner first.
- A transfer in the spool names the partner, in any state, the finished ones
  too. The answer says how many. OFTPD has disabled the partner by then, so that
  nothing new arrives. Delete the transfers with #cmd("OFTP DELETE") or
  #cmd("OFTP PURGE"), and give the command again.
- The partner is in the member only. Remove it from #cmd("OFTPPTN"), and
  refresh.

If the partner is in the member as well, the entry of the member is the partner
again after the delete, and the answer says so.

=== What to Protect <ptn-maint-safe>

OFTPD keeps the passwords of a partner kept by command in the spool, as clear
text, just as the member holds them. Protect the spool data set as you protect
the member, and guard every copy of it that you make.

#cmd("OFTP PARTNER SHOW name") never shows a password. It says #cmd("SET") or
#cmd("NONE"). When the utility reads its commands from #cmd("SYSIN"), it echoes
them to #cmd("SYSPRINT") with the value of #cmd("PASSWORD") and
#cmd("OURPASSWORD") replaced by eight asterisks. The job itself still holds the
password, in the job that you submitted. A
command in a PARM string or typed at a terminal is not echoed, but it is no safer
where it is kept. Keep a job that holds a password where only you can read it,
and do not leave it in a library that others read.

The request waits in a slot of common storage (CSA) until OFTPD takes it, and a
program in key 8 can read that storage. The wait is short. OFTPD clears the
request in the slot as soon as it has copied it, and the SVC routine clears the
slot when the caller takes its answer. If the caller is gone before OFTPD takes
the request, OFTPD clears the request and does not carry it out. When OFTPD
stops, it clears all slots.

=== The ISPF Dialog <ptn-maint-ispf>

Option 3, Partners, of the ISPF dialog does the same. Type #cmd("M") to change a
partner, #cmd("E") to enable, #cmd("X") to disable and #cmd("D") to delete, in
front of the partner. Type a new name in the field #cmd("New partner") to add a
partner. The panel of the entry shows the passwords only as #cmd("(set)") or
#cmd("(none)"), and does not show what you type for a password. When you change a
partner, a field that you leave blank keeps its value, and a blank password is
never sent. Only #cmd("Host"), #cmd("Address") and #cmd("Via") become empty when
you blank them. When you add a partner, a blank field takes its default. The
dialog sends the same requests as the utility, so the same rules and refusals
apply, and only an administrator may use these functions.

Option 4, Configure, edits the member. When the edit ends, the dialog asks OFTPD
to refresh the table. That changes the partners of the member only.

== Checking a Partner <ptn-check>

#idx("OFTP010W")
After the start or a refresh, check the following.

+ *Errors in the members.* A line OFTPD does not understand is reported with
  message #cmd("OFTP010W"), which names the member and the line. An unknown
  keyword or a value that is not valid disables the partner it belongs to. The
  other partners work.
+ *The partners.* Enter #cmd("F OFTPD,D P"). OFTPD writes a line
  (#cmd("OFTP100I")) for every partner, with its Odette ID, whether it is
  enabled, the protocol level and the host and port (for a partner that is
  reached through another, the word #cmd("VIA") and the name of the other),
  then the number of partners. The message #cmd("OFTP014P") in the log, written at the start, says
  how many of the partners in the member are enabled. The #cmd("OFTP") command
  #cmd("PARTNERS") shows the same table to a user.
+ *A call.* Enter #cmd("F OFTPD,CALL name"), or, as an administrator, the
  utility command #cmd("OFTP CALL name"). OFTPD calls the partner at once,
  although no file waits. Look at the log for what follows.

A call that works leaves lines like these in the log.

#screen(```
OFTP206P MAC1: CALLING AT 192.0.2.10 PORT 3305
OFTP290P MAC1: session with MAC1: level 5, buffer 4096, credit 99, mode B
OFTP203P MAC1: SESSION ENDED, ESID 00 (PARTNER), FILES 0 OUT 0 IN
```)

The level, the buffer and the credit are what the two sides agreed on. The
ESID 00 is a normal end\; the word after it, #cmd("OURS") or #cmd("PARTNER"),
says which side ended the session. @ptn-trouble-tab lists what you see when a call does not
work, and what it means.

#tab(caption: [What a failed call tells you])[
  #table(columns: (1.9in, 1fr),
    [In the log], [Cause],
    [#cmd("OFTP207P name: NOT REACHED")], [There was no connection within about
      5 seconds. Check #cmd("HOST") and #cmd("PORT"), that the partner is
      running, and that nothing between blocks the port.],
    [#cmd("OFTP290W unknown caller id")], [A caller gave an Odette ID that no
      enabled partner has, or called from another address than the
      #cmd("ADDRESS") of its entry. Compare the Odette ID with the partner.],
    [#cmd("OFTP290W wrong password from name")], [A caller's password is not
      the #cmd("PASSWORD") of its entry. Remember that passwords are read as
      upper case.],
    [#cmd("OFTP290W answered by id, not id"), or #cmd("OFTP290W partner
      password does not match")], [You called, and the answer of the partner
      carries another Odette ID or password than your entry has. Compare
      #cmd("ODETTEID") and #cmd("PASSWORD") with the partner.],
    [#cmd("OFTP203P") with an ESID other than 00], [The session ended with the
      reason code, and the word after it says which side ended it. 03 is a
      user code not known and 04 an invalid password\; if the partner sent
      them (#cmd("PARTNER")), your Odette ID or #cmd("OURPASSWORD") is not
      what the partner expects. 07 is a buffer size mismatch, 08 resources not
      available, 10 mode or capabilities not compatible. The chapter
      “Messages” of the Reference lists them.],
  )
] <ptn-trouble-tab>

For a closer look, switch the protocol trace on for that partner with
#cmd("F OFTPD,TRACE ON name"). Every command of the sessions of the partner is
then written to the log, as #cmd("OFTP290P") lines. The lines carry the fields
of the commands, never a password and never file data. Switch the trace off
when you have what you need, with #cmd("F OFTPD,TRACE OFF"). After it OFTPD
writes no more lines for the commands of a session.

The trace is a diagnostic, not a standing setting. Every command is a line in
the log. The trace has no size limit, so a busy partner fills the log quickly. When the
log is full, OFTPD writes every further message to the console, trace lines
included.

With the trace on for all partners, the first two lines of a session that a
partner started carry the caller's address instead of the partner name,
because OFTPD does not know the partner yet. For a caller that is refused or
unknown, every line carries the address.
