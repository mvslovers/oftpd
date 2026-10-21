#import "../bookmaster/bookmaster.typ": *

= Receive Rules <ref-rcv>

#idx("receive rules")#idx("OFTPRCV")
A receive rule says what OFTPD does with a file that a partner sends: under
which name the file is stored, whether a data set of that name is replaced,
on which volume and with how much space it is allocated, and whether its data
is translated. The rules are the member #cmd("OFTPRCV"), which the OFTPD
started task reads from the DD #cmd("OFTPRCV"). Without a rule OFTPD names the
data set after the virtual file name, as described in @cfg-dsn\; a rule
replaces that naming for the files it matches. This chapter describes the
member, the symbols that build a name, the checks OFTPD makes, and what
happens to a data set that is replaced. For the first rule, see the
_OFTPD User's Guide_.

== The Member OFTPRCV <rcv-member>

#idx("OFTPRCV", "member")#idx("OFTPRCV0")
The sample member #cmd("OFTPRCV0") is in #cmd("OFTPD.SAMPLIB"). Copy it to the
library that holds #cmd("OFTPPRM0") and #cmd("OFTPPTN0"), by default
#cmd("SYS2.PARMLIB"), and write the rules into it. The sample describes every
keyword in its comments and consists of comments only, which changes nothing.

OFTPD itself does not need the DD #cmd("OFTPRCV"). If the started task has no
such DD, OFTPD has no rules and says nothing about it. The sample procedure has
the DD and points it at the member through the parameter #cmd("R"), default
#cmd("OFTPRCV0"). With that procedure a member that is not in the library
does not stop the job either: OFTPD starts without rules and says nothing, at
the start and at a refresh. #cmd("OFTP024I") is then missing from the log.
Copy the member to the library, or delete the DD from your copy of the
procedure.

The member follows the format rules of @cfg-format: a list of
#cmd("KEYWORD=VALUE") pairs separated by blanks, tabs, commas or line ends,
comments with an asterisk in column 1, only columns 1 to 72 read, keywords and
values not case sensitive. #cmd("RULE=") opens a rule\; the keywords after it
belong to that rule until the next #cmd("RULE="). A rule need not fit on one
line. Other than in the two members of @cfg-format, a word that is not valid
does not just get skipped: it is an error that stops the whole member from
being used (@rcv-load).

#screen(```
RULE=ORDERS  FROM=MAC1  VFN=ORDERS.*
             DSN=ACCT.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
RULE=DAILY   FROM=*     VFN=DAILY
             DSN=ACCT.IN.%ID.DAILY  REPLACE=Y  VOLSER=WORK01  SPACE=30
```)

A member holds at most 64 rules. A member with any error is not used at all
(see @rcv-load).

== How a Rule Is Chosen <rcv-match>

#idx("receive rules", "matching")
When a partner offers a file, OFTPD looks through the rules in the order of
the member and takes the *first* rule that matches both the partner and the
virtual file name. That rule decides everything about the file. There is no
ranking by how specific a rule is: write the narrow rules before the wide
ones.

A rule matches when

- its #cmd("FROM=") is the name of the partner that sent the file, as it is in
  the partner table, or is #cmd("*"), and
- its #cmd("VFN=") pattern matches the virtual file name of the file.

In a #cmd("VFN=") pattern, #cmd("*") stands for any number of characters
(also none) and #cmd("?") for exactly one. Capital and small letters are not
distinguished. The pattern is matched against the whole virtual file name,
dots included: #cmd("ORDERS.*") matches #cmd("ORDERS.JAN") and
#cmd("ORDERS.2026.JAN"), but not #cmd("ORDERS") and not #cmd("OLD.ORDERS.JAN").

If no rule matches, the file is received as if there were no rules at all
(@cfg-dsn). A file that OFTPD passes on to another partner (#cmd("RELAY=Y"),
see @cfg-relay) meets no rule, because the rules are about files that end
here.

OFTPD chooses the rule once, when the partner starts the file. The rule stays
the same until the file is finished, even if a refresh reads another member
meanwhile.

== The Keywords <rcv-kw>

#idx("receive rules", "keywords")
@rcv-kw-tab lists the keywords. Only #cmd("RULE=") is required.

#tab(caption: [Keywords of a receive rule])[
  #table(columns: (1.1in, 1fr),
    [Keyword], [Meaning],
    [#cmd("RULE=")], [The name of the rule, 1 to 8 characters. It opens the
      rule. Names must differ within the member. The messages about a refused
      file give the name of the rule.],
    [#cmd("FROM=")], [A partner of the partner table, or #cmd("*") for any
      partner. Default #cmd("*"). A name that is not in the table is an error.],
    [#cmd("VFN=")], [The pattern for the virtual file name, up to 32
      characters, with #cmd("*") and #cmd("?"). Default #cmd("*").],
    [#cmd("DSN=")], [The name of the data set, up to 80 characters, with the
      symbols of @rcv-sym-tab. Without #cmd("DSN=") the file gets the name it
      would have without a rule (@cfg-dsn), also when the rule matched\; such a
      rule can still set #cmd("REPLACE="), #cmd("UNIT="), #cmd("VOLSER="),
      #cmd("SPACE="), #cmd("XLATE=") or #cmd("JOB=").],
    [#cmd("REPLACE=")], [#cmd("Y") (or #cmd("YES")): a data set of that name is
      replaced by the new file when the file is complete. #cmd("N") (or
      #cmd("NO")): the file is refused. Default #cmd("N"). See
      @rcv-replace. Both need #cmd("DSN=")\; in a rule without one, nothing is
      replaced or refused, and the name is made unique with the transfer key
      as described in @cfg-dsn.],
    [#cmd("UNIT=")], [The unit name for the data set, up to 8 characters.
      Default: the #cmd("UNIT") of #cmd("OFTPPRM").],
    [#cmd("VOLSER=")], [The volume serial for the data set, up to 6
      characters. Default: the #cmd("VOLSER") of #cmd("OFTPPRM").],
    [#cmd("SPACE=")], [The primary space in tracks, 1 to 9999. The secondary
      space is half of it plus 1. It is used only when the sender does not say
      how large the file is\; see @cfg-alloc. When the sender gives a size,
      the size decides.],
    [#cmd("XLATE=")], [#cmd("Y") or #cmd("N"): whether the data of this file is
      translated between ASCII and EBCDIC, instead of the #cmd("XLATE") of the
      partner entry. It has no effect on a file of text format, which is
      always translated.],
    [#cmd("JOB=")], [A job to submit when the file has arrived. See
      @cfg-job.],
  )
] <rcv-kw-tab>

#cmd("UNIT=") and #cmd("VOLSER=") are passed on to the allocation, and
nothing checks that the volume exists before the file arrives. A volume that
is not mounted, or has no room, shows up as message #cmd("OFTP220E") when
the file is offered.

*Space.* The sender may announce the size of a file, and OFTPD then allocates
from that size and ignores #cmd("SPACE="). In a test with #cmd("SPACE=30"), a
member of a partitioned data set (its size is not announced) got 30 tracks
primary and 16 secondary. A whole sequential data set of two records got the
minimum of 5 tracks and 3 secondary, and one of 2000 records, which uses 11
tracks on the sender, got 14 tracks and 8 secondary.

*Translation.* A file arrives either in ISO 8859-1, because the sender
translated it, or as the EBCDIC bytes that it had, and OFTPD cannot tell which.
#cmd("XLATE=Y") translates the data of a file of fixed, variable or undefined
format from ISO 8859-1 to EBCDIC\; #cmd("XLATE=N") stores it as it comes. The
rule decides if it has the keyword, and otherwise the partner entry. If the
choice does not fit what the sender does, the data set holds data that cannot
be read, and OFTPD says nothing. @rcv-xlate-tab shows what a test of two OFTPD
with a file of four fixed records produced. The rule won over the partner entry
both ways, #cmd("Y") over #cmd("N") and #cmd("N") over #cmd("Y").

#tab(caption: [The result of XLATE= in a test])[
  #table(columns: (2.0in, 0.9in, 1fr),
    [The sender sent], [In force], [Stored]
    , [ISO 8859-1 (#cmd("SEND ... XLATE"))], [#cmd("N")],
      [ISO 8859-1 bytes, not readable on MVS.]
    , [ISO 8859-1], [#cmd("Y")], [EBCDIC, identical to what was sent.]
    , [EBCDIC bytes (no #cmd("XLATE") on #cmd("SEND"))], [#cmd("N")],
      [EBCDIC, identical to what was sent.]
    , [EBCDIC bytes], [#cmd("Y")],
      [Garbled: the EBCDIC was taken for ISO 8859-1 and translated again.]
  )
] <rcv-xlate-tab>

== The Symbols <rcv-sym>

#idx("receive rules", "symbols")#idx("symbols", "in DSN")
In the name of #cmd("DSN=") OFTPD replaces the symbols of @rcv-sym-tab by the
values that belong to the file. Everything else in #cmd("DSN=") is taken
literally: letters, digits, the characters #cmd("@"), #cmd("#"), #cmd("$"),
#cmd("-"), and the dot between the qualifiers. OFTPD changes letters to upper
case. Any other character, and any symbol that is not in the table, is an
error in the rule.

#tab(caption: [Symbols in DSN=])[
  #table(columns: (1.4in, 1fr),
    [Symbol], [Value],
    [#cmd("%VFN")], [The virtual file name.],
    [#cmd("%Q1") to #cmd("%Q9")], [The first to the ninth qualifier of the
      virtual file name, that is the parts between the dots. A qualifier that
      the name does not have is empty, and the data set name then comes out
      not valid (@rcv-name).],
    [#cmd("%ID")], [The name of the partner in the partner table.],
    [#cmd("%YYYY"), #cmd("%MM"), #cmd("%DD")], [Year (4 digits), month and day
      of the arrival of the file, in the local time of the system on which
      OFTPD runs.],
    [#cmd("%HH"), #cmd("%MI"), #cmd("%SS")], [Hour, minute and second of the
      arrival.],
    [#cmd("%CTR")], [The 4 digits of the file counter of the sender. The
      sender announces a file with the time of its creation as
      #cmd("HHMMSScccc"), where the last four digits count the files created
      in the same second. A file whose time has no such counter is refused
      like one whose name is not valid (@rcv-name).],
    [#cmd("%KEY")], [The transfer key: #cmd("T") and 7 digits, as
      in #cmd("T0000012"). It is different for every file.],
  )
] <rcv-sym-tab>

The time of the arrival is the time at which OFTPD accepts the start of the
file, not the time at which the sender created it. Names of symbols are not
case sensitive, like the rest of the member.

*The virtual file name is used as it is.* The default naming of @cfg-dsn
drops characters that are not allowed, cuts qualifiers to 8 characters and
puts an #cmd("X") in front of a qualifier that starts with a digit. A rule
does none of this\; #cmd("%VFN") and #cmd("%Q1") to #cmd("%Q9") are put into the name
unchanged except for the upper case, and a name that does not come out valid
is refused (see @rcv-name). If you do not control the names that a partner
sends, use #cmd("%KEY") or #cmd("%ID") and the date for what you need to be
unique, and a fixed text for the rest.

== Names That Are Not Valid <rcv-name>

#idx("receive rules", "data set name check")
OFTPD checks the finished name when the file arrives, before it allocates
anything. A valid name has these properties.

- It consists of qualifiers separated by single dots, and none is empty.
- A qualifier has 1 to 8 characters. The first is a letter or one of
  #cmd("@"), #cmd("#") and #cmd("$"). Further characters may also be digits
  and the hyphen.
- The whole name has at most 44 characters.

The rule that a qualifier starts with a letter and has at most 8 characters
matters for dates. #cmd("%YYYY%MM%DD") alone is 8 characters, and with a
letter in front of it, 9. A date in one qualifier therefore needs the letter
and at most 7 characters in all, as in @rcv-date-tab.

#tab(caption: [Dates and times in a qualifier])[
  #table(columns: (2.2in, 1fr),
    [Qualifier in DSN=], [Result]
    , [#cmd("Y%YYYY")], [#cmd("Y2026"), 5 characters, valid.]
    , [#cmd("D%MM%DD")], [#cmd("D1021"), 5 characters, valid.]
    , [#cmd("T%HH%MI%SS")], [#cmd("T101500"), 7 characters, valid.]
    , [#cmd("D%YYYY%MM%DD")], [#cmd("D20261021"), 9 characters, not valid.]
    , [#cmd("%YYYY")], [#cmd("2026"), starts with a digit, not valid.]
  )
] <rcv-date-tab>

If the name is not valid, OFTPD refuses the file with the reason code 01 (file
name not valid), tells the sender not to try again, and writes
#cmd("OFTP224W") with the name of the rule that made the name. Nothing is
allocated and no transfer is kept in the spool. The partner has to send the
file under a name that the rule can handle, or you change the rule.

@rcv-reason-tab shows what OFTPD answers when it does not take a file, and
which message it writes. The reason codes are those of the OFTP protocol. A
refusal of the start of a file is a start-file negative answer (SFNA), and one
of its end an end-file negative answer (EFNA). Each carries a reason code, and
the start-file answer says whether the sender may try again.

#tab(caption: [Refusals that concern receive rules])[
  #table(columns: (1.6in, 1.0in, 0.7in, 1fr),
    [Situation], [Message], [Reason], [The sender is told],
    [The name comes out invalid], [#cmd("OFTP224W")], [01], [Not to try again.],
    [The data set exists and the rule has #cmd("REPLACE=N")],
      [#cmd("OFTP225W")], [13], [To try again later.],
    [#cmd("REPLACE=Y"), and the data set is not a plain sequential data set],
      [#cmd("OFTP234W")], [12], [Not to try again.],
    [The data set for the file cannot be created],
      [#cmd("OFTP220E")], [various], [To try again later.],
    [At the end of the file the old data set cannot be put aside],
      [#cmd("OFTP226E")], [12], [That the file was not accepted (an
        end-file negative answer), without a retry.],
    [At the end of the file the new data set cannot be given its name],
      [#cmd("OFTP221E")], [12], [As for #cmd("OFTP226E").],
  )
] <rcv-reason-tab>

What a sender does with a refusal is its own business\; for OFTPD as the
sender, see @cfg-retry.

== Existing Data Sets and REPLACE <rcv-replace>

#idx("REPLACE")#idx("receive rules", "REPLACE")
A file is received into a data set whose name is built from #cmd("TMPPREFIX")
and the transfer key, and it gets its final name only when it is complete and
closed (@cfg-dsn). What happens when a data set of the final name exists
already depends on the rule.

*#cmd("REPLACE=N") (default).* OFTPD finds out at the start of the file that
the data set exists. It refuses the file with the reason code 13 (the file is
a duplicate), tells the sender to try again later, and writes #cmd("OFTP225W")
with the name of the data set and of the rule. Nothing is allocated. A sender
that keeps trying keeps being refused until you delete the data set or change
the rule.

*#cmd("REPLACE=Y").* OFTPD receives the file as usual and does nothing to the
old data set until the new file is complete and closed. Then it does three
things, in this order.

+ It renames the old data set to #cmd("TMPPREFIX.B") followed by the 7 digits
  of the transfer key. For the transfer #cmd("T0000012") and the default
  #cmd("TMPPREFIX") this is #cmd("OFTP.TMP.B0000012"). This is the *backup*.
+ It renames the new data set to the final name.
+ It deletes the backup and writes #cmd("OFTP229I").

A file that fails on the way therefore costs nothing: the old data set is
still there under its own name, because nothing touches it before the file is
complete. In addition:

- *Only a plain sequential data set can be replaced.* If the name is that of a
  partitioned data set or of a VSAM cluster, OFTPD does not touch it. It refuses the file at its start with the reason code 12 and
  #cmd("OFTP234W"), and tells the sender not to try again. If the data set
  changes into such a name while the file is arriving, the file is refused at
  its end with #cmd("OFTP226E").
- *A data set that OFTPD cannot rename cannot be replaced.* This can be so
  when another job has it allocated, or when the catalog is full. If the first
  rename fails, OFTPD refuses the file at its end with the reason code 12 and
  writes #cmd("OFTP226E"). The old data set stays as it was, and the
  temporary data set is deleted.
- If the second rename fails, OFTPD renames the backup to the final name
  again, deletes the temporary data set, refuses the file with the reason
  code 12, and writes #cmd("OFTP221E"). If renaming the backup back fails
  too, it writes #cmd("OFTP227E") first, and the old data set is under the
  name of the backup\; you rename it back by hand.
- If OFTPD cannot delete the backup, it writes #cmd("OFTP228W"). The new file
  is in place\; the backup is a spare copy, and you delete it when you have
  looked at it.

#note[*The backup is visible.* For the short time of a replace the old content
is under the name of the backup. Whoever may read data sets under
#cmd("TMPPREFIX") can read it. Protect #cmd("TMPPREFIX") like the data sets
that your rules write to.]

The started task needs the authority to create the data sets of the rules.
Replacing also renames and deletes data sets, which may need more authority
than creating them. Check what your security product asks for, and give the
user ID of the started task no more than the names of the rules need.

=== After a Stop in the Middle of a Replace <rcv-recover>

#idx("REPLACE", "recovery")#idx("OFTP239I")#idx("OFTP248W")
If OFTPD or the system stops in the instant between the first and the second
rename, the final name does not exist, and the backup is the *only copy* of
the old data set. Do not delete it. The transfer is still active in the spool
(the close did not complete), so at the next start OFTPD discards the partial
receive, as it does for every file that was arriving (described in the _OFTPD User's Guide_), and looks
at the backup of that transfer.

- If the final name does not exist, OFTPD renames the backup to it and writes
  #cmd("OFTP239I"). The old data set is back, and the partner has to send the
  file again.
- If the final name exists and so does the backup, the second rename had
  happened already. OFTPD leaves both and writes #cmd("OFTP248W"). The final
  name holds the new file, which is complete\; the backup is the old version.
  Compare them, and delete the backup when you do not need it.
- If the rename fails, OFTPD writes #cmd("OFTP227E") and names the backup.

Both messages were produced in a test. The monitor was cancelled while it
received a large file for a rule with #cmd("REPLACE=Y"), and the old data set
was then renamed by hand to the name of the backup, as the first rename would
have done. At the next start the log said

#screen(raw(read("../ex/ug-rules/recover-log.txt")))

For the second message the test left the old data set where it was and put a
data set of the name of the backup next to it by hand. The message is cut at
126 characters on the console\; the log has it whole.

#screen(raw(read("../ex/ug-rules/recover-both-log.txt")))

OFTPD looks for a backup only for a transfer that the spool shows as
interrupted, and it forms the name of the backup from the current
#cmd("TMPPREFIX"). After a stop, do not change #cmd("TMPPREFIX") and do not
define a new spool before the next start. If you have lost the spool, look for
data sets under #cmd("TMPPREFIX") whose last qualifier starts with a #cmd("B")
and is followed by 7 digits.

If you have to restore a data set by hand, do this. Find out whether the final
name exists. If it does not, rename the backup to the final name. Delete the
backup only if the final name exists and is the file you want.

== Keeping the Name Space Safe <rcv-safe>

#idx("receive rules", "security")#idx("first qualifier")
A rule builds a data set name from text that a partner chooses, and the
started task creates, and with #cmd("REPLACE=Y") replaces, data sets of that
name. A rule that lets the partner choose the first qualifier would let the
partner create, and replace, any data set the started task may write. OFTPD
therefore does not allow it.

- The *first qualifier*, the part of #cmd("DSN=") before the first dot, must not
  contain #cmd("%VFN"), #cmd("%Q1") to #cmd("%Q9") or #cmd("%CTR"). A pattern
  with no dot is its own first qualifier. #cmd("DSN=ACCT.IN.%VFN") is valid\;
  #cmd("DSN=%VFN"), #cmd("DSN=ACCT%VFN") and #cmd("DSN=%Q1.X") are errors, and
  so is the member. #cmd("%ID"), #cmd("%KEY") and the symbols of the date may
  stand anywhere.
- A #cmd("DSN=") that can make a name under #cmd("TMPPREFIX") is an error. OFTPD
  compares the fixed beginning of the pattern, up to its first symbol, with
  #cmd("TMPPREFIX") and a dot. That is where the temporary data sets and the
  backups are, and a file must not be able to take their place. A pattern
  whose first qualifier is built from #cmd("%ID"), #cmd("%KEY") or a date
  symbol is not checked, because OFTPD cannot tell what it will make.

These checks do not make a rule safe by themselves. A rule such as
#cmd("DSN=ACCT.IN.%VFN") with #cmd("REPLACE=Y") lets a partner replace every
sequential data set under #cmd("ACCT.IN") that it can name. Use
#cmd("REPLACE=Y") only for a partner and a name you trust, and keep the fixed
part of the name as long as the application allows.

== Reading the Rules: Start and Refresh <rcv-load>

#idx("receive rules", "refresh")#idx("OFTP024I")#idx("OFTP025E")#idx("OFTP026W")
OFTPD reads the member when it starts, after the partner table, and again at
every #cmd("F OFTPD,REFRESH") (@cfg-refresh). It reads the whole member before
it uses a line of it.

- If the member has no error, it replaces all the rules that OFTPD had, and
  OFTPD writes #cmd("OFTP024I") with the number of rules. The new rules decide
  the files that start after that.
- If the member has any error, OFTPD does not use it. It writes
  #cmd("OFTP010W") for each line in error, and then #cmd("OFTP025E") with the
  number of errors. At start this means no rules at all\; at a refresh the
  rules that were in force stay, and the message says so. Files that arrive
  meanwhile follow them.
- If a refresh cannot open the DD #cmd("OFTPRCV") and OFTPD had rules, it
  writes #cmd("OFTP026W") and the rules stay. At the start, a DD that cannot
  be opened is not reported.

These are errors of the member:

- a word that is not #cmd("KEYWORD=VALUE"), a keyword that OFTPD does not know,
  a value that is not valid, or a keyword before the first #cmd("RULE="),
- more than 64 rules, or two rules with the same name,
- a #cmd("DSN=") with a symbol that does not exist, a character that is not
  allowed, more than 80 characters, a first qualifier that is not fixed, or a
  name under #cmd("TMPPREFIX") (@rcv-safe),
- a #cmd("FROM=") that names no partner of the partner table.

The last one connects the two members. A partner has to be in the table
before a rule may name it, and OFTPD reads the partner table first. After
you add a partner and a rule for it, a single #cmd("REFRESH") brings in both.
If you remove a partner, remove its rules as well, or the refresh keeps the
old rules.

== What Is Not Part of the Rules <rcv-not>

Receive rules do not pass a file on to another partner (that is
#cmd("RELAY=Y")), do not write a file to a generation data set, to SYSOUT or
to tape, and do not start anything but the one job of #cmd("JOB=").
