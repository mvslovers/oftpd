#import "../bookmaster/bookmaster.typ": *

= Operator Commands <ref-op>

#idx("operator commands")#idx("MODIFY")
The operator controls the OFTPD monitor from the console with the MODIFY
command, abbreviated #cmd("F"), and stops it with the STOP command:

#screen(```
F OFTPD,command
P OFTPD
```)

OFTPD reads the command in upper or lower case. Blanks or commas separate the
operands. If the text after the comma is not one of the commands below,
OFTPD answers with message #cmd("OFTP199E"). The commands of this chapter are
for the operator. Users and administrators who work from TSO or batch use
the #cmd("OFTP") utility instead.

#cmd("TRACE") takes two operands. The other commands use the first operand
and ignore any that follow. #cmd("F OFTPD,HOLD P1,P2") holds the sends of P1
only, and gives no message about P2.

In the command formats, square brackets enclose an operand that may be left
out, braces enclose a choice of which one must be taken, and a bar separates
the choices. Words in lower case stand for a value that you supply.

== DISPLAY <op-display>

#idx("DISPLAY")#idx("F OFTPD,D")
#screen(```
F OFTPD,{D|DISPLAY} [P|Q|M|S]
```)

The command writes information about the monitor to the console. Only the
first character of the operand counts.

#deflist(width: 1.1in,
  [#cmd("D P")], [Shows the partners. OFTPD writes one #cmd("OFTP100I") line
    for each partner of the table, whether it comes from #cmd("OFTPPTN") or is
    kept by command, then #cmd("OFTP101I") with the number of partners
    and sessions, then #cmd("OFTP127I") with the state of the trace.],
  [#cmd("D Q")], [Shows the spool. OFTPD writes one #cmd("OFTP110I") line for
    each transfer, then #cmd("OFTP111I") with the count.],
  [#cmd("D M")], [Shows the largest free block of storage, the number of
    subtask handles and the number of sessions, with message
    #cmd("OFTP102I").],
  [#cmd("D S"), #cmd("D") without an operand, or any other operand],
    [Shows the number of sessions with message #cmd("OFTP101I"), then the
    state of the trace with #cmd("OFTP127I").],
)

== CALL <op-call>

#idx("CALL")
#screen(```
F OFTPD,CALL partner
```)

The command makes OFTPD call the partner at once, without waiting for work
to send. OFTPD answers #cmd("OFTP121I"). The message says that the call is
requested, not that it succeeded\; the messages of the session follow. If the
partner is not enabled, or OFTPD cannot call it (see the keywords
#cmd("CALL"), #cmd("HOST") and #cmd("VIA")), the answer is #cmd("OFTP120E")
and no call is made.

== REFRESH <op-refresh>

#idx("REFRESH")
#screen(```
F OFTPD,REFRESH
```)

The command reads the partner table #cmd("OFTPPTN") again, and after it the
receive rules (chapter “Receive Rules”). It does not read
#cmd("OFTPPRM"). OFTPD answers #cmd("OFTP016I") when it has read the member,
or #cmd("OFTP017E") if it could not open it, in which case the table in use
is not changed. The command leaves alone the partners that an administrator
keeps by command with the #cmd("PARTNER") command of the #cmd("OFTP") utility.
What happens to new, changed and removed partners is described in the chapter
“Configuration and the Partner Table”.

== HOLD and RELEASE <op-hold>

#idx("HOLD")#idx("RELEASE")
#screen(```
F OFTPD,HOLD {partner|ALL}
F OFTPD,RELEASE {partner|ALL}
```)

#cmd("HOLD") stops the sends to one partner, or to all partners with
#cmd("ALL"). It changes the transfers that are queued or waiting for a retry
to the state held, so OFTPD does not call for them. A send that is in a
session is not stopped. #cmd("RELEASE") changes the held transfers back to
queued, sets their count of attempts to 0, and wakes the scheduler.

OFTPD answers with #cmd("OFTP233I"), which tells how many transfers it
changed. The count can be 0. Without an operand, OFTPD answers
#cmd("OFTP123E"). If the partner is unknown or not enabled, the answer is
#cmd("OFTP120E"). If the spool cannot be used, the answer is
#cmd("OFTP237E").

OFTPD matches the partner name against the partner a transfer is sent
through. For a transfer routed through another partner (see the keyword
#cmd("VIA")) that is the other partner, not the final one. Only sends are
changed. #cmd("HOLD") takes the sends in state queued or waiting, and
#cmd("RELEASE") the sends in state held. Sends in any other state, and all
receives, stay as they are.

== TRACE <op-trace>

#idx("TRACE")
#screen(```
F OFTPD,TRACE {ON|OFF} [partner]
```)

#cmd("TRACE ON") makes OFTPD write the protocol commands of the sessions to
the log as #cmd("OFTP290P") lines, for all partners or for the named one.
The trace also applies to sessions that are already running.
#cmd("TRACE OFF") ends it. Without a partner it ends all tracing. OFTPD
answers #cmd("OFTP124I").

A partner cannot be taken out of a trace that is on for all partners\;
#cmd("TRACE OFF partner") is then refused with #cmd("OFTP125E"). A first
operand other than #cmd("ON") or #cmd("OFF") is refused with
#cmd("OFTP126E"). A partner that is not in the table is refused with
#cmd("OFTP120E")\; a partner that is disabled is accepted. The state of the
trace is shown by #cmd("D P") and #cmd("D S") in message #cmd("OFTP127I").
#cmd("OFTP127I") lists partner names up to a total of 99 characters. A name
that does not fit is left out. #cmd("D P") marks every traced partner with
#cmd("TRACE") in its #cmd("OFTP100I") line.

After #cmd("TRACE OFF") OFTPD writes no more lines for the commands of a
session, a session that is running at that moment included. When such a
session ends, it can still write one #cmd("TRACE SEND DATA") or
#cmd("TRACE RECV DATA") line for the data buffers it counted before the
trace was switched off.

The trace is a diagnostic\; switch it off when you have what you need. Every
protocol command is one line in the log. A partner that connects often fills
the log quickly, and there is no limit to the size of what the trace writes. If the
log data set is full, OFTPD gives the log up and writes every further message
to the console (#cmd("OFTP023E")), the trace lines included, so a forgotten
trace can flood the console.

With #cmd("TRACE ON") for all partners, the first two lines of a session that
the partner started, #cmd("SEND SSRM") and #cmd("RECV SSID"), show the
caller's address in place of the partner name, because OFTPD does not know the
partner yet. If the caller is refused or unknown, every line of the session
shows the address. A trace for one partner starts with the #cmd("RECV SSID")
line. In a session that OFTPD started with a call, the partner is known from
the first line, which is #cmd("RECV SSRM").

== PURGE <op-purge>

#idx("PURGE")#idx("housekeeping")
#screen(```
F OFTPD,PURGE [days]
```)

The command deletes the finished transfers from the spool whose last change
is older than the given number of days, from 0 to 9999. A finished transfer is
one in state delivered, confirmed, rejected or failed\; the others are not
deleted. Without an operand it uses #cmd("KEEPDAYS") of #cmd("OFTPPRM"). If #cmd("KEEPDAYS") is 0 and no
operand is given, or the operand is not a number from 0 to 9999, the answer is
#cmd("OFTP122E") and nothing is deleted. A second operand, as in
#cmd("PURGE 30,x"), is not refused\; OFTPD ignores the second operand. Otherwise OFTPD answers
#cmd("OFTP235I") with the number of transfers deleted, or #cmd("OFTP236E") if
the spool failed. Only the spool records are deleted\; no data set is
deleted.

== Stopping OFTPD <op-stop>

#idx("STOP")
#cmd("P OFTPD") stops the monitor. OFTPD does not take a stop as a modify
command. The messages written while it stops are described in the appendix
“Messages”, starting with #cmd("OFTP002I").
