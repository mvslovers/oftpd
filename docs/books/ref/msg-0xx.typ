#import "../bookmaster/bookmaster.typ": *
#import "msg-lib.typ": msg

#msg("OFTP000I", "version STARTING",
  [The OFTPD started task begins. #var("version") is the product name, the version number and the build identifier in parentheses. The message follows the opening of the log. It is the first message of a start, unless #cmd("OFTP022W") came before it.],
  [None. Information only.])

#msg("OFTP001E", "NO COMMAND IN PARM OR SYSIN. OFTP SEND|LIST|SHOW|DELETE|HOLD|RELEASE|RETRY|CALL|PARTNERS|PARTNER|PURGE", [The OFTP utility found no command in the PARM string and no command card in SYSIN. Blank cards and cards that start with an asterisk are not commands. The utility ends with return code 8.], [Give a command in the PARM string, or in SYSIN, and run the utility again.], dest: "utility output")

#msg("OFTP001I", "OFTPD READY",
  [OFTPD has read its settings and partner table, opened the spool, recovered it, and is listening on its port. It now accepts calls from partners, runs the scheduler for the transfers waiting in the spool, and answers #cmd("F OFTPD") commands. The cross-memory start has been tried by now and has reported its outcome, whether it succeeded or not.],
  [None. Information only.])

#msg("OFTP002E", "UNKNOWN COMMAND word", [The OFTP utility does not know the command #var("word"). The known commands are #cmd("SEND"), #cmd("LIST"), #cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"), #cmd("RELEASE"), #cmd("RETRY"), #cmd("CALL"), #cmd("PARTNERS") and #cmd("PURGE"). The command is not sent to OFTPD, and the return code of the run is at least 8. When the commands come from SYSIN, the utility goes on with the next card.], [Correct the command and run the utility again.], dest: "utility output")

#msg("OFTP002I", "OFTPD STOPPING",
  [OFTPD has received the #cmd("P OFTPD") command and begins to end. It stops the cross-memory service, the listener and the scheduler, then waits for the running sessions to end. While sessions remain, #cmd("OFTP095I") is written. A stop of this kind is followed by #cmd("OFTP003I").],
  [None. Information only.])

#msg("OFTP003E", "COMMAND TOO LONG", [The OFTP utility cannot take a command because it is too long. #cmd("COMMAND TOO LONG") is written when the command with its operands, as the utility turns them into a request, does not fit in 255 characters, or when the cards of a continued command in SYSIN together reach 255 characters. The variant #cmd("OPERAND TOO LONG: keyword") is written for an operand whose value is longer than 79 characters. It names the keyword and never the value, because the value can be a password. The variant #cmd("OPERAND TOO LONG") has no keyword\; it is written for an operand whose keyword (or lone word) is longer than 15 characters. The command is not sent to OFTPD, and the return code is at least 8.], [Shorten the command, or split it into several commands, and run the utility again.], dest: "utility output")

#msg("OFTP003I", "OFTPD ENDED",
  [OFTPD has ended after a #cmd("P OFTPD") command. The sessions are over and the spool is closed. This is the last message of a normal run. The started task ends with return code 0.],
  [None. Information only.])

#msg("OFTP004P", "AUTHORIZED BY LIBRARY",
  [OFTPD runs authorized and says how it got there. The text is #cmd("AUTHORIZED BY LIBRARY") when OFTPD was already authorized at entry, which means that it was loaded from an APF-authorized library. The text is #cmd("AUTHORIZED BY SVC") when it was not, and the request for authorization through the authorization SVC succeeded.],
  [None. Information only.])

#msg("OFTP005W", "NOT AUTHORIZED, RC=n, CONTINUING",
  [OFTPD is neither in an APF-authorized library nor did the request for authorization succeed. #var("n") is the return code of that request. OFTPD continues, but it cannot start the cross-memory service in this state, so the OFTP utility cannot reach it. If SVC= is set in OFTPPRM, #cmd("OFTP035W") says so\; if not, #cmd("OFTP038P") does.],
  [Put the library OFTPD is loaded from on the APF list of the system, then restart the OFTPD started task. If you do not need the utilities, nothing is needed.])

#msg("OFTP006W", "NO ABEND RECOVERY (ESTAE FAILED)",
  [OFTPD could not set up its abend recovery. It continues. If OFTPD abends in this state, nothing closes its sockets, so the port can stay held, and the SVC entry the cross-memory service took is not given back.],
  [None.])

#msg("OFTP007E", "OPERAND n HAS NO VALUE: WRITE KEYWORD(VALUE)",
  [The OFTP utility found a word without a value where it expects the form #cmd("KEYWORD(VALUE)") or #cmd("KEYWORD=VALUE"). A command that has a lone operand (#cmd("SHOW"), #cmd("DELETE"), #cmd("HOLD"), #cmd("RELEASE"), #cmd("RETRY"), #cmd("CALL"), #cmd("PURGE") and #cmd("PARTNER")) takes the first lone word for its main operand\: the key, the partner or the number of days. A second lone word is refused. #var("n") is the place of the word, counting the words after the command from 1 (after the sub-command, for #cmd("PARTNER"))\; the word is not shown, because it can be a password, for example the second half of one that has a blank in it. The command is not sent to OFTPD, and the return code is 8. When the commands come from SYSIN, the utility goes on with the next card.],
  [Write the operand as #cmd("KEYWORD(VALUE)"). In SYSIN a value that holds a blank goes in single quotes. Then run the command again.],
  dest: "utility output")

#msg("OFTP010E", "text", [OFTPD did not carry out a command of the OFTP utility, and the utility ends the command with return code 12. #var("text") tells why. The texts that can appear are: #cmd("OFTPD not reached: not running, or without SVC="), #cmd("OFTPD stopped before carrying out the request"), #cmd("incomplete answer from OFTPD"), #cmd("answer from OFTPD does not end"), #cmd("transport answered n"), and the text of a final answer of OFTPD that the utility rates as return code 12, which is one of #cmd("unknown partner"), #cmd("unknown or disabled partner"), #cmd("the partner it is reached through is disabled") and #cmd("not a partner that can be called").], [For the first text, check that the OFTPD started task is running and that #cmd("OFTPPRM") has the #cmd("SVC=") keyword, which the utility needs to find OFTPD\; #cmd("OFTP039P") in the log of the start says that cross memory is active. The second means OFTPD is stopping\; run the command again when it is running. For a partner text, check the name with the #cmd("PARTNERS") command. For the other texts, run the command again and look at the console for other OFTPD messages.], dest: "utility output")

#msg("OFTP010W", "member: line n: text",
  [OFTPD does not understand a line of OFTPPRM or OFTPPTN. #var("member") is #cmd("OFTPPRM") or #cmd("OFTPPTN"). #var("n") is the record number in that member, counting every record including comments, and columns 73 to 80 are ignored. The texts that can follow are: #cmd("word n is not KEYWORD=VALUE") (the place of the word on the line, never the word), #cmd("a word is longer than 99 characters"), #cmd("unknown keyword tok"), #cmd("invalid value for tok"), #cmd("tok before the first PARTNER="), and #cmd("more than 32 partners"). Three more come only from the partner table: #cmd("partner name has no ODETTEID, disabled"), #cmd("partner name defined twice, disabled") and #cmd("partner name: VIA names no direct partner, disabled"). A word that is not KEYWORD=VALUE, a word of more than 99 characters, or a keyword before the first PARTNER=, is skipped\; a word of more than 99 characters also disables the partner that was being defined in OFTPPTN. In OFTPPRM an unknown keyword is skipped and an invalid value leaves the default in place. In OFTPPTN an unknown keyword or an invalid value disables the partner that was being defined. A partner is also disabled when its ODETTEID is missing, when its name or its ODETTEID repeats an earlier entry, or when its VIA does not name an enabled partner without a VIA of its own. After #cmd("more than 32 partners") the PARTNER= entry is skipped, and the keywords that follow it on later lines apply to the last partner read. OFTPD starts anyway. The message is written again for OFTPPTN by every #cmd("F OFTPD,REFRESH").],
  [Correct the line named. Changes to OFTPPTN take effect with #cmd("F OFTPD,REFRESH"). OFTPPRM is read only when OFTPD starts, so restart the OFTPD started task after you change it.])

#msg("OFTP011E", "DD OFTPPRM CANNOT BE OPENED",
  [OFTPD cannot open the data set behind DD OFTPPRM, the monitor settings. It has no settings to work with, so it ends with return code 8 and does not open the spool or listen.],
  [Check that the OFTPD procedure has a DD OFTPPRM and that it names an existing data set or member. Then start OFTPD again.])

#msg("OFTP012E", "OFTPPRM HAS NO ODETTEID",
  [OFTPPRM has been read but contains no usable ODETTEID, the Odette ID of this system. OFTPD ends with return code 8. An ODETTEID that is too long is reported by #cmd("OFTP010W") before this message and counts as missing. An empty value counts as missing too, without that message.],
  [Add ODETTEID=#var("id") to OFTPPRM and start OFTPD again. The member must be the one the procedure names, and the record must not run into columns 73 to 80.])

#msg("OFTP013W", "DD OFTPPTN CANNOT BE OPENED, NO PARTNERS",
  [OFTPD cannot open the data set behind DD OFTPPTN, the partner table. It continues with an empty table and accepts no partner. #cmd("OFTP014P") then reports 0 OF 0.],
  [Check that the OFTPD procedure has a DD OFTPPTN that names an existing data set or member. When it is right, #cmd("F OFTPD,REFRESH") reads the table without a restart.])

#msg("OFTP014P", "ODETTE ID id, n OF m PARTNERS ENABLED",
  [OFTPD has read the partner table at start. #var("id") is the ODETTEID from OFTPPRM, #var("m") is the number of PARTNER= entries read and #var("n") the number that are enabled. An entry that #cmd("OFTP010W") disabled, or that has ENABLED=NO, is not counted in #var("n"). If DD OFTPLOG cannot be used, this message goes to the console.],
  [None. Information only. If #var("n") is smaller than you expect, look for #cmd("OFTP010W") messages.])

#msg("OFTP015W", "PARTNER name NOT ADDED, TABLE FULL",
  [A refresh found the partner #var("name") in OFTPPTN, but the table in the running OFTPD already holds 32 entries, after the slots of the partners that left the member have been freed. The new partner is skipped and the refresh goes on. A partner that has left OFTPPTN keeps its entry, disabled, as long as a transfer in the spool names it or a session of it is running.],
  [Remove entries from OFTPPTN that you do not need, so that no more than 32 remain, and use #cmd("F OFTPD,REFRESH") again. A partner that is skipped is read again by the next refresh. An entry that you removed from OFTPPTN and that stays in the table is freed by a refresh as soon as no transfer in the spool names it. Delete such transfers with #cmd("DELETE") or #cmd("PURGE").])

#msg("OFTP016I", "PARTNERS RELOADED: n ENABLED, n NEW, n DISABLED, n REMOVED",
  [OFTPD has read OFTPPTN again after #cmd("F OFTPD,REFRESH") or the same request from the ISPF dialog of an administrator. The first number is the enabled partners now in the table. The second is the number of partners found in OFTPPTN that were not in the table before. The third is the number of partners that were enabled, are no longer in OFTPPTN, and stay in the table, disabled, because a transfer in the spool names them or a session of them is running. The fourth is the number of partners that are no longer in OFTPPTN and that OFTPD has taken out of the table\; their slots are free. A partner whose entry now has ENABLED=NO is counted in none of the last three. Every partner that is still in OFTPPTN has been replaced by its entry.],
  [None. Information only. Check the numbers against what you changed, and look for #cmd("OFTP010W") messages before this one. A partner counted in the third number is taken out by a later refresh, when nothing names it any more.])

#msg("OFTP017E", "DD OFTPPTN CANNOT BE READ",
  [The operator command #cmd("F OFTPD,REFRESH") could not open DD OFTPPTN, or OFTPD had no storage for the new table. The partner table in use is unchanged. A refresh requested from the ISPF dialog of an administrator gets an error reply in its own output instead of this message.],
  [Check the DD OFTPPTN of the procedure. If the DD is right and the message stays, restart the OFTPD started task.])

#msg("OFTP020E", "SPOOL NOT OPEN: reason",
  [OFTPD cannot open the spool, the VSAM data set behind DD OFTPSPL, and ends with return code 8. #var("reason") is one of: #cmd("no storage"), #cmd("open rc n, VERIFY condition code n"), #cmd("open rc n, again rc n after VERIFY"), #cmd("control record: lookup rc n reason n, insert rc n reason n"), #cmd("DD OFTPSPL cannot be opened"), #cmd("initial load failed, rc n"), #cmd("reopen after initial load failed") and #cmd("spool thread not started"). The numbers are VSAM and IDCAMS codes. After a failed open OFTPD runs a VERIFY for the cluster once and opens it again before it gives up.],
  [Check DD OFTPSPL of the procedure. It must name the spool cluster with DISP=OLD, and no other job may have it open. Check that the cluster is defined as in the installation job, and look at the VSAM messages on the console and in the job log for the codes named. Then start OFTPD again.])

#msg("OFTP021W", "SPOOL WAS NOT CLOSED (OPEN RC n), VERIFIED",
  [The first open of the spool failed with return code #var("n"), as it does when the previous OFTPD did not close it. OFTPD ran VERIFY for the cluster, and the second open worked. OFTPD continues. #cmd("OFTP040P") follows if the spool held transfers that were in a session at the time.],
  [None. The previous run usually ended abnormally, for example by an abend or a CANCEL. After such an end, #cmd("OFTP098E") is normally in the log of that run.])

#msg("OFTP022W", "DD OFTPLOG CANNOT BE OPENED, MESSAGES GO TO THE CONSOLE",
  [OFTPD cannot open the log, so the messages go to the console only for this run, class P messages as well. The other text is #cmd("DD OFTPLOG IS NOT RECFM=F OR FB, MESSAGES GO TO THE CONSOLE"), when the data set has another record format. OFTPD starts and runs without a log. The message is the first of the start.],
  [Add a DD OFTPLOG to the procedure, with RECFM=F or FB, and start OFTPD again. A message is written as one record of the data set. If the record length is shorter than the log line (20 bytes of date and time and a message of at most 123 bytes), the line is cut and its last character is replaced by >.],
  dest: "console only")

#msg("OFTP023E", "LOG WRITE FAILED, ERRNO n, MESSAGES GO TO THE CONSOLE FROM NOW ON",
  [A write to the log failed. #var("n") is the error number of the write, or -1 if the write gave none. OFTPD gives up the log for the rest of the run and sends every message to the console, so that none is lost. The message is written once. The other text is #cmd("LOG CLOSE FAILED, ERRNO n"), when closing the log at the end of OFTPD failed.],
  [Check the data set behind DD OFTPLOG, for example for a full volume or a full output spool. OFTPD does not open the log again, so restart the OFTPD started task to get the log back.],
  dest: "console only")

#msg("OFTP030P", "CROSS MEMORY ON SVC n, ANCHOR=address MODULE=address (n BYTES), NOT ACTIVE", [OFTPD has installed the cross-memory path for the OFTP utility and the ISPF dialog on the SVC number named by #cmd("SVC=") in #cmd("OFTPPRM"). It has allocated the anchor in common storage (CSA), which holds the four request slots, and loaded the routine #cmd("OFTPSVC") into CSA. The two addresses are hex, the number in brackets is the size of the routine in bytes. The SVC table entry now points to the routine. The path does not serve callers yet\: #cmd("OFTP039P") follows when it is switched on.], [None. Information only. The addresses help when you read a dump.])

#msg("OFTP031P", "SVC n ENTRY FOUND word word", [Before OFTPD takes the SVC table entry for the number from #cmd("SVC="), it logs the eight bytes it finds there, as two hex words. The first word is the entry point address. OFTPD saves these bytes and puts them back when it ends (#cmd("OFTP032P")). If the entry is not free, #cmd("OFTP033E") follows.], [None. Information only. Keep the line\: it shows what the entry held before OFTPD took it.])

#msg("OFTP032P", "SVC n RESTORED, ENTRY READS word word", [OFTPD is ending and has put the SVC table entry back. It has read the entry again and compared it with the bytes it saved at start (#cmd("OFTP031P")). They are equal, so the SVC number is free again for other programs. The two words are the entry as it reads now.], [None. Information only.])

#msg("OFTP033E", "SVC n NOT FREE (EP address LOCK x, UNUSED EP address), NO CROSS MEMORY", [The SVC number from #cmd("SVC=") in #cmd("OFTPPRM") is already in use. OFTPD treats an entry as free when its entry point is the one most unused entries in the range 200 to 255 share (shown as UNUSED EP) and it asks for no lock. This entry has another entry point or a lock byte (shown after LOCK). OFTPD does not touch the entry, frees the routine and the anchor and runs on without cross memory. Partners and the operator commands still work\; the OFTP utility and the ISPF dialog find no OFTPD.], [Choose a number between 200 and 255 that no other program uses, put it in #cmd("OFTPPRM") as #cmd("SVC=") and restart the OFTPD started task. If the number is the one you meant, find which product holds it. A number left over from an earlier OFTPD that could not give it back is described under #cmd("OFTP036W").])

#msg("OFTP034E", "NO CROSS MEMORY: reason", [OFTPD could not install or switch on the cross-memory path and runs on without it. Partners and the operator commands still work\; the OFTP utility and the ISPF dialog find no OFTPD. The reason is one of\: SUPERVISOR STATE REFUSED (OFTPD could not enter supervisor state and key 0)\; NO CSA FOR n BYTES FROM SUBPOOL 241 (the anchor could not be allocated in common storage)\; OFTPSVC NOT LOADED INTO CSA (RC n) (the routine #cmd("OFTPSVC") could not be loaded into common storage, with the return code of the load)\; or name IS n, OFTPSVC EXPECTS n, where name is a field of the request slot or of the anchor. The last one means that the layout of the data OFTPD shares with #cmd("OFTPSVC") is not the one the routine is written for. OFTPD checks this before it installs anything. SUPERVISOR STATE REFUSED is also written at the last step of the start, when OFTPD switches the path on\; then the routine and the SVC entry are already installed, and OFTPD gives the SVC entry back when it ends.], [For NO CSA, check the free common storage of the system. For NOT LOADED, check that the member #cmd("OFTPSVC") is in a load library OFTPD can reach, such as its STEPLIB. A field mismatch is not a setting you can change\; the OFTPD in use does not fit its #cmd("OFTPSVC"), so install a matching release. For SUPERVISOR STATE REFUSED, look at the authorization messages at the start (#cmd("OFTP004P"), #cmd("OFTP005W")). Then restart the OFTPD started task.])

#msg("OFTP035W", "NO CROSS MEMORY, NOT AUTHORIZED", [An #cmd("SVC=") is set, but OFTPD is not running authorized. The cross-memory path needs it, so OFTPD does not install it and runs on without. Partners and the operator commands still work\; the OFTP utility and the ISPF dialog find no OFTPD. At start OFTPD has already logged whether it got authorization (#cmd("OFTP004P")) or not (#cmd("OFTP005W")).], [Authorize the load library OFTPD runs from through the APF list of the system, make sure all libraries in its STEPLIB are authorized, and restart the OFTPD started task.])

#msg("OFTP036W", "SVC n NOT RESTORED (NO KEY 0), CSA RETAINED", [OFTPD is ending but could not put the SVC table entry back, because it could not enter key 0. The same text is written when OFTPD put the entry back but could not enter key 0 again to read it back. Variant\: SVC n NOT RESTORED, ENTRY READS word word, CSA RETAINED. OFTPD put the entry back, read it again and found it different from what it saved at start. In all cases OFTPD leaves the routine and the anchor in common storage, so that a call through the entry still finds valid code. The SVC number may stay unusable until the next IPL.], [In the variant, the words show what the entry holds\; #cmd("OFTP031P") of the start shows what it held before. When a new OFTPD starts and finds the entry taken, it ends cross memory with #cmd("OFTP033E"). In that case set another #cmd("SVC=") in #cmd("OFTPPRM"), or restart after an IPL.])

#msg("OFTP037W", "CROSS MEMORY DRAIN TIMED OUT, n IN FLIGHT, CSA RETAINED", [OFTPD is ending. It has switched the cross-memory path off, taken its subsystem entry out of the chain and waited up to 10 seconds for callers still inside the routine to leave. n callers have not. The SVC entry is put back (see #cmd("OFTP032P")), but OFTPD does not free the routine and the anchor in common storage. A caller may still run in them. The storage stays allocated until the next IPL.], [None needed for the operation of the system. Look for a stuck TSO user or batch job that was calling OFTPD.])

#msg("OFTP038P", "NO SVC= IN OFTPPRM, NO CROSS MEMORY", [#cmd("OFTPPRM") has no #cmd("SVC=") keyword. This is a valid setting\: OFTPD does not install the cross-memory path and runs without it. Partners and the operator commands still work\; the OFTP utility and the ISPF dialog find no OFTPD. The message is there so that a missing keyword does not go unnoticed.], [If the utility and the dialog are wanted, add #cmd("SVC=n") to #cmd("OFTPPRM") with a free SVC number from 200 to 255 and restart the OFTPD started task. Otherwise none.])

#msg("OFTP039P", "CROSS MEMORY ACTIVE ON SVC n, SUBSYSTEM name", [The last step of the start is done. OFTPD has chained its subsystem entry (name is OFT1) and switched the path on. The OFTP utility and the ISPF dialog find OFTPD by the subsystem name and reach it through SVC n. The slot thread of OFTPD answers their requests, at most four at a time.], [None. Information only.])

#msg("OFTP040P", "RECOVERED n SENDS, DISCARDED n PARTIAL RECEIVES",
  [At start OFTPD found transfers in the spool that were in a session when the previous OFTPD ended. A send is put back to waiting and will be sent again by the scheduler. A receive that was not finished is deleted, together with its temporary data set. The first number is the sends and the second the receives. The message is not written if both are 0.],
  [None. The #cmd("F OFTPD,D Q") command lists the transfers in the spool with their state.])

#msg("OFTP050E", "LISTEN ON PORT n FAILED, ERRNO n",
  [OFTPD cannot listen on the port #var("n") named by PORT in OFTPPRM, 3305 if the keyword is absent. The socket, bind or listen call failed with the error number given. OFTPD closes the spool and ends with return code 8, without #cmd("OFTP002I") and #cmd("OFTP003I"). A thread that cannot be created at this point also ends OFTPD without a further message.],
  [Check whether another program holds the port, for example an OFTPD that is still running or that abended and left its socket open. Change PORT in OFTPPRM, or free the port, and start OFTPD again.])

#msg("OFTP051P", "LISTENING ON PORT n, SOCKET n",
  [OFTPD listens on the port #var("n") for calls from partners. The second number is the number of the socket, which is a system-wide number.],
  [None. Information only.])

#msg("OFTP060W", "CROSS MEMORY SWEPT SLOTS X'mask', A CALLER FAULTED INSIDE THE SVC ROUTINE", [OFTPD checks the four request slots once a second. A slot that stays claimed for 3 checks, or in copy-out for 2, belongs to a caller that stopped inside the routine, for example one that abended. OFTPD frees the slot. The mask has one bit per slot, X'01' for the first and X'08' for the fourth. The slot is usable again.], [None needed. If it repeats, look for abends of TSO users or jobs that call the OFTP utility or the dialog.])

#msg("OFTP061W", "CROSS MEMORY FREED SLOTS X'mask' UNCOLLECTED FOR n SECONDS", [OFTPD had answered a request, but the caller did not collect the answer for more than n seconds (300), and OFTPD could not see that the caller was gone. OFTPD frees the slot. A typical cause is a caller purged while it waited, for example a cancelled TSO command. The mask has one bit per slot, X'01' for the first.], [None needed. The slot is usable again.])

#msg("OFTP062W", "SUBSYSTEM name WAS STILL INSTALLED FROM AN EARLIER RUN, UNCHAINED; ITS CSA IS RETAINED", [At start OFTPD finds the subsystem entry (name is OFT1) of an earlier OFTPD that did not end cleanly, for example after an abend. OFTPD takes the old entry out of the chain, so that its own entry can be put in. It does not free the old entry or the anchor it names, because another address space may still read them. A small amount of common storage is lost with each such start.], [None needed. If the earlier run ended with an abend, #cmd("OFTP098E") says whether the SVC entry was put back. Common storage is freed again at the next IPL.])

#msg("OFTP063E", "SUBSYSTEM name NOT INSTALLED, CROSS MEMORY STAYS OFF", [OFTPD could not chain its subsystem entry (name is OFT1). The OFTP utility and the ISPF dialog find OFTPD through this entry, so OFTPD does not switch the path on without it. The SVC entry and the anchor are installed (see #cmd("OFTP030P")) but not used\; OFTPD gives the SVC entry back when it ends. Partners and the operator commands still work.], [Restart the OFTPD started task. OFTPD does not say why the entry could not be chained.])

#msg("OFTP064W", "CROSS MEMORY: THE OWNER OF SLOT n IS GONE, ANSWER DISCARDED", [OFTPD carried out the request in slot n (0 to 3) and wanted to hand over the answer, but could not, because the caller was no longer there, for example a cancelled job or TSO user, or because OFTPD is stopping and has already told the caller there is no answer. OFTPD drops the answer. The request itself was carried out.], [None. If the request mattered, check its effect with the OFTP utility.])

#msg("OFTP065E", "CROSS MEMORY SLOT THREAD NOT STARTED, CROSS MEMORY STAYS OFF", [OFTPD could not start the thread that answers requests from the OFTP utility and the ISPF dialog. The SVC entry and the anchor are installed (see #cmd("OFTP030P")) but the path is not switched on\; OFTPD gives the SVC entry back when it ends. Partners and the operator commands still work\; the utility and the dialog find no OFTPD.], [Restart the OFTPD started task. If it repeats, check the region size of the OFTPD started task\; the thread needs a stack of 32 KB.])

#msg("OFTP066W", "CROSS MEMORY: THE OWNER OF SLOT n IS GONE, REQUEST NOT CARRIED OUT", [OFTPD found a request in slot n (0 to 3) whose caller has gone, for example a cancelled job or TSO user. OFTPD frees the slot and does not carry the request out.], [None. Issue the request again if it is still wanted.])

#msg("OFTP067W", "CROSS MEMORY FREED SLOTS X'mask', THEIR CALLER IS GONE", [OFTPD had answered the requests in these slots, and their callers have gone before they collected the answers, for example a cancelled job or TSO user. OFTPD frees the slots. The mask has one bit per slot, X'01' for the first.], [None needed. The slots are usable again.])

#msg("OFTP090E", "COM AREA NOT AVAILABLE",
  [OFTPD could not get the communications area it needs to receive the operator commands. It ends with return code 8 at once. The log is not open yet, so the message is on the console only.],
  [Start OFTPD again. If the message stays, the cause is not in the configuration, so keep the job log for the maintainer.],
  dest: "console only")

#msg("OFTP091E", "NO STORAGE",
  [OFTPD could not get the storage for its main control block. It ends with return code 8 at once. The log is not open yet, so the message is on the console only.],
  [Raise the REGION of the OFTPD procedure and start OFTPD again.],
  dest: "console only")

#msg("OFTP095I", "WAITING FOR n SESSIONS",
  [OFTPD is stopping and waits for #var("n") sessions to end. The text can have a word in front of the number: #cmd("LISTENER: WAITING FOR n SESSIONS") or #cmd("SCHEDULER: WAITING FOR n SESSIONS") for the sessions that thread started. The message is written when the wait begins and then about every 10 seconds. The sessions are told to end at the stop. With the word in front, the wait goes on until the sessions have ended. The text without the word comes from the last wait of the stop, after the listener and the scheduler have ended\; this wait lasts at most a minute, and OFTPD then goes on to end.],
  [None. If the wait does not end, see #cmd("OFTP096W"). The #cmd("F OFTPD,D P") command shows which partners have sessions, and #cmd("F OFTPD,D S") shows how many sessions run.])

#msg("OFTP096W", "who: n SESSION SUBTASKS STILL RUNNING",
  [OFTPD is stopping, and #var("n") sessions have not ended after a minute. #var("who") is #cmd("LISTENER") or #cmd("SCHEDULER"), the part of OFTPD that started them. OFTPD keeps waiting and repeats the message every minute. It cannot end while a session runs.],
  [Look at the partners with sessions using #cmd("F OFTPD,D P"). If a session does not end, the operator command CANCEL on the OFTPD started task ends OFTPD. An abend follows with #cmd("OFTP098E"), and the transfers that were in a session are recovered at the next start.])

#msg("OFTP098E", "OFTPD ABEND, n SOCKETS CLOSED, SVC state",
  [OFTPD has abended, or it was cancelled, and its abend recovery has run. #var("n") is the number of sockets it closed. #var("state") says what happened to the SVC entry the cross-memory service takes: #cmd("NOT TAKEN") means OFTPD had not installed it, #cmd("RESTORED") means the entry was given back, and #cmd("NOT RESTORED") means it could not be given back. In that case the SVC number stays reserved until the next IPL. The other text is #cmd("OFTPD ABEND, MONITOR BLOCK NOT FOUND"), which has no numbers. The message is the only one the recovery writes. When the log is busy at that moment, the message goes to the console only. After the message the abend goes on.],
  [Find the abend code in the job log of the started task, and the dump if the procedure has a dump DD. Start OFTPD again. At the next start, #cmd("OFTP021W") and #cmd("OFTP040P") report what the spool held. If the state is #cmd("NOT RESTORED"), report it, because the SVC number is not usable until an IPL.])
