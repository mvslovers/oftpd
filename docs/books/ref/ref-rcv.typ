#import "../bookmaster/bookmaster.typ": *

= Receive Rules <ref-rcv>

#idx("receive rules")#idx("OFTPRCV")
A receive rule decides what OFTPD does with a file that a partner sends to
this system. A rule can choose the name of the data set, the volume and the
space it is allocated with, whether the data is translated, whether an
existing data set of that name is replaced, and a job to submit when the file
has arrived. The rules are in a member that the OFTPD started task reads from
its JCL, the DD #cmd("OFTPRCV").

The rules are optional. A file that meets no rule is received as it is
described in @cfg-dsn and @cfg-alloc: into a data set named from
#cmd("INPREFIX") and the virtual file name, with the settings of
#cmd("OFTPPRM"). A member without any rule changes nothing.

A rule applies to a file that ends at this system. It does not apply to a file
that OFTPD passes on to another partner (relaying, #cmd("RELAY=Y") in
#cmd("OFTPPRM")), and it does not apply to a file that OFTPD refuses because it
is not addressed to this system (message #cmd("OFTP223W")). A rule does not
apply to the files that OFTPD sends.

== The Member OFTPRCV <rcv-member>

#idx("OFTPRCV", "member")#idx("OFTPRCV0")
The DD #cmd("OFTPRCV") names a member of a partitioned data set. The sample
procedure #cmd("OFTPD") sets it with the symbolic parameter #cmd("R") (default
#cmd("OFTPRCV0")) in the data set #cmd("D"), the library of the other two
members, and the sample #cmd("OFTPRCV0") comes with comments that explain the
keywords. OFTPD reads the member when it starts, after the partner table, and
again with every refresh (see @rcv-refresh). If the DD is missing when OFTPD
starts, there are no rules, and OFTPD writes no message.

The member follows the rules of @cfg-format, with these differences.

- A line starts a rule with #cmd("RULE=name"). The keywords that follow, on the
  same line or on the lines after it, belong to that rule until the next
  #cmd("RULE="). A keyword before the first #cmd("RULE=") is an error. The order
  of the keywords within a rule does not matter. If a keyword is given twice,
  the last one counts.
- A word without an equals sign is reported with the word itself:
  #cmd("line n: word is not KEYWORD=VALUE"). The partner table does not show
  the word.
- An unknown keyword is an error. In #cmd("OFTPPRM") it is ignored.
- Every error is reported with message #cmd("OFTP010W"), which names the member
  as #cmd("OFTPRCV"), the line and the fault. OFTPD goes on reading, so that
  it reports all errors in one pass.
- The member is used all or not at all. If it has a single error, OFTPD uses
  none of it and writes #cmd("OFTP025E") (see @rcv-refresh for what that
  means at a refresh). If it has none, OFTPD writes #cmd("OFTP024I") with the
  number of rules.
- The member holds at most 64 rules.

A member looks like this.

#screen(```
* Orders from PARTNER1 go to a data set with the date in its name.
RULE=ORDERS  FROM=PARTNER1  VFN=ORDERS.*
             DSN=ACCT.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS
* A daily file replaces the one of the day before.
RULE=DAILY   VFN=DAILY
             DSN=ACCT.IN.%ID.%VFN  REPLACE=Y
```)

After the last line OFTPD makes three more checks, and each fault is reported
with #cmd("OFTP010W") too.

- #cmd("FROM") must name a partner that is in the partner table
  (#cmd("line n: rule name: FROM=partner is no partner")).
- The names of the rules must be different
  (#cmd("line n: rule name is defined twice")).
- #cmd("DSN") must not be able to name a data set under #cmd("TMPPREFIX")
  (#cmd("line n: rule name: DSN may name a data set under TMPPREFIX=prefix")).
  @rcv-dsn explains this.

The line number is the number of the record in the member, counting comments.

== The Keywords <rcv-kw>

#idx("receive rules", "keywords")
@rcv-kw-tab lists the keywords. A value is the text up to the next blank,
tab or comma, so it holds none of these. OFTPD reads it as upper case.

#tab(caption: [Keywords of a receive rule])[
  #table(columns: (1.0in, 1.1in, 1fr),
    [Keyword], [Default], [Meaning],
    [#cmd("RULE")], [none], [The name of the rule, 1 to 8 characters. It must
      be different from the name of every other rule. Messages show it. It
      opens a rule.],
    [#cmd("FROM")], [#cmd("*")], [The partner that the rule is for: a name from
      the partner table, up to 8 characters, or #cmd("*") for any partner.],
    [#cmd("VFN")], [#cmd("*")], [A pattern for the virtual file name, 1 to 32
      characters. @rcv-match explains the pattern.],
    [#cmd("DSN")], [none], [A pattern for the name of the data set, 1 to 80
      characters, with symbols. @rcv-dsn explains it. Without #cmd("DSN") the
      file is named as if no rule matched.],
    [#cmd("REPLACE")], [#cmd("N")], [#cmd("Y") or #cmd("YES"): a data set of
      that name that exists is replaced when the new file is complete.
      #cmd("N") or #cmd("NO"): the file is refused. @rcv-replace explains it.],
    [#cmd("UNIT")], [#cmd("UNIT") of #cmd("OFTPPRM")], [The unit for the data
      set, up to 8 characters.],
    [#cmd("VOLSER")], [#cmd("VOLSER") of #cmd("OFTPPRM")], [The volume serial
      for the data set, up to 6 characters.],
    [#cmd("SPACE")], [see @rcv-alloc], [The space in tracks, a whole number
      from 1 to 9999. OFTPD uses it only if the sender does not announce a
      size.],
    [#cmd("XLATE")], [that of the partner], [#cmd("Y") or #cmd("YES"):
      translate the data of this file. #cmd("N") or #cmd("NO"): do not.],
    [#cmd("JOB")], [none], [#cmd("pds(member)"): a job to submit when the file
      has arrived. @cfg-job describes it.],
  )
] <rcv-kw-tab>

A keyword that is not in the table is an error. So is a value that is empty,
longer than the table says, or not one of the values listed. OFTPD does not
check that a unit or a volume exists: if the data set cannot be allocated
there, the file is refused (message #cmd("OFTP220E")).

== Which Rule Applies <rcv-match>

#idx("receive rules", "matching")
When a partner offers a file, OFTPD looks at the rules in the order of the
member. The first rule that matches decides, and OFTPD does not look at the
rules after it. If none matches, the file is received as if there were no
rules. Put the narrow rules before the wide ones: a rule with the default
#cmd("FROM") and #cmd("VFN") matches every file, so the rules after it never
apply.

A rule matches when both of these hold.

- #cmd("FROM") is #cmd("*"), or it is the name of the partner that is in the
  session.
- The pattern #cmd("VFN") matches the whole virtual file name.

In the pattern, an asterisk stands for any number of characters, also none, and
a question mark stands for exactly one character. The dot is an ordinary
character. Upper and lower case are the same.

#tab(caption: [Examples for the pattern in VFN])[
  #table(columns: (1.5in, 1fr),
    [Pattern], [Matches],
    [#cmd("ORDERS.*")], [#cmd("ORDERS.JAN"), #cmd("orders.jan.old") and
      #cmd("ORDERS."), but not #cmd("ORDERS") and not #cmd("MYORDERS.JAN").],
    [#cmd("INV?????.DATA")], [#cmd("INV00123.DATA"), but not
      #cmd("INV123.DATA") and not #cmd("INV001234.DATA").],
    [#cmd("*.TXT")], [#cmd("REPORT.TXT") and #cmd("A.B.TXT"), but not
      #cmd("REPORT.TXT2").],
    [#cmd("DAILY")], [#cmd("DAILY") and #cmd("daily"), nothing else.],
  )
]

OFTPD copies the rule into the transfer when the file opens. A refresh of the
rules therefore changes nothing for a file that is being received: it keeps
the rule it started with, for its name, its allocation, #cmd("REPLACE") and
#cmd("JOB").

== The Name of the Data Set <rcv-dsn>

#idx("receive rules", "data set name")#idx("symbols", "in DSN")
The pattern of #cmd("DSN") is made of literal characters and symbols. A literal
character is a letter, a digit, or one of the characters #cmd("."), #cmd("-"),
#cmd("@"), #cmd("#") and #cmd("$"). Anything else is an error, and so is a
percent sign that does not begin one of the symbols of @rcv-sym-tab.

#tab(caption: [Symbols in DSN])[
  #table(columns: (1.1in, 1fr),
    [Symbol], [Value],
    [#cmd("%VFN")], [The virtual file name, as the partner sent it, in upper
      case.],
    [#cmd("%Q1") to #cmd("%Q9")], [The first to the ninth qualifier of the
      virtual file name, the parts between the dots. If the name has fewer
      qualifiers, the symbol stands for nothing.],
    [#cmd("%ID")], [The name of the partner, as in the partner table.],
    [#cmd("%YYYY")], [The year, 4 digits.],
    [#cmd("%MM"), #cmd("%DD")], [The month and the day, 2 digits each.],
    [#cmd("%HH"), #cmd("%MI"), #cmd("%SS")], [The hour (00 to 23), the minute
      and the second, 2 digits each.],
    [#cmd("%CTR")], [The last 4 digits of the time field of the file
      description, which the sender uses as a counter of its files. If the
      field has fewer than 10 characters, or its last 4 are not digits, the
      symbol has no value.],
    [#cmd("%KEY")], [The spool key of the transfer: #cmd("T") and 7 digits.],
  )
] <rcv-sym-tab>

The date and the time are those of the system, in local time, when OFTPD
opens the file, and not those of the file description.

OFTPD makes the name of the data set when the file arrives, and then checks it
as a data set name.

- It has 1 to 8 characters in a qualifier, and no qualifier is empty.
- A qualifier starts with a letter or one of #cmd("@"), #cmd("#") and
  #cmd("$"). The other characters are letters, digits, the same three
  characters, and the hyphen.
- The name is at most 44 characters long.

A file whose name does not pass is refused (see @rcv-refuse). OFTPD does not
change the text of a symbol to make it fit, as it does for the name built from
#cmd("INPREFIX"): a virtual file name with an underscore or a long qualifier
makes a name that is not valid. It also happens when a symbol has no value, and
the name is left with an empty qualifier. Choose the pattern in #cmd("VFN") so
that the names it lets in make valid qualifiers.

OFTPD checks the characters and symbols of the pattern when it reads the
member, but it does not check the length of a qualifier, because that depends
on the file. Count the qualifiers that hold symbols. A date is the usual
trouble: #cmd("Y%YYYY") has 5 characters, #cmd("D%MM%DD") 5 and
#cmd("T%HH%MI%SS") 7, but #cmd("D%YYYY%MM%DD") has 9 and refuses every file.
The key #cmd("%KEY") has 8 characters and fits alone in a qualifier.

#tab(caption: [Examples for DSN, with the data set name that comes of them])[
  #table(columns: (2.0in, 1.7in, 1fr),
    [DSN], [Virtual file name], [Data set name],
    [#cmd("ACCT.ORDERS.Y%YYYY.D%MM%DD.T%HH%MI%SS")], [#cmd("ORDERS.JAN")],
      [#cmd("ACCT.ORDERS.Y2026.D1020.T235609"), for 20 October 2026,
      23:56:09],
    [#cmd("ACCT.IN.%ID.%VFN")], [#cmd("INV00123.DATA") from #cmd("MAC1")],
      [#cmd("ACCT.IN.MAC1.INV00123.DATA")],
    [#cmd("PAY.%ID.%Q2.%Q3")], [#cmd("PAYROLL.NORTH.JAN") from #cmd("MAC1")],
      [#cmd("PAY.MAC1.NORTH.JAN")],
    [#cmd("ACCT.IN.%ID.%KEY")], [any, from #cmd("MAC1")],
      [#cmd("ACCT.IN.MAC1.T0000012"), for the transfer #cmd("T0000012")],
    [#cmd("ACCT.IN.%VFN")], [#cmd("monthly-report-2026.csv")],
      [none: the qualifier #cmd("MONTHLY-REPORT-2026") is too long, the file
      is refused],
    [#cmd("ACCT.IN.%VFN")], [#cmd("2026.LOG")],
      [none: #cmd("2026") starts with a digit, the file is refused],
    [#cmd("PAY.%ID.%Q3")], [#cmd("PAYROLL.NORTH")],
      [none: #cmd("%Q3") has no value, so the name ends in a dot, and the file
      is refused],
  )
]

=== The First Qualifier

#idx("receive rules", "first qualifier")#idx("security", "receive rules")
The virtual file name is the partner's choice. A rule that lets it into the
first qualifier would let a partner create, and with #cmd("REPLACE=Y") replace,
any data set that the started task may write. OFTPD therefore does not accept
#cmd("%VFN"), #cmd("%Q1") to #cmd("%Q9") and #cmd("%CTR") before the first dot
of the pattern, or anywhere in a pattern that has no dot. The member is in error
(#cmd("DSN: the first qualifier must be literal")). #cmd("DSN=%VFN"),
#cmd("DSN=PROD%VFN") and #cmd("DSN=%Q1.X") are of this kind\; #cmd("DSN=ACCT.IN.%VFN")
is not. The symbols #cmd("%ID"), #cmd("%KEY") and the date and time are
allowed in the first qualifier.

OFTPD also rejects a pattern that can name a data set under #cmd("TMPPREFIX"),
because that is where it keeps the files it receives and the old data sets it
replaces. It compares the literal start of the pattern, up to its first symbol,
with #cmd("TMPPREFIX") and a dot, as far as the shorter of the two goes. With
#cmd("TMPPREFIX=OFTP.TMP"), the patterns #cmd("OFTP.TMP.X") and #cmd("OFTP") are
rejected, and #cmd("OFTP.IN.X") is not. OFTPD does not compare a first
qualifier that is built from symbols, such as #cmd("%ID.X"). Do not write a
pattern that can make a name under #cmd("TMPPREFIX") that way.

=== A Rule Without DSN

A rule that has no #cmd("DSN") names the file as if no rule matched: from
#cmd("INPREFIX") and the virtual file name as in @cfg-dsn, and with the
transfer key as an extra qualifier if the name is taken. The other keywords of
the rule still apply. #cmd("REPLACE") has nothing to act on in this case.

=== A Name That Is Taken

If the rule has a #cmd("DSN"), OFTPD does not change the name when a data set of
that name exists, and it does not add the key. Without #cmd("REPLACE=Y") the file
is refused. If every file is to be kept, make the name differ with
#cmd("%KEY") or with the date and the time.

== Space, Volume and Translation <rcv-alloc>

#idx("receive rules", "allocation")
The file is first received into a temporary data set under #cmd("TMPPREFIX"),
and renamed to its final name when it is complete (see @cfg-dsn). The
#cmd("UNIT") and the #cmd("VOLSER") of the rule take the place of those of
#cmd("OFTPPRM"), each one for itself: a rule that has only #cmd("VOLSER") uses
the #cmd("UNIT") of #cmd("OFTPPRM"). The temporary data set is created with them.

#cmd("SPACE=n") counts only if the file description gives no size, that is, the
size is 0. The primary quantity is then n tracks, and the secondary quantity is
half of n plus 1, rounded down. If the sender does give a size, the rule
for it in @cfg-alloc holds and #cmd("SPACE") is not used. Without
#cmd("SPACE"), a file of size 0 gets a primary and a secondary quantity of 15
tracks.

#cmd("XLATE") of the rule takes the place of #cmd("XLATE") of the partner for
this file. It matters for the formats F, V and U\; a file of format T is
always translated.

== Replacing a Data Set <rcv-replace>

#idx("receive rules", "REPLACE")#idx("REPLACE")
With #cmd("REPLACE=Y") a new file takes the place of a data set of the same
name. This works for a plain sequential data set only. OFTPD does it by
renaming, so that the old data is kept until the new file is complete.

+ When the file opens, OFTPD looks for the final name. If it exists and
  #cmd("REPLACE=N") holds, OFTPD refuses the file (#cmd("OFTP225W")). If it
  exists and is not a plain sequential data set, a partitioned data set, a
  member or a VSAM data set for instance, OFTPD refuses the file whatever
  #cmd("REPLACE") says (#cmd("OFTP234W")). OFTPD leaves the data set alone.
+ The file is received into the temporary data set
  #cmd("TMPPREFIX.T")#var("nnnnnnn"). The old data set is not touched. If the
  transfer fails, the old data set stays as it was.
+ When the file is complete, OFTPD looks at the final name again. If a data set
  is there, OFTPD renames it to #cmd("TMPPREFIX.B")#var("nnnnnnn"), the backup
  name, where #var("nnnnnnn") are the 7 digits of the transfer key. For the
  transfer #cmd("T0000012") the temporary name is #cmd("OFTP.TMP.T0000012")
  and the backup name is #cmd("OFTP.TMP.B0000012").
+ OFTPD renames the temporary data set to the final name.
+ OFTPD deletes the backup and writes #cmd("OFTP229I").

A rename can fail, for example when another job has the old data set allocated.
@rcv-rep-tab lists what happens.

#tab(caption: [When a step of a replacement fails])[
  #table(columns: (1.7in, 1fr),
    [Failure], [Result],
    [The old data set cannot be renamed to the backup name, or it is not a plain
      sequential data set any more.], [Message #cmd("OFTP226E"). The old data
      set stays as it was. OFTPD deletes the new one and answers the partner
      with a negative end-of-file response, reason 12.],
    [The temporary data set cannot be renamed to the final name.], [OFTPD
      renames the backup to the final name again. If that works, only
      #cmd("OFTP221E") is written\; if not, #cmd("OFTP227E") comes first.
      The partner gets reason 12.],
    [The backup cannot be deleted.], [The new data set is in place and complete,
      and the partner gets its answer. Message #cmd("OFTP228W") names the
      backup, which holds the old data.],
  )
] <rcv-rep-tab>

If the monitor or the system stops while a replacement is under way, the old
data set can be left under the backup name. At the next start OFTPD looks for
such a backup for every transfer that was in progress.

- If the final name is free, OFTPD renames the backup back and writes
  #cmd("OFTP239I"). The old data set is as it was.
- If a data set is under the final name as well, the rename of the new file had
  worked. OFTPD leaves both and writes #cmd("OFTP248W").
- If the rename back fails, OFTPD writes #cmd("OFTP227E").

In none of these cases does OFTPD send a receipt for the file: the transfer is
dropped from the spool.

@rcv-op-tab says what the operator does.

#tab(caption: [What to do after a message about a replacement])[
  #table(columns: (1.2in, 1fr),
    [Message], [Action],
    [#cmd("OFTP226E")], [Nothing is lost. Find out why the old data set cannot
      be renamed, and have the partner send the file again.],
    [#cmd("OFTP227E")], [The old data set is now under the backup name that the
      message shows. Rename it to the other name in the message. If a data set
      of that name is there, decide which one to keep.],
    [#cmd("OFTP228W")], [The new data is in place. Delete the backup named in
      the message when you do not need the old data.],
    [#cmd("OFTP248W")], [The final name holds the new file, which was complete.
      The backup holds the old data. Delete the backup when you do not need the
      old data.],
    [#cmd("OFTP239I")], [None.],
  )
] <rcv-op-tab>

== Refused Files <rcv-refuse>

#idx("receive rules", "refusals")
OFTPD tells the partner why it refuses a file with a reason code and, at the
start of a file, whether the sender may offer it again. No text goes to the
partner. The text is in the messages of OFTPD. A file that is refused is not
kept in the spool.

#tab(caption: [Refusals that come from a rule])[
  #table(columns: (0.9in, 1fr, 1.0in),
    [Message], [Cause], [Reason sent],
    [#cmd("OFTP224W")], [The rule makes no valid data set name (see
      @rcv-dsn).], [Start of file, 01, no retry],
    [#cmd("OFTP225W")], [The data set exists and the rule has #cmd("REPLACE=N").
      ], [Start of file, 13, retry],
    [#cmd("OFTP234W")], [#cmd("REPLACE=Y"), and the data set that exists is not
      a plain sequential data set.], [Start of file, 12, no retry],
    [#cmd("OFTP220E")], [The temporary data set cannot be allocated, with the
      unit, the volume and the space of the rule.], [Start of file, the reason
      in the message, retry],
    [#cmd("OFTP226E"), #cmd("OFTP221E")], [A step of the replacement or the
      final rename failed (see @rcv-replace).], [End of file, 12],
  )
]

The reason for a data set that exists is 13 (duplicate file), for a name that
is not valid 01 (invalid file name), and for a failure with the data set 12.
The answer at the end of a file has no indication of a retry. A file that the
partner offers again after OFTPD has received it, with the same originator,
virtual file name, date and time, gets reason 13 without a retry, and OFTPD
sends the receipt once more. It does not make a data set name for it.

== Changing the Rules <rcv-refresh>

#idx("receive rules", "refresh")#idx("REFRESH")
The rules are read into storage when OFTPD starts. Editing the member changes
nothing until OFTPD reads it again. The operator command
#cmd("F OFTPD,REFRESH") reads the partner table, and after it the rules. So does
the #cmd("REFRESH") request of an administrator. Every refresh of the partner
table reads the rules again. If the partner table cannot be read, the rules
are not read either.

The result is in the console and in the log. The utility and the dialog show
only the answer about the partners.

- #cmd("OFTP024I") says how many rules OFTPD has read, and the new rules apply
  to the files that open from then on.
- #cmd("OFTP025E") says that the member has errors. At a refresh the rules
  that OFTPD had stay. At the start OFTPD has no rules, and every file is
  received as if there were none.
- #cmd("OFTP026W") says that the DD cannot be opened at a refresh. The rules
  that OFTPD has stay. If OFTPD has no rules, it writes nothing.

The check of #cmd("FROM") uses the partner table as it is after the refresh.
If you remove a partner, remove the rules that name it in the same step, or the
member is not used. The member that #cmd("JOB") names is not read with the
rules. OFTPD reads it for every file, and a change to it applies at once.

== Security <rcv-safe>

#idx("security", "receive rules")
The started task creates the data sets and replaces them with its own
authority. A rule decides which names the partner can make it use, because the
partner chooses the virtual file name.

- Protect the member and the library that holds it as you protect
  #cmd("SYS2.PARMLIB"). Whoever can change the member decides where files go.
  This goes for the members named by #cmd("JOB") too.
- Begin every pattern with a fixed qualifier that is not shared with other
  applications, as in #cmd("ACCT.IN.%VFN"). A short fixed part such as
  #cmd("ACCT") lets the partner reach any data set below it that the started
  task may write.
- Keep #cmd("VFN") as narrow as the application allows, for example
  #cmd("INV?????.DATA") and not #cmd("*").
- Use #cmd("REPLACE=Y") only with a pattern that cannot reach more than the
  data sets that are meant to be replaced. A name made of the partner's
  virtual file name can be any name the pattern allows.
- Give the started task access only to the high-level qualifiers that the
  rules use, and to the prefixes #cmd("INPREFIX") and #cmd("TMPPREFIX").
- The advice for #cmd("JOB") is in @cfg-job-safe.

== What the Rules Do Not Do <rcv-not>

#idx("receive rules", "limits")
- A rule chooses. It cannot refuse a file by itself, but a name that is not
  valid makes OFTPD refuse the file.
- A rule looks only at the partner and the virtual file name. It does not look
  at the format, the size, the destination or the user data.
- A rule does not change the record format and the record length. They come
  from the file description (see @cfg-alloc).
- A rule cannot name a member of a partitioned data set or a VSAM data set.
- A rule does not set an owner or a protection profile for the data set.
- A rule does not apply to files that OFTPD sends or relays, or to a file that
  the partner offers again after OFTPD received it.
- A rule does not make the receipt wait. The receipt says that the file
  has arrived, whatever a job does with it.
