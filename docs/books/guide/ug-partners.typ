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
default #cmd("SYS2.PARMLIB"), with the members #cmd("OFTPPRM0"),
#cmd("OFTPPTN0") and #cmd("OFTPRCV0"). The procedure picks other members through its parameters.
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
instead, and a later refresh takes it out. OFTPD answers with message
#cmd("OFTP016I"), which gives the number of partners enabled, new, disabled
and removed. The command reads the receive rules again too (see
@xfer-rules).

The table has 32 places. If it is full, OFTPD writes #cmd("OFTP015W") and does
not add the partner\; remove a partner from the member and refresh again. A
partner that stays in the table because a transfer names it needs that
transfer deleted first. Keep the member to at most 32 partners in any
case\; a further #cmd("PARTNER=") is refused, but the keywords after it still go
to the last partner.

A change to #cmd("OFTPPRM") needs a restart.

== Checking a Partner <ptn-check>

#idx("OFTP010W")
After the start or a refresh, check the following.

+ *Errors in the members.* A line OFTPD does not understand is reported with
  message #cmd("OFTP010W"), which names the member and the line. An unknown
  keyword or a value that is not valid disables the partner it belongs to. The
  other partners work.
+ *The partners.* Enter #cmd("F OFTPD,D P"). OFTPD writes a line
  (#cmd("OFTP100I")) for every partner, with its Odette ID, whether it is
  enabled, the protocol level and the host and port, then the number of
  partners. The message #cmd("OFTP014P") in the log, written at the start, says
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
when you have what you need, with #cmd("F OFTPD,TRACE OFF"). After it no
trace line follows.

The trace is a diagnostic, not a standing setting. Every command is a line in
the log, and a session that moves three small files writes about 28 of them.
The trace has no size limit, so a busy partner fills the log quickly. When the
log is full, OFTPD writes every further message to the console, trace lines
included.

With the trace on for all partners, the first two lines of a session that a
partner started carry the caller's address instead of the partner name,
because OFTPD does not know the partner yet. For a caller that is refused or
unknown, every line carries the address.

A character of an Odette ID or a file name from the partner that cannot be
printed shows as a dot in the trace and in the session lines.
