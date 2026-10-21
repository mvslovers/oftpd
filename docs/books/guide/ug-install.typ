#import "../bookmaster/bookmaster.typ": *

= Installing OFTPD <ug-install>

#idx("installation")
The steps of the installation are kept in one place, the file
#cmd("docs/installation.md") of the oftpd repository, and in the package that
comes with each release. This chapter says what an installation needs and what
it consists of, so that you know what to prepare. Follow that file for the
steps themselves.

== What You Need <inst-needs>

- MVS 3.8j on Hercules, with TCP/IP for MVS programs (the DYN75 instruction of
  Hercules), and SMP 4 for the installation.
- A user ID that may create the data sets and submit jobs.
- For the dialog, ISPF, and a screen of more than 24 rows.
- An SVC number for the utility and the dialog. The sample settings use 237.
  OFTPD checks at start that the entry is free and takes it only then (see the
  keyword #cmd("SVC") in the Reference).
- A library that can be made APF-authorized. The monitor asks for
  authorization when it starts. Without it the monitor still runs, but it
  cannot start the cross-memory service, so the utility and the dialog cannot
  reach it (messages #cmd("OFTP005W") and #cmd("OFTP035W")).
- A VSAM cluster for the spool.

The sockets that OFTPD uses are those of the Hercules host. The address
#cmd("127.0.0.1") is the machine on which Hercules runs, not MVS, and the port
of OFTPD is opened there.

== What Is Installed <inst-parts>

The installation puts the following on the system.

#deflist(width: 1.5in,
  [#cmd("OFTPD.LINKLIB")], [The load modules: the monitor #cmd("OFTPD"), the
    utility #cmd("OFTP"), and the dialog #cmd("OFTPI").],
  [#cmd("OFTPD.SAMPLIB")], [The sample procedure, the sample members
    #cmd("OFTPPRM0"), #cmd("OFTPPTN0") and #cmd("OFTPRCV0"), the job that defines the spool, and
    an example of the utility in batch.],
  [#cmd("OFTPD.ISPPLIB")], [The panels of the dialog.],
)

After the installation you do five things.

+ Make the load library APF-authorized.
+ Define the spool with the sample job.
+ Copy the sample members to your parameter library and edit them, as shown in
  @ug-partners. #cmd("OFTPRCV0") holds the receive rules and consists of
  comments only\; the procedure needs it even if you want no rules (see
  @xfer-rules).
+ Copy the sample procedure to your procedure library. It starts the monitor, as
  shown in @ug-monitor.
+ For the dialog, add its two libraries to the ISPF allocations.

For the utility in batch and under TSO, the load library has to be in the
STEPLIB of the job, or in the link list, or in the logon procedure. See
@ug-transfer.

== Upgrading <inst-upgrade>

#idx("upgrading")
The sample procedure of release 1.0.0 has a DD #cmd("OFTPRCV") for the receive
rules (@xfer-rules). If you take the new procedure over your old one, copy
#cmd("OFTPRCV0") to your parameter library first, or delete the DD from your
copy\; otherwise the monitor starts without rules and says nothing about the
missing member.
