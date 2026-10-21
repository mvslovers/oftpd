#import "../bookmaster/bookmaster.typ": *
#import "msg-lib.typ": msg

= Messages <apx-messages>

#idx("messages")
OFTPD messages have the form #cmd("OFTPnnnC"): the prefix #cmd("OFTP"), a
three-digit number #var("nnn"), and a class letter #var("C"). The class
tells how serious the message is and where it goes.

#tab(caption: [Message classes])[
  #table(columns: (0.7in, 1.6in, 1fr),
    [Class], [Meaning], [Destination],
    [#cmd("I")], [Information. Nothing is wrong.], [Console and log.],
    [#cmd("W")], [Warning. Something was refused or went wrong, and OFTPD goes
      on.], [Console and log.],
    [#cmd("E")], [Error. A function or a request failed.], [Console and log.],
    [#cmd("P")], [Protocol of the normal work: sessions, transfers,
      requests.], [Log only.],
  )
]

Each log line has the date and time in front\; the console line has not. A
message is at most 123 characters\; a longer one is cut and ends in a
greater-than sign. The destination of each message is stated in its entry. A few messages are
written to the console only, because they come before the log is open or
report a failure of the log. Whenever the log is not open or has failed, the
messages that would go to the log go to the console.

The messages #cmd("OFTP001E"), #cmd("OFTP002E"), #cmd("OFTP003E"),
#cmd("OFTP004E"), #cmd("OFTP007E") and #cmd("OFTP010E") come from the
#cmd("OFTP") utility and are written to its output, not to the console.
Except for #cmd("OFTP010E"), they are found before OFTPD is asked, and the
return code is 8.

An entry shows the message as OFTPD writes it. Words in lower case, such as
#var("name") or #var("n"), stand for text that OFTPD fills in. The
explanation says what the message means and when it appears. The action says
what you should do about it. Where the action is none, the message is for
information only.

Messages are listed in the order of their numbers. The numbers fall into
three ranges:

#deflist(width: 1.6in,
  [#cmd("OFTP000"), #cmd("OFTP099")], [Starting and stopping, the settings and
    the partner table, the log, the cross-memory service and the utility
    messages.],
  [#cmd("OFTP100"), #cmd("OFTP199")], [The operator commands.],
  [#cmd("OFTP200"), #cmd("OFTP299")], [Sessions, calls, transfers, and the
    requests that the utilities make.],
)

== Messages OFTP000 to OFTP099 <msg-0xx>

#include "msg-0xx.typ"

== Messages OFTP100 to OFTP199 <msg-1xx>

#include "msg-1xx.typ"

== Messages OFTP200 to OFTP299 <msg-2xx>

#include "msg-2xx.typ"

== Message of the dialog <msg-dlg>

The ISPF dialog (#cmd("OFTPI")) has one message of its own. Like the
messages of the utility, it does not go to the console or the log.

#msg("OFTPI001E", "OFTPI RUNS UNDER ISPF: SELECT CMD(OFTPI)",
  [The dialog could not set up its ISPF services. The usual cause is that it was started outside ISPF, for example from a plain TSO READY prompt or from a batch job. The dialog ends with return code 8.],
  [Start ISPF and start the dialog from there, with #cmd("SELECT CMD(OFTPI) NEWAPPL(OFTP)") or from an option of a menu panel, as the installation guide describes.],
  dest: "terminal")
