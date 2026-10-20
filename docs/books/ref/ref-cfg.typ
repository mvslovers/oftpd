#import "../bookmaster/bookmaster.typ": *

= Configuration and the Partner Table <ref-cfg>

#idx("configuration")#idx("OFTPPRM")#idx("OFTPPTN")
OFTPD is configured by two members that the OFTPD started task reads from
its JCL: #cmd("OFTPPRM") holds the settings of the monitor, and #cmd("OFTPPTN")
holds the partner table. This chapter describes both, the data sets the
started task needs, the way OFTPD names and allocates the data sets it
receives, and what a refresh of the partner table does and does not change.
How to set up a first partner is described in the _OFTPD User's Guide_.

== The Data Definitions of the Started Task <cfg-dd>

#idx("started task", "DD statements")
The OFTPD started task needs the data definitions in @cfg-dd-tab. The sample
procedure that comes with OFTPD sets them up with the symbolic parameters
#cmd("M") (the member of #cmd("OFTPPRM"), default #cmd("OFTPPRM0")), #cmd("P")
(the member of #cmd("OFTPPTN"), default #cmd("OFTPPTN0")), #cmd("D") (the
partitioned data set that holds both, default #cmd("SYS2.PARMLIB")) and
#cmd("S") (the spool, default #cmd("OFTP.SPOOL")), and a region of 8 MB.

#tab(caption: [Data definitions of the OFTPD started task])[
  #table(columns: (1.6in, 1fr),
    [DD name], [Content],
    [#cmd("OFTPPRM")], [The settings of the monitor, described in
      @cfg-prm. Read once, when OFTPD starts.],
    [#cmd("OFTPPTN")], [The partner table, described in @cfg-ptn. Read when
      OFTPD starts, and again by the #cmd("REFRESH") command. The partners
      that are kept by command (@cfg-cmd) are not in it.],
    [#cmd("OFTPSPL")], [The spool, a VSAM data set that holds one record for
      each transfer and one for each partner that is kept by command. Allocate
      it with #cmd("DISP=OLD").],
    [#cmd("OFTPLOG")], [The log, a data set with fixed or fixed blocked
      records of at least 143 bytes. The sample procedure writes it to
      SYSOUT with a record length of 160.],
    [#cmd("SYSPRINT"), #cmd("STDOUT"), #cmd("STDERR"), #cmd("SYSUDUMP")],
      [Standard output and the dump data set.],
  )
] <cfg-dd-tab>

If OFTPD cannot open the log, all messages go to the console only.

== Rules for Both Members <cfg-format>

#idx("OFTPPRM", "format")#idx("OFTPPTN", "format")
#cmd("OFTPPRM") and #cmd("OFTPPTN") follow the same rules.

- A member is a list of #cmd("KEYWORD=VALUE") pairs. Blanks, tabs and commas
  separate the pairs.
- A line with an asterisk in column 1 is a comment.
- Only columns 1 to 72 of a line are read. Use columns 73 to 80 for sequence
  numbers or leave them blank.
- Keywords and values are not case sensitive, and OFTPD reads them as
  upper case. The one exception is the value of #cmd("HOST"), which keeps its
  case.
- A word without an equals sign is not a pair. OFTPD reports it as
  #cmd("X is not KEYWORD=VALUE") and skips it. A partner is not disabled by
  it.
- An unknown keyword or a value that is not valid is reported with message
  #cmd("OFTP010W"), which names the member and the line. In #cmd("OFTPPRM")
  OFTPD ignores the pair and the default stays. In #cmd("OFTPPTN") the
  partner that the pair belongs to is disabled\; the other partners are not
  affected.

== The Monitor Parameters: OFTPPRM <cfg-prm>

#idx("OFTPPRM")
OFTPD reads #cmd("OFTPPRM") once, when it starts. A change takes effect only
after OFTPD is stopped and started again. #cmd("REFRESH") does not read this
member. If the member cannot be opened, OFTPD ends with message
#cmd("OFTP011E"). If it has no #cmd("ODETTEID"), OFTPD ends with message
#cmd("OFTP012E").

#deflist(width: 1.35in,
  [#cmd("ODETTEID")], [*Required.* The Odette identification of this OFTPD,
    up to 25 characters. It is the identification OFTPD presents to its
    partners, unless the partner has an #cmd("OURID").],
  [#cmd("PORT")], [The TCP port on which OFTPD accepts calls. Default 3305,
    allowed 1 to 65535.],
  [#cmd("SVC")], [The number of the installation SVC through which the
    #cmd("OFTP") utility and the ISPF dialog reach OFTPD, from 200 to 255.
    There is no default. Without #cmd("SVC"), OFTPD writes
    #cmd("OFTP038P") and cannot be reached by the utility.],
  [#cmd("INPREFIX")], [The high-level qualifiers of the data sets that OFTPD
    creates for the files it receives, up to 35 characters. Default
    #cmd("OFTP.IN"). See @cfg-dsn.],
  [#cmd("TMPPREFIX")], [The high-level qualifiers of the data sets in which a
    file is first received, up to 35 characters. Default #cmd("OFTP.TMP").
    See @cfg-dsn.],
  [#cmd("MAXSESS")], [The most sessions that run at the same time, with all
    partners together. Default 4, allowed 1 to 16.],
  [#cmd("SDEB")], [The largest exchange buffer OFTPD offers in a session, in
    bytes. Default 4096, allowed 128 to 8192. A partner can lower it with
    its own #cmd("SDEB").],
  [#cmd("CREDIT")], [The largest credit OFTPD offers in a session, the number
    of exchange buffers a sender may send before it must wait for
    permission. Default 99, allowed 1 to 999. A partner can lower it with its
    own #cmd("CREDIT").],
  [#cmd("TIMEOUT")], [The time in seconds that a session waits for data from
    the partner. Default 60, allowed 5 to 3600.],
  [#cmd("TLRECL")], [The record length of the data sets that receive a text
    file. Default 255, allowed 20 to 32756. See @cfg-alloc.],
  [#cmd("UBLKSIZE")], [The block size of the data sets that receive a file of
    undefined format. Default 6144, allowed 512 to 32760.],
  [#cmd("UNIT")], [The unit name for the data sets OFTPD allocates for
    received files, up to 8 characters. Without it the allocation does not
    name a unit.],
  [#cmd("VOLSER")], [The volume serial for those data sets, up to 6
    characters. Without it the allocation does not name a volume.],
  [#cmd("RETRY")], [The base of the waiting times, in seconds, after a
    failed attempt. Default 300, allowed 10 to 86400. See
    @cfg-retry.],
  [#cmd("MAXRETRY")], [The number of attempts after which OFTPD gives up a
    file. Default 10, allowed 0 to 255. 0 means no limit. See
    @cfg-retry.],
  [#cmd("KEEPDAYS")], [The number of days a finished transfer stays in the
    spool. Default 7, allowed 0 to 9999. 0 keeps the transfers for ever.
    See @cfg-keep.],
  [#cmd("RELAY")], [#cmd("Y") (or #cmd("YES")) lets OFTPD act as a node, #cmd("N")
    (or #cmd("NO")) does not. Default #cmd("N"). See @cfg-relay.],
  [#cmd("ADMIN")], [The user ID of an administrator, 1 to 8 characters. Repeat
    the keyword for each administrator, up to 8. A duplicate is kept once.
    See @cfg-admin.],
)

=== Retries <cfg-retry>

#idx("RETRY")#idx("MAXRETRY")
When an attempt to send a file fails, OFTPD waits before it tries again.
The wait is #cmd("RETRY") seconds multiplied by the number of attempts made
so far, and never more than one day. After #cmd("MAXRETRY") attempts OFTPD
gives up:

- A file of this system is held (message #cmd("OFTP215W")). An operator or a
  user releases it when the cause is removed.
- A file that OFTPD was relaying is rejected, and the sender receives a
  negative end-to-end response (message #cmd("OFTP242W")).

#cmd("RETRY") is also the pause before OFTPD calls a partner again after a
call that failed to connect or ended in an error. After a partner has
refused a call with the reason code 08 (the partner has no free session),
OFTPD waits for a shorter time of its own (message #cmd("OFTP218P")).

=== Housekeeping: KEEPDAYS <cfg-keep>

#idx("KEEPDAYS")#idx("housekeeping")
Once a day, the first time at the first tick after OFTPD starts, OFTPD
deletes the finished transfers from the spool whose last change is older than
#cmd("KEEPDAYS") days. A transfer is finished when its state is
#cmd("DELIVERED"), #cmd("CONFIRMED"), #cmd("REJECTED") or #cmd("FAILED"). The
operator command #cmd("F OFTPD,PURGE") and the utility command #cmd("PURGE")
do the same on request. Only the spool records are deleted. The data sets of
received files and the files that were sent are never deleted by OFTPD.

OFTPD writes message #cmd("OFTP235I") when the daily housekeeping has deleted
something, and for every request. It writes #cmd("OFTP236E") if the spool
cannot be accessed.

=== Acting as a Node: RELAY <cfg-relay>

#idx("RELAY")#idx("node")
Without #cmd("RELAY=Y"), OFTPD accepts only files that are addressed to
itself. A file for any other Odette ID is refused (message #cmd("OFTP223W")).

With #cmd("RELAY=Y"), OFTPD also takes files for the Odette IDs of its
partners, whether direct or reached through another partner (see
#cmd("VIA")), and passes them on unchanged. The end-to-end response of the
final receiver is passed back the same way.

=== Administrators: ADMIN <cfg-admin>

#idx("ADMIN")#idx("authority")
An #cmd("OFTP") utility user who is not an administrator sees and changes
only their own transfers, and #cmd("LIST") shows only these. An administrator
sees and changes every transfer, including the received files, which have no
owner, and may use the #cmd("CALL"), #cmd("REFRESH") and #cmd("PURGE")
requests. Only an administrator may add, change, enable, disable or delete a
partner with the #cmd("PARTNER") command or in the ISPF dialog\; showing and
listing partners is open to every user. Without any #cmd("ADMIN") keyword,
these requests are refused with the answer #cmd("507"). For #cmd("CALL"),
#cmd("REFRESH") and #cmd("PURGE") the text is #cmd("needs ADMIN= in
OFTPPRM").

== The Partner Table: OFTPPTN <cfg-ptn>

#idx("OFTPPTN")#idx("partner table")
Each partner is an entry that starts with #cmd("PARTNER=") and goes on to the
next #cmd("PARTNER=") or to the end of the member. A keyword that comes before
the first #cmd("PARTNER=") is reported. The table holds at most 32 partners.
The partners that are kept by command (@cfg-cmd) use places of the same table.

#note[After 32 partners, OFTPD refuses a further #cmd("PARTNER=") with the
report #cmd("more than 32 partners"), but it still reads the keywords that
follow, and applies them to the 32nd partner. Do not put more than 32
partners in the member, and leave room for the partners that you keep by
command.]

OFTPD reads the member when it starts, and again at each #cmd("REFRESH")
(see @cfg-refresh). After the read, OFTPD disables a partner that has no
#cmd("ODETTEID"), or whose name or #cmd("ODETTEID") is used by an earlier
partner, and reports it with message #cmd("OFTP010W").

#deflist(width: 1.35in,
  [#cmd("PARTNER")], [*Required, and it starts the entry.* The name of the
    partner, 1 to 8 characters. The names are unique. The commands of the
    operator and the utility use this name.],
  [#cmd("ODETTEID")], [*Required.* The Odette identification of the partner,
    up to 25 characters. It must be unique in the table. A caller is
    recognized by it.],
  [#cmd("PASSWORD")], [The password the partner presents to OFTPD, up to 8
    characters. Default: none.],
  [#cmd("OURPASSWORD")], [The password OFTPD presents to the partner, up to 8
    characters. Default: none.],
  [#cmd("OURID")], [The Odette identification that OFTPD uses towards this
    partner. Default: the #cmd("ODETTEID") from #cmd("OFTPPRM"). A file for
    this identification is accepted from this partner, as well as one for
    the #cmd("ODETTEID") of #cmd("OFTPPRM").],
  [#cmd("HOST")], [The host name or the dotted address under which OFTPD
    calls the partner, up to 63 characters. A dotted address is used as it
    is\; anything else is looked up as a name. The case is kept. Without a
    host, OFTPD never calls the partner, and only answers its calls.],
  [#cmd("PORT")], [The TCP port of the partner. Default 3305, allowed 1 to
    65535.],
  [#cmd("LEVEL")], [The highest protocol level OFTPD uses with this partner:
    #cmd("5") for OFTP 2.0 (the default), #cmd("4") for 1.4, #cmd("2") for 1.3
    and #cmd("1") for 1.2. Level 1 is spoken as 1.3. The lower of the level
    of the partner and this one is used in the session. A partner that
    announces level 3 is treated as 1.3. No other values are allowed.],
  [#cmd("SDEB")], [The largest exchange buffer used with this partner. Default:
    the #cmd("SDEB") of #cmd("OFTPPRM"). Allowed 128 to 8192. The buffer in
    use is the smaller of the sizes OFTPD and the partner offer. At level 5
    it must be at least 165 bytes, otherwise the session ends with the
    reason code 10.],
  [#cmd("CREDIT")], [The largest credit used with this partner. Default: the
    #cmd("CREDIT") of #cmd("OFTPPRM"). Allowed 1 to 999. The credit in use is
    the smaller of the two values offered.],
  [#cmd("MODE")], [What OFTPD does with this partner, seen from this side:
    #cmd("S") only sends, #cmd("R") only receives, #cmd("B") does both
    (the default). A call that does not fit the mode ends with the reason
    code 10. In mode #cmd("R") a session sends no files, and in mode
    #cmd("S") it refuses the files the partner offers.],
  [#cmd("COMPRESS")], [#cmd("Y") offers compression of the exchange buffers.
    Default #cmd("N"). It is used only if the partner asks for it too. When
    OFTPD calls a partner and the partner asks for compression that OFTPD
    does not offer, the session ends with the reason code 10.],
  [#cmd("ENABLED")], [#cmd("N") refuses the partner and never calls it. The
    entry stays in the table. Default #cmd("Y").],
  [#cmd("CALL")], [#cmd("N") means OFTPD never calls the partner, and only
    answers its calls. Default #cmd("Y"). The same is true if the partner has
    no #cmd("HOST"), or a #cmd("VIA").],
  [#cmd("ADDRESS")], [The dotted address from which the partner calls, up to
    15 characters. A call from another address is refused (message
    #cmd("OFTP201W")). Default: any address.],
  [#cmd("XLATE")], [#cmd("Y") translates the data of files of fixed, variable
    and undefined format between EBCDIC and ASCII. Text files are always
    translated. Default #cmd("N").],
  [#cmd("VIA")], [The name of another partner, up to 8 characters, through
    which this partner is reached. See @cfg-via.],
  [#cmd("SESSIONS")], [The most sessions that run with this partner at the
    same time, from 1 to 16 and not above #cmd("MAXSESS"). Default 1. A
    call from a partner that has reached the limit is refused with the reason
    code 08 (message #cmd("OFTP202W")). While work is waiting that no session
    holds, OFTPD opens further sessions to the partner, up to the limit and
    up to #cmd("MAXSESS").],
)

=== Partners Reached Through Another Partner: VIA <cfg-via>

#idx("VIA")
A partner with #cmd("VIA=name") has no host, port or passwords of its own
and is never called, nor accepted as a caller. OFTPD sends the files for it
in a session with the partner #cmd("name"), addressed to the Odette ID of the
final partner. #cmd("name") must be a direct partner that is enabled. Only
one level is allowed: if #cmd("name") is not an enabled direct partner,
OFTPD disables the partner with the #cmd("VIA").

== Partners Kept by Command <cfg-cmd>

#idx("partner table", "kept by command")#idx("PARTNER")
The partner table has two sources, which are one table in the running OFTPD.

- The member #cmd("OFTPPTN") is the seed. OFTPD reads it when it starts and at
  each #cmd("REFRESH").
- A partner that an administrator adds or changes with the #cmd("PARTNER")
  command (see @cmd-partner) or in the ISPF dialog is kept by command. OFTPD
  stores it in the spool, one record for each partner, and reads these records
  at each start.

When both sources have a partner of one name, the spool wins. The entry of
#cmd("OFTPPTN") is not used for as long as the record exists. This is how a
partner of #cmd("OFTPPTN") is changed by command\; the first change makes a
record, which has the whole entry. The command #cmd("PARTNER DELETE") deletes
the record, and the entry of #cmd("OFTPPTN") is in use again. A partner that
is only in #cmd("OFTPPTN") cannot be deleted by command.

At the start OFTPD reads #cmd("OFTPPTN") first (message #cmd("OFTP014P")),
then opens the spool and reads the records (message #cmd("OFTP018I")). If it
cannot read the records to the end, it does not start (message
#cmd("OFTP019E")), because a partner that was disabled or changed by command
would come back as #cmd("OFTPPTN") has it. A record that OFTPD cannot use is
ignored, with the message #cmd("OFTP018W"), and stays in the spool.

The record holds the entry in the form of a line of #cmd("OFTPPTN"), without
the values that are the defaults. A partner whose #cmd("OURID"), #cmd("SDEB")
or #cmd("CREDIT") is the same as the setting of #cmd("OFTPPRM") when it is
stored therefore follows #cmd("OFTPPRM") when that setting changes, at the next
start. The passwords are stored in clear text. Protect the spool like
#cmd("OFTPPTN") and its backups.

Two enabled partners cannot have one Odette ID. If the sources give two, OFTPD
disables one of them and writes #cmd("OFTP018W")\: the one that is kept by
command stays, and of two of one source the first. A change by command is
checked first, and is refused when it would make the second.

A partner takes a place in the table of 32, whichever source it comes from.
When the table is full, #cmd("PARTNER ADD") is refused with the code 514.

== Refreshing the Partner Table <cfg-refresh>

#idx("REFRESH")
The operator command #cmd("F OFTPD,REFRESH") reads #cmd("OFTPPTN") again\; so
does the #cmd("REFRESH") request of an administrator in the ISPF dialog. It
does not read #cmd("OFTPPRM"), and it reads the member only\: the partners
that are kept by command are not read again (see @cfg-cmd). OFTPD reports the
result with message #cmd("OFTP016I").

- A partner that is new is added.
- A partner whose entry has changed is replaced, and OFTPD calls it again at
  once.
- A partner that is kept by command stays as it is. If #cmd("OFTPPTN") has an
  entry of the same name, the entry is not used.
- A partner that is no longer in the member is taken out of the table, and its
  place is free again. If it has a session, OFTPD is calling it, or a transfer
  in the spool names it, it stays in the table, disabled, until a later
  refresh finds it free of these.

The table holds 32 places, so it can be full even if fewer than 32 partners
are enabled (message #cmd("OFTP015W")). Delete a partner that is kept by
command, or take an entry out of the member, and use #cmd("REFRESH") again\;
a partner of the member that was skipped is read again. If the member cannot
be opened, OFTPD writes #cmd("OFTP017E") and goes on with the table it has.

== Names of Received Data Sets <cfg-dsn>

#idx("received files", "data set names")
A file that OFTPD receives is stored in a new cataloged data set. OFTPD
builds its name from #cmd("INPREFIX") and the virtual file name of the
transfer, one qualifier at a time:

- Letters are changed to upper case. Letters, digits, and the characters
  #cmd("@"), #cmd("#"), #cmd("$") and #cmd("-") are kept\; any other character
  is dropped.
- A qualifier is cut to 8 characters. If it does not start with a letter or
  one of #cmd("@"), #cmd("#") and #cmd("$"), an #cmd("X") is put in front of it.
- Empty qualifiers are left out. A name that has none left becomes
  #cmd("DATA").
- Qualifiers are dropped from the end of the name until #cmd("INPREFIX") and
  the name fit into 44 characters.

@cfg-dsn-tab gives examples.

#tab(caption: [Virtual file names and the data set names built from them])[
  #table(columns: (2.0in, 1fr),
    [Virtual file name], [Name after #cmd("INPREFIX")],
    [#cmd("NOT-CATEGORIZED")], [#cmd("NOT-CATE")],
    [#cmd("report-2026.csv")], [#cmd("REPORT-2.CSV")],
    [#cmd("2026.LOG")], [#cmd("X2026.LOG")],
    [#cmd("a..b")], [#cmd("A.B")],
  )
] <cfg-dsn-tab>

If the name is already used, OFTPD adds the transfer key (for example
#cmd("T0000012")) as an extra qualifier.

The file is first received into a data set named #cmd("TMPPREFIX") followed by
the transfer key, and is renamed to the final name after it has been closed.
If the rename fails, OFTPD writes message #cmd("OFTP221E"). The state of the
transfer is #cmd("RECEIVED") until the end-to-end response has been sent, and
#cmd("CONFIRMED") after it.

== Allocation of Received Data Sets <cfg-alloc>

#idx("received files", "allocation")
The format of the file in the virtual file description decides the
attributes of the data set, as @cfg-alloc-tab shows.

#tab(caption: [Attributes of the data set for a received file])[
  #table(columns: (1.2in, 1fr),
    [Format], [Data set],
    [F (fixed)], [#cmd("RECFM=FB"), record length from the file description,
      block size a multiple of it near 6144.],
    [V (variable)], [#cmd("RECFM=VB"), record length of the file description
      plus 4.],
    [U (undefined)], [#cmd("RECFM=U"), block size #cmd("UBLKSIZE").],
    [T (text)], [#cmd("RECFM=VB"), record length #cmd("TLRECL"), block size
      the larger of #cmd("TLRECL") plus 4 and 6144. A line longer than
      #cmd("TLRECL") minus 4 characters is an error.],
  )
] <cfg-alloc-tab>

#cmd("UNIT") and #cmd("VOLSER") are passed to the allocation if they are set.
The space is in tracks, with a primary and a secondary quantity derived from
the size in kilobytes that the sender announces: 15 and 15 if the size is 0, otherwise a
primary of the size divided by 15 plus 1, but at least 5, and a secondary of
half the primary plus 1. The disposition is #cmd("NEW,CATLG,DELETE"). A name
that is already cataloged is refused.

== A Job After Receipt <cfg-job>

#idx("JOB")#idx("job after receipt")#idx("internal reader")#idx("symbols", "in a job")
A receive rule can carry the keyword #cmd("JOB=pds(member)"). OFTPD then reads
that member once the file is under its final name, puts the symbols of
@cfg-job-sym-tab into it, and gives it to its own internal reader. The job is
submitted before OFTPD owes the receipt (EERP) to the partner, but the receipt
does not wait for the job. It says that the file has arrived, not that anything
was done with it.

#cmd("JOB=") names a partitioned data set and a member, for example
#cmd("JOB=ACCT.JCL(ORDERS)"). The data set name is up to 44 characters and
must be a valid data set name\; the member has 1 to 8 characters, letters,
digits and the national characters #cmd("@"), #cmd("#") and #cmd("$"), and does
not start with a digit. Neither part may hold a symbol. A rule with a
#cmd("JOB=") that is not of this form is in error like any other error in the
rule member: the member is not used at all. The data set and the member are
read when a file arrives, so the member can be changed at any time without a
refresh\; the #cmd("JOB=") keyword itself is read with the rule, and
#cmd("F OFTPD,REFRESH") reads it again.

=== The Symbols <cfg-job-sym>

OFTPD replaces the symbols of @cfg-job-sym-tab in every record of the member, also
in a comment or in data that follows a #cmd("DD *") statement.

#tab(caption: [Symbols in the member of a job])[
  #table(columns: (1.1in, 1fr),
    [Symbol], [Value],
    [#cmd("?PFILE")], [The data set the file was received as. It is the name that
      OFTPD built from the #cmd("DSN=") of the rule (or from #cmd("INPREFIX")),
      so a #cmd("DSN=") with #cmd("%VFN") in it makes it carry text of the
      partner.],
    [#cmd("?VFILE")], [The virtual file name, as the partner sent it.],
    [#cmd("?ID")], [The name of the partner.],
    [#cmd("?KEY")], [The spool key of the transfer: #cmd("T") and 7 digits.],
  )
] <cfg-job-sym-tab>

The names are in upper case. A name ends at the first character that is neither
a letter nor a digit. #cmd("?ID.X") and #cmd("?ID-?KEY") hold the symbols
#cmd("?ID") and #cmd("?KEY"), but #cmd("?IDX"), #cmd("?PFILE1") and
#cmd("?KEYS") are other names and stay as they are. A #cmd("?") that starts
anything else stays as it is too, so a member is not damaged by a question mark
in its text.

=== The Width of a Card <cfg-job-width>

The job goes to the internal reader as 80-column cards. In a record that
starts with #cmd("//") the text of the statement ends in column 71: column 72
tells JES whether the statement is continued, and columns 73 to 80 are the
sequence number. Characters from column 72 on stay where they are, and the
symbols make only the text grow. Any other record has 80 columns. Blanks at the
end of a record do not count.

A record that grows past its columns after the symbols went in is *not* cut.
The job is not submitted instead (see below).

=== When a Job Is Not Submitted <cfg-job-not>

OFTPD reads the member twice. The first pass checks every record and opens no
reader\; the second pass sends the cards. A member that fails a check therefore
never leaves a half-written job in JES. No job is submitted when:

- the data set or the member cannot be opened, or it has no records\;
- a record is longer than 80 columns (#cmd("RECORD PAST COLUMN 80"))\;
- a record grows past its columns after the symbols went in
  (#cmd("LINE TOO LONG AFTER SUBSTITUTION"))\;
- #cmd("?VFILE") is used and the virtual file name holds a character that is
  not allowed (#cmd("?VFILE NOT ALLOWED IN JCL")). These are letters, digits and
  the characters #cmd("."), #cmd("-"), #cmd("_"), #cmd("@"), #cmd("#") and
  #cmd("$"). A quote, a blank, a comma or an ampersand is not\;
- the record format is undefined (#cmd("RECFM=U")) or the record length is not
  positive (#cmd("RECORD FORMAT NOT SUPPORTED"))\;
- there is no storage for the record, or the internal reader cannot be opened,
  written or closed.

The file is kept and acknowledged all the same. The receipt tells the partner
that the file has arrived\; whether a job came of it concerns only you.

=== What OFTPD Reports <cfg-job-report>

#idx("OFTP246P")#idx("OFTP247W")
A submitted job is reported by #cmd("OFTP246P") in the log, with the job
number. A job that is not submitted is reported by #cmd("OFTP247W") on the
console and in the log, with the reason and, for a fault in a record, the number
of the record. #cmd("OFTP246P") comes before the #cmd("OFTP222P") of the same
file.

The result is also kept in the record of the transfer, and #cmd("OFTP SHOW key")
prints it as one line: #cmd("job submitted JOBnnnnn (JES job id)"), or
#cmd("job not submitted:") followed by the reason. #cmd("LIST") does not show it.
A transfer of a rule without #cmd("JOB=") has no such line.

#note[*What "submitted" proves.* The line says that JES took the job. It does not say
that the member was read to its end: the routine that reads the member does not
tell a read error from the end of the member. A job that fails when it runs fails
in the JES log of that job and not in OFTPD.]

=== Who Decides What the Job Does <cfg-job-safe>

#idx("security", "job after receipt")#idx("?VFILE")
The virtual file name is chosen by the partner and ends up in the JCL. The
character check of @cfg-job-not protects the card, not its meaning:
#cmd("SYS1.PARMLIB"), #cmd("IEFBR14") and #cmd("A.B.C") pass it, and each is a
valid name in JCL. The name of the partner's file must therefore never decide
by itself what the job touches or runs.

- Use #cmd("?VFILE") only behind a fixed high-level qualifier that the partner
  cannot leave, as in #cmd("DSN=ACCT.IN.?VFILE").
- Never use it as a whole name (#cmd("DSN=?VFILE")), as #cmd("PGM=?VFILE"),
  inside #cmd("PARM="), on the job card, or in any operand that names an
  identity: #cmd("USER="), #cmd("PASSWORD=") and #cmd("GROUP=").
- Keep the #cmd("VFN=") pattern of the rule as narrow as the application allows:
  #cmd("VFN=INV?????.DATA"), not #cmd("VFN=*").

A partner can have one job queued for every file it may send under a rule that
matches, so #cmd("VFN=") also limits how many. A partner name that is used with
#cmd("?ID") should be a plain name: OFTPD does not restrict the characters of a
name in #cmd("OFTPPTN"), and it does not check #cmd("?ID") as it checks
#cmd("?VFILE").

=== Whose Job It Is <cfg-job-whose>

The job goes through the internal reader of the OFTPD started task, but it does
not run as that task. OFTPD sets no #cmd("USER="), and with RAKF a job without
one runs as the default user of RAKF, #cmd("PROD"). It can do what that user
may do under your RAKF profiles, and that can be a lot. What the default user
may do on your system is not known to OFTPD and not stated in this book. Before
you write #cmd("JOB="), look it up in your profiles and give that user only the
access that the jobs need.

Apart from the symbols, OFTPD passes the cards of the member on as they are,
so the job card may carry a #cmd("USER=") and a #cmd("PASSWORD="). The password is then plain text in the
member. OFTPD checks nothing about what the job does: the member is named in the
rule member, so whoever may change the rule member or the job member decides
what runs. Protect both, and the library that holds the job members, as you
protect the library of the other OFTPD members, #cmd("SYS2.PARMLIB") by default.

Not part of the job after receipt: a job after a send, a job when a send fails,
and a job for a file that was refused.
