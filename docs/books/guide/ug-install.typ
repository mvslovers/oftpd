#import "../bookmaster/bookmaster.typ": *

= Installing OFTPD <ug-install>

#idx("installation")
This chapter takes you from the files of a release to a monitor that is ready
to start. The steps are those of the file #cmd("docs/installation.md") in the
oftpd repository, which also comes with every release\; the screens in this
chapter are the output of a real installation of the release described in
@intro-examples, on a TK5 system. Where your system differs from TK5, the text
says so.

When you are done, you have

- the load modules in #cmd("OFTPD.LINKLIB"), installed by SMP;
- the sample library #cmd("OFTPD.SAMPLIB") and the panel library
  #cmd("OFTPD.ISPPLIB");
- an authorized load library;
- a spool;
- your own copies of the settings, the partner table, the receive rules and the
  procedure.

Starting the monitor is the next chapter, @ug-monitor.

== What You Need <inst-needs>

- MVS 3.8j on Hercules, with TCP/IP for MVS programs (the DYN75 instruction of
  Hercules), and SMP 4 with the procedures #cmd("SMPREC") and #cmd("SMPAPP").
- A user ID that may create the data sets and submit jobs.
- For the dialog, ISPF and a screen of more than 24 rows (@dlg-needs).
- An SVC number for the utility and the dialog. The sample settings use 237.
  OFTPD checks at start that the entry is free and takes it only then (see the
  keyword #cmd("SVC") in the Reference).
- A library that can be made APF-authorized (@inst-apf).
- Space for a VSAM cluster, the spool.

The sockets that OFTPD uses are those of the Hercules host. The address
#cmd("127.0.0.1") is the machine on which Hercules runs, not MVS, and the port
of OFTPD is opened there.

== The Files of a Release <inst-files>

A release has five files. The name of each has the version in it.

#tab(caption: [The files of a release])[
  #table(columns: (2.3in, 1fr),
    [File], [What it is],
    [#cmd("oftpd-<v>-load.xmit")], [The load modules #cmd("OFTPD"),
      #cmd("OFTP"), #cmd("OFTPI") and #cmd("OFTPSVC").],
    [#cmd("oftpd-<v>-samplib.xmit")], [#cmd("OFTPD.SAMPLIB"): the procedure,
      the sample settings and jobs.],
    [#cmd("oftpd-<v>-ispplib.xmit")], [#cmd("OFTPD.ISPPLIB"): the panels of the
      dialog.],
    [#cmd("oftpd-<v>-alloc.jcl")], [Allocates the SMP libraries once.],
    [#cmd("oftpd-<v>-inst.jcl")], [Receives the three files and installs the
      SYSMOD.],
  )
]

The SYSMOD is a function with the FMID #cmd("TOFT") and the digits of the
version\; release 1.0.0 is #cmd("TOFT100"). SMP spends an FMID once. Before you
install, check that it is free.

== Check That the FMID Is Free <inst-fmid>

Run SMP with #cmd("LIST") against both zones.

#screen(```
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 LIST CDS SYSMOD(TOFT100) .
 LIST ACDS SYSMOD(TOFT100) .
/*
```)

The job answers like this on a system that has never had OFTPD.

#screen(raw(read("../ex/ug-install/fmid-free.txt")))

Return code 4 and nothing listed mean that the FMID is free. If anything is
listed, even an entry with only #cmd("TYPE = FUNCTION") and a #cmd("DELBY"),
the FMID is taken\: stop and ask. If it is the leftover of an OFTPD that you
have removed, @ug-remove shows how to free it.

== Upload the Files and Install <inst-run>

+ Upload the three #cmd(".xmit") files #emph[in binary] into sequential data
  sets with #cmd("RECFM=FB") and #cmd("LRECL=80"), for example
  #cmd("HERC01.OFTPD.LOAD.XMIT"), #cmd("HERC01.OFTPD.SAMPLIB.XMIT") and
  #cmd("HERC01.OFTPD.ISPPLIB.XMIT"). Use IND\$FILE, FTP in binary mode, or the
  mvsMF REST API.
+ Submit #cmd("oftpd-<v>-alloc.jcl") once. It allocates #cmd("OFTPD.LINKLIB")
  and #cmd("OFTPD.AOFTPLOD"). Edit #cmd("UNIT=") or add a #cmd("VOL=SER=") if
  #cmd("SYSDA") is not what you want.
+ In #cmd("oftpd-<v>-inst.jcl") replace the three names that start with
  #cmd("CHANGE.ME") by the names of step 1, and submit it.

On MVS/CE, TSO #cmd("RECEIVE") needs a volume\; add #cmd("VOLUME('PUB000')"),
or another volume of yours, to each of the three #cmd("RECEIVE") commands. TK5
does not need it.

The job receives the three files, then receives, applies and accepts the
SYSMOD. This is what the first #cmd("RECEIVE") says.

#screen(raw(read("../ex/ug-install/receive-load.txt")))

The job deletes and receives #cmd("OFTPD.SAMPLIB") and #cmd("OFTPD.ISPPLIB")
again each time it runs. Copy members out of them, and never edit them in
place\; the copies that you make in the following sections are not touched by a
new installation.

=== Check the Result <inst-check>

A return code of 0 says that SMP ran, not that it copied anything. Look in the
output of the step #cmd("APPLY") for one line #cmd("HMA2380 COPY SUCCESSFUL")
for each of the four modules, and for the status #cmd("APPLIED").

#screen(raw(read("../ex/ug-install/apply.txt")))

The step #cmd("ACCEPT") says the same for the distribution library, with the
status #cmd("ACCEPTED").

#screen(raw(read("../ex/ug-install/accept.txt")))

Then list the libraries from TSO. All four modules have to be in
#cmd("OFTPD.LINKLIB"), and the sample members and the panels in the other two.

#screen(raw(read("../ex/ug-install/members.txt")))

The listing shows the library that the next sections work with.
#cmd("OFTPD.LINKLIB") holds the modules\; #cmd("OFTPSVC") is the module of the
cross-memory service that the monitor loads, and nobody runs it.
#cmd("OFTPD.SAMPLIB") holds #cmd("OFTPD") (the procedure), #cmd("OFTPDJOB")
(the monitor as a batch job), #cmd("OFTPBAT") (the utility in batch and under
TSO), #cmd("OFTPSPL") (the job that defines the spool) and the three sample
members of the settings, the partner table and the receive rules.

== Authorize the Load Library <inst-apf>

#idx("APF")#idx("authorization")
The monitor is link-edited as authorized, and every library of its
#cmd("STEPLIB") has to be authorized. Add #cmd("OFTPD.LINKLIB") with its volume
to the member #cmd("IEAAPF00") of #cmd("SYS1.PARMLIB"), in the format of the
other entries of that member. Each line has a data set name and a volume, and
every line but the last ends in a comma. This is the end of the member on TK5
with the new entry added, for a library on the volume #cmd("TSO002") (see the
listing above).

#screen(```
 STF.LINKLIB TK5002,
 EXH.EXHLIB TK5002,
 EXH.ESPLIB TK5002,
 OFTPD.LINKLIB TSO002
```)

MVS 3.8j reads #cmd("IEAAPF00") only at IPL, so the entry works after the next
IPL. If the system has SVC 244 and RAKF permits it, the monitor can also ask
for authorization through the SVC, without the entry. The first lines of the log
say which way it went (@mon-log): #cmd("OFTP004P AUTHORIZED BY LIBRARY") or
#cmd("OFTP004P AUTHORIZED BY SVC"). The system of the examples in this guide
went the second way\; this guide does not show an IPL.

Without authorization the monitor starts, and the partners work, but the
cross-memory service is not started, and the #cmd("OFTP") utility and the
dialog cannot reach it. They answer #cmd("OFTPD not reached: not running, or without SVC=")
with return code 12, the same text for every cause\; @mon-start shows the
messages in the log that tell which one it is.

== Define the Spool <inst-spool>

OFTPD keeps every transfer in a VSAM key-sequenced data set, its spool. Copy
#cmd("OFTPD.SAMPLIB(OFTPSPL)"), edit the name of the cluster and a volume that
has space and belongs to your catalog, and submit it once. This is the
definition that the examples use.

#screen(raw(read("../ex/ug-install/spool.txt")))

A finished transfer, whether delivered, confirmed, refused or failed, stays in
the spool for #cmd("KEEPDAYS") days after its last change, 7 by default, and the
monitor deletes the older ones once a day. Size the spool for what arrives and
leaves in that time. Only spool entries are deleted, never data sets. A transfer
that is not finished (a send that waits for its receipt, a receive that owes
one) stays until its receipt is settled or you delete it.

The spool belongs to the monitor alone. The utility and the dialog never open
it\; they ask the monitor. It also holds the partners that you keep by command
(@ptn-cmd), #emph[with their passwords in clear text], as the partner member
does. Protect the cluster and every copy of it as you protect the partner
member.

== Settings, Partners and Receive Rules <inst-settings>

Copy the three sample members to your parameter library, and edit them. On TK5
the library is #cmd("SYS1.PARMLIB")\; TK5 has no #cmd("SYS2.PARMLIB"). The
procedure of the next section names it with #cmd("D=").

#screen(raw(read("../ex/ug-install/copy-members.txt")))

The first #cmd("COPY") is the three members. Each of them describes its
keywords at the top.

- #cmd("OFTPPRM0") holds the settings of this system: its Odette ID
  (#cmd("ODETTEID"), required), the port (#cmd("PORT"), 3305), the SVC for the
  utility and the dialog (#cmd("SVC")), the user IDs of the administrators
  (#cmd("ADMIN")), the number of sessions at once (#cmd("MAXSESS")) and the
  prefixes of the files that are received (#cmd("INPREFIX"),
  #cmd("TMPPREFIX")). The monitor reads it at start only\; a change needs a
  restart.
- #cmd("OFTPPTN0") holds the partners that the monitor knows at start.
  @ug-partners shows how to write them and how to keep them by command.
- #cmd("OFTPRCV0") holds the receive rules. The sample consists of comments
  only, which changes nothing. @ug-rules shows the rules.

The settings of the system in the examples of this guide are these. The user
HERC01 is the administrator, and the SVC number is 237.

#screen(raw(read("../ex/ug-monitor/settings-a.txt")))

The sample procedure points a DD at #cmd("OFTPRCV0"). The monitor does not need
the DD\; without it there are no rules, and the start says nothing about it. So
either copy #cmd("OFTPRCV0") to your library, as above, or delete the DD from
your copy of the procedure.

== The Procedure <inst-proc>

Copy #cmd("OFTPD.SAMPLIB(OFTPD)") to your procedure library, #cmd("SYS1.PROCLIB")
on TK5. The second #cmd("COPY") in the listing above did that. Its
#cmd("STEPLIB") already names #cmd("OFTPD.LINKLIB"). Its defaults are
#cmd("SYS2.PARMLIB") for the members, and #cmd("OFTP.SPOOL") for the spool\;
change them in the procedure or give yours when you start it (@mon-start).

The procedure and its parameters are shown in @mon-proc.

If the system protects data sets with RAKF, the user ID of the started task needs
update access to the spool and the authority to create data sets under
#cmd("INPREFIX") and #cmd("TMPPREFIX"), and under any other first qualifier that
a receive rule names.

== The Utility and the Dialog <inst-utility>

The utility #cmd("OFTP") runs from batch with #cmd("PGM=OFTP") and under TSO as
the command #cmd("OFTP"). In both cases the load library has to be found: in the
#cmd("STEPLIB") of the job, in the link list, or in the logon procedure.
@ug-transfer shows both uses. The sample member #cmd("OFTPBAT") has a job for
each.

The dialog needs the two libraries in the allocations of ISPF. @ug-dialog shows
how, with the entry for the menu.

== Upgrading <inst-upgrade>

#idx("upgrading")
*From 0.1.0.* This is not a drop-in. Release 0.1.0 was a prerelease, and no way
of upgrading from it has been tested. #cmd("TOFT100") deletes #cmd("TOFT010"), so
the SYSMOD installs over the prerelease, but it brings no configuration with it\:
set the system up again as in this chapter. Do not run the allocation job a
second time, since it allocates #cmd("OFTPD.LINKLIB") and
#cmd("OFTPD.AOFTPLOD") with #cmd("DISP=(NEW,CATLG,DELETE)"), and the prerelease
created both. Stop the monitor before you run the install job.

The TCP control port is gone. Remove the keyword #cmd("CONTROLPORT") from
#cmd("OFTPPRM0")\; OFTPD reports it as an unknown keyword with
#cmd("OFTP010W") and skips it. Add #cmd("SVC=237"), or another free number of the
range 200 to 255. Without it the utility and the dialog cannot reach the monitor.

*The procedure.* The sample procedure of release 1.0.0 has a DD #cmd("OFTPRCV")
for the receive rules (@ug-rules). If you take the new procedure over your old
one, copy #cmd("OFTPRCV0") to your library first, or delete the DD from your
copy\; otherwise the monitor starts without rules and says nothing about the
missing member.

== Removing <inst-remove>

#idx("removal")
To take OFTPD off a system, use the steps of @ug-remove. They free the FMID, so
that OFTPD can be installed again. Do not remove OFTPD to move to a newer
release: the release deletes its predecessor itself. #cmd("RESTORE") and
#cmd("REJECT") do not remove OFTPD, because the install job accepts it (see
@ug-remove-why).
