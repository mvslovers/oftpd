#import "../bookmaster/bookmaster.typ": *

= Introducing OFTPD <ug-intro>

#idx("OFTPD")#idx("OFTP")
OFTPD (oftpd) is an OFTP server and client for MVS 3.8j. OFTP, the ODETTE File
Transfer Protocol, is a protocol for exchanging files between computers, much
used in European industry. With OFTPD an MVS system sends files to a partner
and receives files from one over TCP/IP. The partner can be another MVS system
that runs OFTPD, or a PC or a server that speaks OFTP.

OFTPD is meant for hobbyist and test systems, not for production use.

== What OFTPD Consists Of <intro-parts>

OFTPD has four parts that you meet.

#deflist(width: 1.5in,
  [The monitor], [The started task #cmd("OFTPD"). It listens for the calls of
    partners, calls partners when files wait for them, and keeps track of
    every file. It is the one part that sends and receives. The operator
    starts and stops it, and controls it from the console with the
    #cmd("F OFTPD") and #cmd("P OFTPD") commands.],
  [The spool], [A VSAM data set that holds one record for each transfer, with
    its state and what is needed to resume it, and one for each partner that an
    administrator keeps by command. Only the monitor uses it. The other parts
    ask the monitor.],
  [The partner table], [A list of the partners: their Odette IDs, their
    passwords, and where to call them. It has two sources, the member
    #cmd("OFTPPTN"), which the monitor reads when it starts and again on
    request, and the partners that an administrator adds and changes with the
    #cmd("OFTP") utility or in the dialog.],
  [The utility and the dialog], [The command #cmd("OFTP") for TSO and batch,
    and the ISPF dialog #cmd("OFTPI"). Users queue files for sending and look
    at their transfers with them. They do not send anything themselves\; they
    pass the request to the monitor.],
)

The settings of the monitor are in a second member, #cmd("OFTPPRM"). Both
members are described in the chapter “Configuration and the Partner Table” of
the _OFTPD Reference_.

The utility and the dialog reach the monitor through an SVC that OFTPD
installs, and the monitor learns the user ID of the caller from the system
itself. A user therefore sees and changes the transfers that the user queued.
An administrator, named in the settings, sees and changes all of them. The
chapter “The OFTP Command” of the Reference gives the rules.

== What OFTPD Speaks <intro-levels>

#idx("protocol levels")#idx("downgrade")
OFTPD speaks OFTP 2.0, and falls back to OFTP 1.4 or 1.3 when the partner
cannot do better. In each session the two sides use the highest level that
both allow. The highest level that OFTPD uses with a partner is set for each
partner with the keyword #cmd("LEVEL").

OFTPD does not have the security features of OFTP 2.0. There is no TLS, so the
connection is plain TCP. There is no authentication by certificate, and no
signing or encryption of files. A file that is offered encrypted, signed or
compressed in the manner of OFTP 2.0 is refused. Passwords, as in OFTP 1.x,
are used. If a connection has to be protected, protect it outside OFTPD, for
example with a gateway in front of the port.

Compression of the exchange buffers, which belongs to the exchange protocol, is
used when both sides ask for it (the keyword #cmd("COMPRESS") of a partner).

== The Way of a File <intro-flow>

A file travels in one of two directions.

*Sending.* A user runs #cmd("OFTP SEND") or uses the dialog. The monitor checks
that the user may read the data set, stores a transfer in the spool, and gives
it a key such as #cmd("T0000012"). The transfer is queued. When its time has
come, the monitor calls the partner, opens a session, and sends the file. When
the partner has the file, the transfer is sent. When the receipt of the partner
arrives, it is delivered. If the call or the transfer fails, the monitor waits
and tries again.

*Receiving.* A partner calls OFTPD and offers a file. OFTPD checks the Odette ID
and the password of the partner, creates a data set whose name is built from
the virtual file name, and receives the file into it. When the file is
complete, OFTPD sends the receipt to the partner, and the transfer is
confirmed.

In both directions either side can make the call. In a session each side
speaks in turn, so a call from a partner also collects the files that wait for
that partner.

== What Do You Want to Do? <intro-tasks>

#tab(caption: [Where to find what you want to do])[
  #table(columns: (2.6in, 1fr),
    [You want to], [Read],
    [Install OFTPD on a system], [@ug-install],
    [Start and stop the monitor, read its log], [@ug-monitor],
    [Set up a partner and check that it answers], [@ug-partners],
    [Send a data set to a partner and see that it arrived], [@ug-transfer],
    [See what a partner sent, and where it went], [@ug-receive],
    [Name the data sets of received files, or start a job when a file has
      arrived], [@ug-rules],
    [Do all this from panels], [@ug-dialog],
    [Hold sends, retry them, clean up the spool, find out why a transfer does
      not move], [@ug-ops],
    [Take OFTPD off the system], [@ug-remove],
  )
]

== The Examples in This Book <intro-examples>

Every session in this book was run on a system of its own and is shown as it
came out. Two monitors ran on one MVS 3.8j system (TK5), so that OFTPD had a
partner to talk to: the first is the system of the examples, with the Odette ID
#cmd("O0000000000MVS1") and the port 3305. The second plays the partner, called
#cmd("MAC1"), with the Odette ID #cmd("O0000000000MAC1") and the port 3315.
The user is #cmd("HERC01"), who is the administrator of both. Both monitors
reach the other at #cmd("127.0.0.1"), the machine on which Hercules runs. A
partner of your own has another host and port\; where the book shows a partner
at another address, it uses #cmd("192.0.2.20"), an address that is set aside
for examples.

The log lines show the version of the build, #cmd("1.0.0-dev"), in the
message #cmd("OFTP000I"). Yours shows the version you installed. The lines of
the log begin with the date and the time\; the lines of one run are cut out of
the whole log with nothing changed in them.

== Terms Used in This Book <intro-names>

#idx("Odette ID")#idx("virtual file name")
The Odette ID is the name by which a system is known in OFTP, a text of up to
25 characters that you agree with the partner. Each system has one, and OFTPD
recognizes its partners by theirs.

The virtual file name is the name under which a file is known between the
partners, whatever the names of the data sets on either side. It is up to 26
characters. The sender chooses it, and the receiver builds the name of its
data set from it.

The glossary at the end of the book explains the other terms.
