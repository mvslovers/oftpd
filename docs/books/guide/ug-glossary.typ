#import "../bookmaster/bookmaster.typ": *

= Glossary <ug-glossary>

#idx("glossary")
This glossary defines the terms of OFTPD and of OFTP as this book uses them.

#deflist(width: 1.6in,
  [administrator], [A user named with #cmd("ADMIN") in #cmd("OFTPPRM"). An
    administrator sees and changes all transfers, including the files received,
    and may call a partner, purge the spool, and add, change and delete
    partners.],
  [buffer], [The size of the blocks in which a session carries data. The two
    sides agree on it when the session starts.],
  [call], [To open a session with a partner. OFTPD calls a partner that has a
    host when a file waits for it, or when the operator asks for it.],
  [clearing centre], [A system that passes files on to other systems. A partner
    that is reached through one is described with #cmd("VIA").],
  [credit], [The number of data blocks a side may send before the other side
    must say that it has them. The two sides agree on it when the session
    starts.],
  [EERP], [End-to-end response. The positive receipt that a partner sends when
    it has a file. It moves a send to the state #cmd("DELIVERED").],
  [ESID], [End session: the command that closes a session, with a reason code.
    The code 00 is a normal end.],
  [finished transfer], [A transfer in the state #cmd("DELIVERED"),
    #cmd("CONFIRMED"), #cmd("REJECTED") or #cmd("FAILED"). Only finished
    transfers are removed from the spool by housekeeping.],
  [housekeeping], [The removal of finished transfers from the spool after
    #cmd("KEEPDAYS") days.],
  [monitor], [The started task #cmd("OFTPD"), which listens, calls, sends,
    receives and keeps the spool.],
  [NERP], [Negative end-to-end response. The receipt with which a partner says
    that it has refused a file after it was transferred. It moves a send to the
    state #cmd("REJECTED").],
  [Odette ID], [The name by which a system is known in OFTP, up to 25
    characters.],
  [operator], [The person who controls the monitor from the console with
    #cmd("F OFTPD") and #cmd("P OFTPD").],
  [partner], [A system that exchanges files with OFTPD, described by an entry in
    the partner table.],
  [partner kept by command], [A partner that an administrator added or changed
    with the #cmd("PARTNER") command or in the dialog. OFTPD keeps it in the
    spool, and it wins over an entry of the same name in #cmd("OFTPPTN").],
  [partner table], [The table of the partners. Its sources are the member
    #cmd("OFTPPTN") and the partners kept by command.],
  [reason code], [The two-digit number with which a refusal says why, for
    example 01 for a file name that is not valid, 12 for a failure of the access
    method, and 13 for a file that is already there.],
  [receipt], [The answer of the receiving side that says it has a file (an EERP)
    or has refused it (a NERP).],
  [receive rule], [A rule in the member #cmd("OFTPRCV") that says how files from
    certain partners are named, whether they replace an existing data set, and
    how they are allocated.],
  [session], [A connection between two systems, in which the two sides identify
    themselves and then exchange files and receipts.],
  [settings], [The member #cmd("OFTPPRM"), which describes the system on which
    OFTPD runs and the limits of the monitor.],
  [SFID, SFNA], [The start-file command, with which a sender announces a file,
    and the negative answer to it, with which the receiver refuses the file and
    says whether to try again. The end-file negative answer is the EFNA.],
  [spool], [The VSAM data set in which the monitor keeps one record for every
    transfer, and the state of each. It also holds the partners kept by command.],
  [SVC], [The supervisor call that the installation reserves for OFTPD. The
    utility and the dialog reach the monitor through it.],
  [transfer], [The sending or the receiving of one file, kept as one record in
    the spool, and named by its key, such as T0000012.],
  [utility], [The program #cmd("OFTP"), which runs under TSO and in batch, and
    with which users send files and look at transfers.],
  [virtual file name], [The name of a file as the two partners know it. It has
    nothing to do with the name of the data set on either system.],
)
