#import "../bookmaster/bookmaster.typ": *

= The ISPF Dialog <ug-dialog>

#idx("ISPF dialog")#idx("OFTPI")
The dialog does what the utility does, with panels: it sends a data set, lists
the transfers and changes them, maintains the partner table, and shows the
monitor. It reaches the monitor the way the utility does, through the SVC, and
with the same rules, so a user sees and changes what the user may in the
utility. This chapter sets the dialog up on TK5 and goes through its panels
with the screens of a real session.

== What the Dialog Needs <dlg-needs>

- ISPF (the one of TK5 and MVS/CE) and the monitor, running, with #cmd("SVC=")
  in its settings (@ug-monitor).
- A screen of more than 24 rows. The main menu and the send panel do not fit on
  24. The panels are made for 43 rows (3278 or 3279 model 4) and use the width
  of a wider screen. On 24 rows the ISPF of TK5 does not say what is wrong\; it
  shows #cmd("ISPF SERVICE ERROR DISPLAY") with #cmd("SERVICE NAME DISPLAY") and
  #cmd("RETURN CODE 20"). Ask your 3270 emulator for model 4.
- The two libraries of OFTPD in the allocations of ISPF, as the next section
  shows.

The screens in this chapter have the width of 80 columns, and show 43 rows
shortened to the lines that hold text.

== Setting Up the Dialog <dlg-setup>

The dialog #cmd("OFTPI") needs its panels in the #cmd("ISPPLIB") allocation and
its module in the #cmd("ISPLLIB") allocation, because it loads #cmd("ISPLINK")
from there. This ISPF has no #cmd("LIBDEF"), so add the libraries where your
system allocates them, usually in the CLIST that starts ISPF.

- #cmd("OFTPD.ISPPLIB") to the #cmd("ISPPLIB") allocation,
- #cmd("OFTPD.LINKLIB") to the #cmd("ISPLLIB") allocation.

Add both at the #emph[end] of the concatenation. #cmd("OFTPD.ISPPLIB") is received
with the block size 3120. On TK5, with it in front of #cmd("HERC01.ISP.PLIB"),
which has the block size 27920, ISPF ended with #cmd("FUNCTION HAS ABENDED") on
the next panel it read. The cause is not known\; with the library at the end the
dialog ran. #cmd("LISTDS 'name'") shows the block sizes of your libraries.
#cmd("OFTPD.LINKLIB") in #cmd("ISPLLIB") was only ever tried at the end.

=== On TK5

TK5 builds the allocations at logon, in #cmd("SYS1.CMDPROC(ISPLOGON)"), with the
REXX exec #cmd("$CONCAT") (ddname, data set, and #cmd("BOT") to add at the end).
The same procedure then runs #cmd("userid.CMDPROC(MYLOGON)") if there is one.
Create it for your user ID.

#screen(raw(read("../ex/ug-ispf/mylogon.txt")))

and log on again. The menu entry goes into the “Additional applications” panel,
option #cmd("M") of the primary menu. That panel is the member #cmd("TSOAPPLS")
of #cmd("SYS2.ISP.PLIB"). Do not change that library\; copy the member to a
panel library of your own that comes before #cmd("SYS2.ISP.PLIB") in
#cmd("ISPPLIB") (check with #cmd("LISTA STATUS")), and add two lines there. The
first goes into the body, below the last entry. Copy the line of another entry
and change the option and the name.

#screen(```
%   O @OFTPD       + Odette file transfer
```)

The second goes into the #cmd("TRANS") list, beside the others.

#screen(```
                O,'CMD(OFTPI) NEWAPPL(OFTP)'
```)

Then the panel shows the entry.

#screen(raw(read("../ex/ug-ispf/more.txt")))

Without the entry, enter #cmd("ISPEXEC SELECT CMD(OFTPI) NEWAPPL(OFTP)") in
option #cmd("6") (COMMAND) of the primary menu. On another system, put a line for
the option into the body of a menu panel of your own, and
#cmd("O,'CMD(OFTPI) NEWAPPL(OFTP)'") into its #cmd("TRANS") list, where you
replace #cmd("O") by the option you choose. Started outside ISPF, the dialog says
#cmd("OFTPI001E OFTPI RUNS UNDER ISPF: SELECT CMD(OFTPI)").

== The Main Panel <dlg-menu>

Option #cmd("O") of the additional applications opens the main panel.

#screen(raw(read("../ex/ug-ispf/menu.txt")))

It has these options.

#deflist(width: 1.5in,
  [1 Send], [Queues a data set for a partner (@dlg-send).],
  [2 Transfers], [Lists the transfers, which you can limit to a partner, a
    state, or your own. Type a letter in front of a line: S to show it, H to
    hold, R to release, T to retry, D to delete (@dlg-transfers).],
  [3 Partners], [Shows the partner table as OFTPD has it. An administrator can
    also maintain the table here (@dlg-partners).],
  [4 Configure], [Lets an administrator edit the member of the partner table.
    When the edit ends, OFTPD reads the member again. The partners kept by
    command are not read again (@dlg-config).],
  [5 Monitor], [Shows the Odette ID, the port, the SVC, the sessions and the
    spool of the monitor.],
  [X Exit], [Leaves the dialog.],
)

Option 5 is the quickest check that the dialog reaches the monitor.

#screen(raw(read("../ex/ug-ispf/monitor.txt")))

The panel shows the settings of the monitor, the number of sessions and
partners, a count of the spool by state, and the version. If the dialog cannot
reach the monitor, it shows the same answer as the utility, #cmd("OFTPD not
reached: not running, or without SVC=") (@ug-monitor).

== Sending a Data Set <dlg-send>

Option 1 asks for the partner, the data set and how to send it. A question mark
in the partner field shows the partners\; type S in front of the one to pick. The
data set is fully qualified and has no quotes\; a member is in parentheses. The
virtual file name is the name the partner sees, at most 26 characters, and
blank takes the name of the data set or the member. The panel below queues the
data set held. The message at the foot is the same sentence that the utility
prints.

#screen(raw(read("../ex/ug-ispf/send.txt")))

*Text* is #cmd("FORMAT(T)") of the utility, *Translate* is #cmd("XLATE"), and
*Hold* queues the file held, to be released from the list of the transfers. *User
data* is the user field of the file description, at most 8 characters.

== The Transfers <dlg-transfers>

Option 2 lists the transfers. The fields at the top limit the list to a
partner, a state or your own transfers. The example is the list of the
administrator on the sending system, after the sessions of the previous
chapters.

#screen(raw(read("../ex/ug-ispf/transfers.txt")))

*D* in the column of a line is the direction: S for a send, R for a received
file. Type a letter in front of a line and press Enter. #cmd("S") shows the
transfer in full. The panel shows what #cmd("OFTP SHOW") shows.

#screen(raw(read("../ex/ug-ispf/show.txt")))

With the state limited to #cmd("HELD"), the list holds the transfer that the send
panel queued above. #cmd("D") deletes it, and the list says so in the line
under the heading. The data set is not touched.

#screen(raw(read("../ex/ug-ispf/held.txt")))

#screen(raw(read("../ex/ug-ispf/deleted.txt")))

Not every letter is allowed in every state. A hold is refused for a transfer that
is not queued or waiting, with the message #cmd("only a queued or waiting send can be held").

== Configure <dlg-config>

Option 4 is for an administrator. It opens an ISPF edit of the partner member,
named on its panel, and when the edit ends it asks the monitor to read the member
and the receive rules again, which is #cmd("F OFTPD,REFRESH"). The first time it
offers #cmd("SYS2.PARMLIB(OFTPPTN0)") and keeps the name you give in your ISPF
profile\; on TK5, type the member you use, for example
#cmd("SYS1.PARMLIB(OFTPPTN0)"). The editor is the one of TK5. Anyone who is not
an administrator gets #cmd("needs ADMIN=") after the edit. On a system without a
security product nobody is one (see @ops-admin).

The monitor reads the member of the DD #cmd("OFTPPTN") of the started task,
whichever member you edited.

== Maintaining the Partners in the Dialog <dlg-partners>

#idx("ISPF dialog", "partners")
Option 3 shows the partner table as OFTPD has it: the name, the Odette ID, the
state, whether a session is busy, the level and where the partner is reached.

#screen(raw(read("../ex/ug-ispf/partners.txt")))

It does with panels what the command #cmd("PARTNER") does (see
@ptn-cmd). Every change goes to OFTPD as a request, and the answer of OFTPD is
shown as the message of the panel. A user who is not an administrator can look
at the table, but OFTPD refuses a change.

- To add a partner, type its name into the field *New partner* and press Enter.
  If the partner is in the table already, the message says so\; use M to change
  it. Otherwise the entry panel follows.
- In front of a partner, type one of these letters\: *M* to modify it, *E* to
  enable it, *X* to disable it, *D* to delete it, *C* to call it now. The
  letter *S* selects a partner for a send\; it works only in the list that the
  Send panel shows, not here.

This is the entry panel for a new partner, after Enter was pressed with
nothing in the field of the Odette ID. The panel refuses it, and says so in its
title line.

#screen(raw(read("../ex/ug-ispf/partner-blank.txt")))

The Odette ID of another partner is refused in the same way. The message says
which partner has it.

#screen(raw(read("../ex/ug-ispf/partner-taken.txt")))

The entry panel has the keywords of an entry of the member: the Odette ID, the
two passwords, our own ID, the host, the port, the address, the level, the
mode, compression, call, sessions, translation, via and enabled. A new partner
starts with level 5, mode B, no compression, call and enabled, one session and
no translation. For a partner that is there, the panel shows its values. It shows
the passwords as *(set)* or *(none)*, never as text. A blank password keeps the
password as it is\; type a new one to replace it. Another field that you leave
as it is stays as it is, and a blank Host, Address or Via is cleared. Enter
saves, and END leaves the panel without saving.

When OFTPD takes the entry, the entry panel ends and the answer stands in the
message line of the partner list. Any answer with a code from 200 to 299 counts
as taken, #cmd("210") too\: for an enabled partner without a password the
message is #cmd("partner MAC2 added, but it has no PASSWORD"). The dialog shows
only the last line of the answer, not the line with the warning that the
utility writes before it. The line commands *E* and *X* show their answer in
the same place. This is the list after a partner was added without a password.

#screen(raw(read("../ex/ug-ispf/partner-added.txt")))

Any answer that is not a code from 200 to 299 keeps the entry panel open, with
the answer as its message, and you can correct the entry. That is a refusal
by OFTPD, or OFTPD busy, or OFTPD not reached, or an answer that ends before its
last line (#cmd("incomplete answer from OFTPD")) and so carries no code.

On an 80-column screen
the message line has 80 columns, and a longer answer is cut\; the answer of a refused
#cmd("DELETE") is one of these. The command #cmd("OFTP PARTNER DELETE") gives
the whole answer.

A partner of the member #cmd("OFTPPTN") cannot be deleted from the panel. The
answer says where to remove it.

#screen(raw(read("../ex/ug-ispf/delete-in-oftpptn.txt")))

The letter *D* shows a panel that asks you to confirm. Enter deletes the
partner, and END leaves it. OFTPD refuses it under the same rules as the
command: no session, no transfer in the spool, and no partner reached through
it. The deleted definition, with its passwords, cannot be got back.
