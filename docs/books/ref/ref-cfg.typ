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
      OFTPD starts, and again by the #cmd("REFRESH") command.],
    [#cmd("OFTPSPL")], [The spool, a VSAM data set that holds one record for
      each transfer. Allocate it with #cmd("DISP=OLD").],
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
requests. Without any #cmd("ADMIN") keyword, these three requests are refused
with the answer #cmd("507 needs ADMIN= in OFTPPRM").

== The Partner Table: OFTPPTN <cfg-ptn>

#idx("OFTPPTN")#idx("partner table")
Each partner is an entry that starts with #cmd("PARTNER=") and goes on to the
next #cmd("PARTNER=") or to the end of the member. A keyword that comes before
the first #cmd("PARTNER=") is reported. The table holds at most 32 partners.

#note[After 32 partners, OFTPD refuses a further #cmd("PARTNER=") with the
report #cmd("more than 32 partners"), but it still reads the keywords that
follow, and applies them to the 32nd partner. Do not put more than 32
partners in the member.]

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

== Refreshing the Partner Table <cfg-refresh>

#idx("REFRESH")
The operator command #cmd("F OFTPD,REFRESH") reads #cmd("OFTPPTN") again\; so
does the #cmd("REFRESH") request of an administrator in the ISPF dialog. It
does not read #cmd("OFTPPRM"). OFTPD reports the result with message
#cmd("OFTP016I").

- A partner that is new is added.
- A partner whose entry has changed is replaced, and OFTPD calls it again at
  once.
- A partner that is no longer in the member is disabled. Its slot in the
  table stays used until OFTPD is started again.

The table holds 32 slots, so after several changes it can be full even if
fewer than 32 partners are enabled (message #cmd("OFTP015W")). Restart OFTPD
to free the slots. If the member cannot be opened, OFTPD writes
#cmd("OFTP017E") and goes on with the table it has.

== Names of Received Data Sets <cfg-dsn>

#idx("received files", "data set names")
A file that OFTPD receives is stored in a new cataloged data set. OFTPD
builds its name from #cmd("INPREFIX") and the virtual file name of the
transfer, one qualifier at a time:

- Letters are changed to upper case. Letters, digits, and the characters
  #cmd("@"), #cmd("\#"), #cmd("$") and #cmd("-") are kept\; any other character
  is dropped.
- A qualifier is cut to 8 characters. If it does not start with a letter or
  one of #cmd("@"), #cmd("\#") and #cmd("$"), an #cmd("X") is put in front of it.
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
