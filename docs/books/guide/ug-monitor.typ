#import "../bookmaster/bookmaster.typ": *

= The Monitor <ug-monitor>

#idx("monitor")#idx("started task")
The monitor is the part of OFTPD that does the work: it listens for calls,
calls partners, sends and receives the files, and keeps the spool. It runs as
the started task #cmd("OFTPD"). This chapter shows how to start and stop it,
how to read its log, and what an operator can ask it to do.

== The Procedure <mon-proc>

The sample procedure #cmd("OFTPD") in #cmd("OFTPD.SAMPLIB") starts the monitor.
Copy it to your procedure library. It runs the program #cmd("OFTPD") from the
load library of OFTPD with a region of 8 MB and no time limit, and it names the
data definitions that the monitor needs. The chapter “Configuration and the
Partner Table” of the _OFTPD Reference_ describes each one.

#screen(```
//OFTPD    PROC M=OFTPPRM0,P=OFTPPTN0,
//            D='SYS2.PARMLIB',
//            S='OFTP.SPOOL'
//OFTPD    EXEC PGM=OFTPD,REGION=8M,TIME=1440
//STEPLIB  DD  DISP=SHR,DSN=OFTPD.LINKLIB
//OFTPPRM  DD  DSN=&D(&M),DISP=SHR,FREE=CLOSE
//OFTPPTN  DD  DSN=&D(&P),DISP=SHR
//OFTPSPL  DD  DSN=&S,DISP=OLD
//OFTPLOG  DD  SYSOUT=*,DCB=(RECFM=FB,LRECL=160,BLKSIZE=160)
//SYSPRINT DD  SYSOUT=*
//STDOUT   DD  SYSOUT=*
//STDERR   DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
```)

The parameters name the members of the settings and the partner table, the
library that holds them, and the spool. Give another one when you start the
monitor, for example #cmd("S OFTPD,S='MY.SPOOL'") or
#cmd("S OFTPD,P=OFTPPTN1").

The started task needs the right to update the spool, and to create data sets
under the prefixes #cmd("INPREFIX") and #cmd("TMPPREFIX"). If the system
protects data sets with RAKF, give the user ID of the started task update
access to the spool and the authority to create data sets under these
prefixes.

== Authorization <mon-auth>

#idx("APF")#idx("authorization")
The monitor asks for authorization when it starts. It is authorized if it is
loaded from an APF-authorized library, and otherwise it asks for authorization
through SVC 244, where RAKF permits that. The log says which way it went
(#cmd("OFTP004P")). To authorize the library, add it with its volume to the
member #cmd("IEAAPF00") of #cmd("SYS1.PARMLIB"). MVS 3.8j reads that member
only at IPL.

If the monitor is not authorized, it writes #cmd("OFTP005W") and goes on. It
then cannot start its cross-memory service, so the #cmd("OFTP") utility and the
dialog cannot reach it. Partners and the operator commands still work.

== Starting the Monitor <mon-start>

Start the monitor from the console.

#screen(```
S OFTPD
```)

During the start the monitor reads the settings and the partner table, opens
the log and the spool, sets right what the previous run left in the spool,
starts the cross-memory service and begins to listen. The console shows
#cmd("OFTP000I") with the version, and when the start is done,
#cmd("OFTP001I OFTPD READY"). A partner entry with an error is reported with
#cmd("OFTP010W") and disabled, and the others work. The rest of the start is in
the log only: whether the monitor is authorized (#cmd("OFTP004P")), how many
partners are enabled (#cmd("OFTP014P")), the port it listens on
(#cmd("OFTP051P")), and the cross-memory service with its SVC number and its
subsystem name (#cmd("OFTP039P")).

If the monitor cannot start, it ends with a message that names the cause.

#tab(caption: [Why the monitor does not start])[
  #table(columns: (1.4in, 1fr),
    [Message], [Cause],
    [#cmd("OFTP011E")], [DD #cmd("OFTPPRM") cannot be opened.],
    [#cmd("OFTP012E")], [#cmd("OFTPPRM") has no #cmd("ODETTEID").],
    [#cmd("OFTP020E")], [The spool cannot be opened. Check DD #cmd("OFTPSPL"),
      and that no other job has the cluster open.],
    [#cmd("OFTP050E")], [The port is in use. Another program holds it, perhaps
      an OFTPD that has not ended.],
    [#cmd("OFTP091E")], [No storage. Raise the region.],
  )
]

A problem with the log, the partner table or the cross-memory service does not
stop the monitor. It writes #cmd("OFTP022W"), #cmd("OFTP013W"), or one of the
messages #cmd("OFTP033E"), #cmd("OFTP034E"), #cmd("OFTP035W") and
#cmd("OFTP038P"), and runs without that part. A
monitor without cross-memory service is useless to users of the utility, so
look at these messages.

When the monitor starts after an abnormal end of the previous run, it sets
right the transfers that were in a session. #cmd("OFTP021W") says that the
spool was not closed, and #cmd("OFTP040P") how many sends were put back for
another attempt and how many incomplete receives were discarded. See
@ops-recover.

== The Log <mon-log>

#idx("log")#idx("OFTPLOG")
The monitor writes every message to the log, the data set of the DD
#cmd("OFTPLOG"). Each line has the date and the time in front.

#screen(```
2026-10-20 14:03:51 OFTP211P MAC1: T0000012 BATCH.TEST2 SENT, 120 RECORDS
```)

The console gets only the messages an operator has to act on. Messages of the
classes I, W and E go to the console and the log. The messages of class P, the
protocol messages that report the normal work of sessions and transfers, go to
the log only. Every session message begins with the name of the partner, so the
lines of one partner can be picked out of the log.

Give the DD these attributes.

- Record format F or FB, with a record length of 143 or more, as in the sample
  procedure. A line is the date and time (20 bytes) and at most 123 bytes of the
  message. A shorter record cuts the line, and the cut line ends in a
  greater-than sign.
- A DCB on the DD itself. A SYSOUT DD without a DCB is refused, and the log is
  not used.
- A held SYSOUT class if you want to read the log afterwards. JES2 keeps a SYSOUT
  data set for the job, and you read it when the started task ends. With
  #cmd("SYSOUT=*") the output of a started task usually goes to a class that
  JES2 prints and purges at once, so that nothing is left when you want it.

There is no switching of the log and no size limit. Stop and start the monitor to
get a new data set.

Without the DD, or if it cannot be opened, the monitor writes #cmd("OFTP022W")
and sends all messages to the console, those of class P too. If a write to the
log fails later, it writes #cmd("OFTP023E") once and does the same from then on.
The chapter “Messages” of the Reference lists each message with its meaning and
what to do.

== What the Operator Can Do <mon-oper>

#idx("operator commands")
The operator controls the monitor with the MODIFY command, abbreviated
#cmd("F"), and stops it with STOP.

#tab(caption: [The operator commands])[
  #table(columns: (2.0in, 1fr),
    [Command], [What it does],
    [#cmd("F OFTPD,D P")], [Shows the partners and the number of sessions.],
    [#cmd("F OFTPD,D Q")], [Shows the transfers in the spool.],
    [#cmd("F OFTPD,D S")], [Shows the number of sessions.],
    [#cmd("F OFTPD,D M")], [Shows the storage that is left.],
    [#cmd("F OFTPD,CALL name")], [Calls a partner now.],
    [#cmd("F OFTPD,REFRESH")], [Reads the partner table again.],
    [#cmd("F OFTPD,HOLD name")], [Holds the sends to a partner, or to
      #cmd("ALL").],
    [#cmd("F OFTPD,RELEASE name")], [Releases them again.],
    [#cmd("F OFTPD,PURGE days")], [Deletes finished transfers from the spool.],
    [#cmd("F OFTPD,TRACE ON|OFF name")], [Switches the protocol trace on or off
      for a partner, or for all.],
    [#cmd("P OFTPD")], [Stops the monitor.],
  )
]

The chapter “Operator Commands” of the Reference describes them, and the
answers they give. Users and administrators who work from TSO or batch use the
#cmd("OFTP") utility instead, see @ug-transfer.

== Stopping the Monitor <mon-stop>

#idx("STOP")
Stop the monitor with

#screen(```
P OFTPD
```)

The monitor writes #cmd("OFTP002I"), stops accepting calls and the cross-memory
service, and waits for the running sessions to end. While it waits it writes
#cmd("OFTP095I") with the number of sessions left. A transfer that is not
finished stays in the spool, and goes on at the next start. When all is closed
it writes #cmd("OFTP003I") and ends with return code 0.

If a session does not end, the monitor writes #cmd("OFTP096W") every minute and
keeps waiting. As a last resort, CANCEL the started task. Its abend recovery
closes the sockets and gives the SVC entry back, and the next start recovers the
transfers that were in a session (#cmd("OFTP098E")).
