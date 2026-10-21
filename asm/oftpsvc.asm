*---------------------------------------------------------------------*
* OFTPSVC -- MVS 3.8j OFTPD cross-memory request service.
*
* Type-3 SVC entry: R1 = caller parameter, R6 = entry point,
* R7 = caller ASCB, R14 = return address. Runs supervisor state/key 0
* in the caller's address space. No C runtime and no writable statics.
*
* The caller header and request are copied to the key-0 slot with
* MVCK under the caller's TCB key, and nothing in caller storage is
* touched before that copy -- not even the eye catcher, which is
* checked on the staged key-0 image. The reply goes back only inside
* MOVEOUT, where SPKA temporarily selects that same key.
*
* Layout mirrors include/oftpsvc.h: slot.rplyptr at +52 holds the
* caller's reply-buffer address. The request header is first staged
* in the slot's req area, then replaced by the request text.
*---------------------------------------------------------------------*
OFTPSVC  CSECT
R0       EQU   0
R1       EQU   1
R2       EQU   2
R3       EQU   3
R4       EQU   4
R5       EQU   5
R6       EQU   6
R7       EQU   7
R8       EQU   8
R9       EQU   9
R10      EQU   10
R11      EQU   11
R12      EQU   12
R13      EQU   13
R14      EQU   14
R15      EQU   15
*---------------------------------------------------------------------*
* OFTP_ANCHOR offsets
ANCEYE   EQU   0
ANCVER   EQU   8
ANCFLAG  EQU   12
ANCSVC   EQU   16
ANCSASCB EQU   20
ANCECB   EQU   24
ANCINFL  EQU   28
ANCNSLOT EQU   32
ANCFULL  EQU   36
ANCSERVD EQU   40
ANCSAVED EQU   44
ANCCLAIM EQU   52                  claim sequence, 24 bits
ANCSLOTS EQU   64

* OFTP_SLOT offsets; size X'960' (2400)
SLSTATE  EQU   0
SLASCB   EQU   4
SLASID   EQU   8
SLRECB   EQU   12
SLUSER   EQU   16
SLREQLEN EQU   24
SLRESUME EQU   28
SLCAP    EQU   32
SLREPLEN EQU   36
SLNEXT   EQU   40
SLRC     EQU   44
SLSTAMP  EQU   48
SLRPLYPT EQU   52                  slot.rplyptr
SLJBNI   EQU   56                  ASCBJBNI at claim (#36)
SLCLAIM  EQU   60                  claim sequence number
SLJOB    EQU   64                  CHKEY, CHSTEP at claim (#36)
SLJOBID  EQU   80                  SSIBJBID at claim
SLREQ    EQU   96
SLREPLY  EQU   352
SLSIZE   EQU   2400

* OFTP_SVCREQ offsets
REQEYE   EQU   0
REQVER   EQU   4
REQREPLY EQU   8
REQCAP   EQU   12
REQRESUM EQU   16
REQLEN   EQU   20
REQTEXT  EQU   24
REQEND   EQU   279

* Temporary header offsets within slot.req: the staged header's
* REQ* fields, relative to SLREQ so that they move with it
QVER     EQU   SLREQ+REQVER
QRPLY    EQU   SLREQ+REQREPLY
QCAP     EQU   SLREQ+REQCAP
QRESUM   EQU   SLREQ+REQRESUM
QLEN     EQU   SLREQ+REQLEN

* Slot states and return codes
STFREE   EQU   0
STCLAIM  EQU   1
STPEND   EQU   2
STDONE   EQU   3
STSTOP   EQU   4
STCPYO   EQU   5                   collected, copy-out in progress
RCOK     EQU   0
RCMORE   EQU   4
RCBUSY   EQU   8
RCDOWN   EQU   12
RCBAD    EQU   16
RCSTOP   EQU   20                  parked at the stop, never served
OFTVER   EQU   1                   request block layout
ANCV     EQU   3                   anchor and slot layout (#36)
MAXSLOT  EQU   4
REQMAX   EQU   255
REPMIN   EQU   256
REPMAX   EQU   2048
MVCKMAX  EQU   255
MVCMAX   EQU   256

* MVS control-block offsets
PSATOLD  EQU   540                 PSA+X'21C' -> caller TCB
TCBRBP   EQU   0                   TCB+0 -> current SVRB
TCBPKF   EQU   28                  TCB+X'1C' -> task storage key
ASCBASID EQU   36                  ASCB+X'24'
ASCBASXB EQU   108                 ASCB+X'6C'
ASCBJBNI EQU   172                 ASCB+X'AC' -> CHKEY, job CSCB
CHSTEPK  EQU   56                  CHSTEP-CHKEY, IEECHAIN
TCBJSCB  EQU   180                 TCB+X'B4' -> JSCB, 24 bits
JSCBSSIB EQU   316                 JSCB+X'13C' -> SSIB
SSIBJBID EQU   12                  SSIB+X'0C' JES job id, CL8
ASXBSENV EQU   200                 ASXB+X'C8' -> ACEE
ACEEUSER EQU   20                  ACEE+X'14', length-prefixed
RBEXSAVE EQU   96                  SVRB+X'60'
* CVT0PT01 (CVT+X'98') comes from the CVT mapping macro at the end of
* this file. A local EQU for it collides with the macro's own symbol
* (IFO196) and makes CVT0PT01-CVTMAP a mixed absolute/relocatable
* expression (IFO213, "instruction zeroed") -- and as370 exits 0 on
* both. libc370's @@xmpost.c does it the same way.
*---------------------------------------------------------------------*
         USING OFTPSVC,R6
         B     OFTPGO              skip patched anchor pointer
OFTANCH  DC    A(0)                +4: anchor address, patched by OFTPD

OFTPGO   DS    0H
         LR    R8,R1               caller's request block
         LR    R5,R7               preserve caller ASCB until claimed
         SLR   R0,R0
         SLR   R1,R1
         LTR   R8,R8
         BZ    BADREQ
*  The eye catcher is NOT checked here. A key-0 CLC on R8 would read
*  caller-supplied storage at an unvalidated address in key 0 -- an
*  oracle for the caller and an S0C4 in key 0 on a bad pointer. It is
*  checked on the staged image after the MVCK instead (BADHDR).
*---------------------------------------------------------------------*
* The caller's key, then TPROT the request header before anything else.
*
* The first of three range checks (internals/xmem.md section 6). It
* sits here, ahead of the anchor, because every exit from this point
* back is free: no in-flight count, no slot. Past the claim a program
* check leaks both, and the routine has no recovery.
*
* TPROT asks for a STORE, not a fetch, although the header is only
* read. Requiring a store means the range has to be in the caller's own
* key, and that closes a cross-key READ that MVCK alone does not:
* MVCK's source key check permits a fetch from differently keyed
* storage whose fetch-protection bit is off, which is the normal state
* of key-0 CSA.
         SLR   R10,R10
         L     R10,PSATOLD(,R10)
         SLR   R9,R9
         IC    R9,TCBPKF(,R10)      caller key, bits 24-27
         LR    R4,R8
         LA    R11,23(,R8)
         BAL   R15,TPRNG
         LTR   R1,R1
         BNZ   BADREQ
*---------------------------------------------------------------------*
* Locate and check the published anchor. No caller storage is touched.
         L     R2,OFTANCH
         LTR   R2,R2
         BZ    DOWN
         CLC   ANCEYE(8,R2),=CL8'OFTPANCH'
         BNE   DOWN
         L     R3,ANCVER(,R2)
         C     R3,=A(ANCV)
         BNE   DOWN
         L     R11,ANCNSLOT(,R2)
         LTR   R11,R11
         BNP   BUSY
         C     R11,=A(MAXSLOT)
         BH    DOWN
*---------------------------------------------------------------------*
* Count this invocation before claiming. Shutdown clears ACTIVE and
* drains this count before restoring the SVC entry or freeing the CSA.
         L     R3,ANCINFL(,R2)
INCLP    LR    R4,R3
         LA    R4,1(,R4)
         CS    R3,R4,ANCINFL(R2)
         BNE   INCLP
         TM    ANCFLAG(R2),X'80'
         BNO   DOWNDEC
*---------------------------------------------------------------------*
* Claim one FREE slot. The counter already includes this caller.
         LA    R10,ANCSLOTS(,R2)
         L     R11,ANCNSLOT(,R2)
CLAIMLP  SLR   R3,R3
         LA    R4,STCLAIM
         CS    R3,R4,SLSTATE(R10)
         BE    CLAIMOK
         LA    R10,SLSIZE(,R10)
         BCT   R11,CLAIMLP
         B     BUSYDEC

CLAIMOK  LR    R7,R10               R7 = our slot
         ST    R5,SLASCB(,R7)        caller ASCB from the FLIH
         LH    R3,ASCBASID(,R5)
         N     R3,=X'0000FFFF'
         ST    R3,SLASID(,R7)
         MVC   SLUSER(8,R7),=CL8' '
*---------------------------------------------------------------------*
* The caller's job, for a batch caller (#36). Its address space is the
* initiator's and outlives the job, so OFTPD compares this with the
* ASCB later to tell a cancelled caller from a slow one. ASCBJBNI is 0
* for TSO and started tasks; else it points at CHKEY in the job's own
* CSCB, which is common storage and holds the step name 56 bytes on.
         XC    SLJBNI(SLREQ-SLJBNI,R7),SLJBNI(R7)
         L     R3,ASCBJBNI(,R5)
         LTR   R3,R3
         BZ    JOBOK
         ST    R3,SLJBNI(,R7)
         MVC   SLJOB(8,R7),0(R3)          jobname
         MVC   SLJOB+8(8,R7),CHSTEPK(R3)  step name
JOBOK    DS    0H
*---------------------------------------------------------------------*
* The caller's JES job id, and this claim's place in the sequence.
* One address space runs one job at a time, so OFTPD reads a later
* claim from the same ASCB with another job id as the end of the
* earlier claim's job -- the one case the CSCB cannot see, a job of
* the same name back in the same initiator. The id is private storage,
* readable here because this runs in the caller's address space. It
* is stored before the sequence number, and the CS serialises, so
* whoever sees the new number sees the new id.
         SLR   R3,R3
         L     R3,PSATOLD(,R3)     caller TCB
         L     R3,TCBJSCB(,R3)
         N     R3,=X'00FFFFFF'
         BZ    JIDOK
         L     R3,JSCBSSIB(,R3)
         N     R3,=X'00FFFFFF'
         BZ    JIDOK
         MVC   SLJOBID(8,R7),SSIBJBID(R3)
JIDOK    L     R3,ANCCLAIM(,R2)
CLMLP    LR    R4,R3
         LA    R4,1(,R4)
         CS    R3,R4,ANCCLAIM(R2)
         BNE   CLMLP
         ST    R4,SLCLAIM(,R7)
*---------------------------------------------------------------------*
* The authoritative userid, from the caller's own control blocks: the
* request cannot supply or override it. R9 already holds the caller's
* key from the header range check above, and R10 its TCB.
         L     R3,ASCBASXB(,R5)
         LTR   R3,R3
         BZ    UIDOK
         L     R3,ASXBSENV(,R3)
         LTR   R3,R3
         BZ    UIDOK
*  aceeuser is LENGTH-PREFIXED: byte 0 is the length, the text starts
*  at byte 1 (libc370 ibm/mvs/ihaacee.h, char aceeuser[9]). Copying 8
*  bytes from +0 would put the length byte into user[0], shift the
*  name by one and drop its last character. The field is already
*  blank-filled, so only the declared bytes are moved.
         SLR   R1,R1
         IC    R1,ACEEUSER(,R3)
         LTR   R1,R1
         BZ    UIDOK                no userid: leave it blank
         C     R1,=A(8)
         BNH   UIDLEN
         LA    R1,8                 clamp to the slot field
UIDLEN   BCTR  R1,0                 EX wants length-1
         LA    R4,SLUSER(,R7)
         LA    R12,ACEEUSER+1(,R3)
         EX    R1,UIDMVC
UIDOK    DS    0H
*---------------------------------------------------------------------*
* MVCK the fixed 24-byte request header into key-0 slot storage before
* interpreting any fields.
*
* What makes this safe is the TPROT above, not MVCK's own key check:
* MVCK's source key only has to permit a FETCH, and a fetch with key 8
* from storage whose key differs but whose fetch-protection bit is off
* is permitted -- which is the normal state of key-0 CSA. So MVCK alone
* does not stop a forged pointer from reading key-0 storage. Requiring
* a STORE through TPROT does.
         LA    R1,24
         LR    R3,R9
         LR    R5,R8
         LA    R4,SLREQ(,R7)
         MVCK  0(R1,R4),0(R5),R3
         CLC   SLREQ(4,R7),=CL4'OFTQ'
         BNE   BADHDR
         L     R3,QVER(,R7)
         C     R3,=A(OFTVER)
         BNE   BADHDR
         L     R3,QRPLY(,R7)
         LTR   R3,R3
         BZ    BADHDR
         ST    R3,SLRPLYPT(,R7)
         L     R3,QCAP(,R7)
         LTR   R3,R3
         BNP   BADHDR
         C     R3,=A(REPMIN)
         BL    BADHDR
         C     R3,=A(REPMAX)
         BNH   CAPOK
         LA    R3,REPMAX
CAPOK    ST    R3,SLCAP(,R7)
         L     R3,QRESUM(,R7)
         ST    R3,SLRESUME(,R7)
         L     R3,QLEN(,R7)
         LTR   R3,R3
         BNP   BADHDR
         C     R3,=A(REQMAX)
         BH    BADHDR
         ST    R3,SLREQLEN(,R7)
*---------------------------------------------------------------------*
* The other two range checks, now that the header describes them: the
* request text this routine reads, and the reply buffer it will store
* into after the WAIT. Both are at most 2048 bytes and a range that
* size spans at most two 4K blocks, so its first and last byte cover
* every block in it -- which is what makes two TPROTs per range enough.
*
* Checking the reply buffer here rather than at the copy-out is worth
* a word: a refused buffer is answered before OFTPD is woken at all,
* instead of after it has built a reply and this routine has spent its
* send budget. TPROT also rejects a reply buffer the caller can read
* but not store into, which the copy-out would otherwise meet as a
* program check under SPKA.
*
* These two sit past the claim, so a refusal releases the slot
* (BADHDR). What they prevent is the program check that would not.
         LA    R4,REQTEXT(,R8)
         L     R11,SLREQLEN(,R7)
         ALR   R11,R4
         BCTR  R11,0                last byte of the request text
         BAL   R15,TPRNG
         LTR   R1,R1
         BNZ   BADHDR
         L     R4,SLRPLYPT(,R7)
         L     R11,SLCAP(,R7)
         ALR   R11,R4
         BCTR  R11,0                last byte of the reply buffer
         BAL   R15,TPRNG
         LTR   R1,R1
         BNZ   BADHDR
*---------------------------------------------------------------------*
* Copy only the declared request bytes. Append a NUL within req[256].
         LA    R5,REQTEXT(,R8)
         LA    R4,SLREQ(,R7)
         L     R1,SLREQLEN(,R7)
         LR    R3,R9
         MVCK  0(R1,R4),0(R5),R3
         L     R3,SLREQLEN(,R7)
         ALR   R4,R3
         MVI   0(R4),X'00'
*---------------------------------------------------------------------*
* Publish the complete slot only after every input field is staged.
         XC    SLRECB(4,R7),SLRECB(R7)
         LA    R3,STPEND
         ST    R3,SLSTATE(,R7)
*---------------------------------------------------------------------*
* Cross-address-space branch POST to OFTPD's private wake ECB.
* Save live registers in this invocation's SVRB extended save area.
         SLR   R9,R9
         L     R9,PSATOLD(,R9)
         L     R9,TCBRBP(,R9)
         LA    R9,RBEXSAVE(,R9)
         ST    R14,0(,R9)
         ST    R2,4(,R9)
         ST    R6,8(,R9)
         ST    R8,12(,R9)
         ST    R7,16(,R9)
         L     R11,ANCECB(,R2)       OFTPD wake ECB (key 8)
         LTR   R11,R11
         BZ    POSTBAD
         O     R11,=X'80000000'
         LA    R12,POSTERR
         L     R13,ANCSASCB(,R2)     OFTPD's ASCB
*  R10 is the POST completion code and is required input. POST does set
*  the post bit itself (IEAVSY50 ANDs OFFWAIT and ORs ONCOMPL before it
*  stores the ECB), so this is not about whether the ECB gets posted.
*  Two other things ride on R10: bits 2-31 are the code whoever waits
*  reads, and the HIGH bit asks for the ECBKEY option, with the key
*  taken from R0 byte 3 (IEAVSY50 interface notes 3 and 7, and the
*  LTR/BNM at ECBVALCK). Hence exactly X'40000000': code 0, ECBKEY off.
*  Without it R10 still held the caller's TCB address from the TCBPKF
*  load above -- bit 0 off, so posted with a garbage code.
         L     R10,=X'40000000'      POST completion code (0)
         SLR   R15,R15
         L     R15,16(,R15)          CVT
         L     R15,CVT0PT01-CVTMAP(,R15)
         BALR  R14,R15
         L     R14,0(,R9)
         L     R2,4(,R9)
         L     R6,8(,R9)
         L     R8,12(,R9)
         L     R7,16(,R9)
         B     POSTOK
POSTERR  DS    0H
         L     R14,0(,R9)
         L     R2,4(,R9)
         L     R6,8(,R9)
         L     R8,12(,R9)
         L     R7,16(,R9)
         B     POSTBAD
POSTOK   DS    0H
*---------------------------------------------------------------------*
* Wait on the key-0 slot ECB. OFTPD posts DONE, or STOP while draining.
*
* It is OFTPD's contract (internals/xmem.md section 10) to store the
* reply and the state BEFORE the POST. So any other state on wake-up
* is a protocol error, and this must NOT re-WAIT: the ECB is still
* posted, a second WAIT returns at once, and the result is a tight
* key-0 spin in the caller's address space with no way to break in.
*
* The state decides, not ACTIVE. A stop clears ACTIVE first and joins
* the slot thread after, so a request the thread was serving is
* answered DONE with ACTIVE already off: it has been carried out, and
* its answer is delivered. STOP is a slot the thread never took, so
* its request was not carried out, and RCSTOP says so (#34). The
* anchor stays while this caller is in flight, ACTIVE or not.
*
* A wake-up without our own ECB posted is not ours, and is waited out
* (#36). The ECB of a slot whose caller was cancelled still names that
* caller's SVRB, and a later job in the same initiator can get an SVRB
* at the same address: OFTPD's POST of the dead caller's ECB then ends
* THIS wait, with our slot still PENDING. Measured: a resubmitted job
* woke so and ended RC 8 while its own request was carried out. The
* complete bit of our ECB is set only by a POST of it, so waiting
* again here cannot spin.
*
* Our ECB still carries the wait bit of the first WAIT, and a WAIT on
* an ECB that is being waited on is S301 (measured: five of five). So
* the ECB goes back to 0 first, by CS against the value it holds: a
* POST landing in between changes it, the CS fails and the ECB is
* looked at again. A POST after the CS finds no waiter, sets the
* complete bit, and the WAIT returns at once.
WAITAGN  LA    R3,SLRECB(,R7)
         WAIT  1,ECB=(R3)
WAKECHK  TM    SLRECB(R7),X'40'    our own ECB posted?
         BO    WAKEOWN
         L     R3,SLRECB(,R7)      no: someone else's POST woke this RB
         SLR   R4,R4
         CS    R3,R4,SLRECB(R7)    take our wait bit back off
         BNE   WAKECHK             changed: a POST, look again
         B     WAITAGN
WAKEOWN  L     R3,SLSTATE(,R7)
         C     R3,=A(STDONE)
         BE    REPLYOK
         C     R3,=A(STSTOP)
         BE    STOPDEC             shutdown while parked
         TM    ANCFLAG(R2),X'80'
         BNO   WAKEDOWN
         B     BADSTATE            anything else: protocol error
*---------------------------------------------------------------------*
* Take the slot out of DONE and into COPYOUT before reading a field of
* it. This is the only thing that tells OFTPD's sweep apart a routine
* that woke and then died from a caller that has not been dispatched
* yet -- the second can sit in DONE for minutes, because its address
* space may be swapped out, and freeing that slot would hand it to
* another caller while its owner is still coming (xmem.md section 6).
*
* CS, not ST: if the sweep got here first the slot is no longer DONE
* and is very likely someone else's by now. Then this invocation owns
* nothing -- it must not store into the slot and must not decrement,
* because whoever freed the slot owns that decrement. SWEPT does
* neither.
REPLYOK  LA    R3,STDONE
         LA    R4,STCPYO
         CS    R3,R4,SLSTATE(R7)
         BNE   SWEPT
         L     R10,SLREPLEN(,R7)
         LTR   R10,R10
         BM    BADREPLY
         C     R10,=A(REPMAX)
         BH    BADREPLY
         L     R3,SLCAP(,R7)
         CLR   R10,R3
         BH    BADREPLY
         L     R3,SLRC(,R7)
         C     R3,=A(RCOK)
         BE    RCBOK
         C     R3,=A(RCMORE)
         BNE   BADREPLY
RCBOK    DS    0H
*---------------------------------------------------------------------*
* Copy the reply to the caller's buffer under the caller's own key.
* IPK preserves the entry PSW key for restoration after each MVC piece.
         LR    R11,R2
         IPK   0
         LR    R12,R2               restore-key SPKA operand
         LR    R2,R11
         SLR   R10,R10
         L     R10,PSATOLD(,R10)
         SLR   R9,R9
         IC    R9,TCBPKF(,R10)      caller-key SPKA operand
         L     R10,SLREPLEN(,R7)    bytes remaining
         SLR   R11,R11              offset
COPYLP   LTR   R10,R10
         BNP   COPYDN
         LR    R1,R10
         C     R1,=A(MVCMAX)
         BNH   COPYSZ
         LA    R1,MVCMAX
COPYSZ   LR    R0,R1                true piece length
         L     R4,SLRPLYPT(,R7)
         ALR   R4,R11
         LA    R5,SLREPLY(,R7)
         ALR   R5,R11
         BCTR  R1,0
         BAL   R15,MOVEOUT
         ALR   R11,R0
         SLR   R10,R0
         B     COPYLP
COPYDN   DS    0H
         L     R3,ANCSERVD(,R2)
SERVLP   LR    R4,R3
         LA    R4,1(,R4)
         CS    R3,R4,ANCSERVD(R2)
         BNE   SERVLP
         L     R0,SLREPLEN(,R7)    returned byte count
         L     R15,SLRC(,R7)
         SLR   R1,R1
         C     R15,=A(RCMORE)
         BNE   RETREL
         L     R1,SLNEXT(,R7)      resume index for next call
*---------------------------------------------------------------------*
* Release our slot, then decrement in-flight last. Request and reply
* are wiped first: the slot is readable from key 8, and once it is FREE
* another caller may be writing into it.
RETREL   BAL   R4,WIPE
         LA    R3,STFREE
         ST    R3,SLSTATE(,R7)
         L     R3,ANCINFL(,R2)
DECLP    LR    R4,R3
         BCTR  R4,0
         CS    R3,R4,ANCINFL(R2)
         BNE   DECLP
         BR    R14
*---------------------------------------------------------------------*
* Header validation failed after claim. Return BADREQ and release.
* The sweep freed our slot while we were parked: it is not ours any
* more and may already be another caller's. Touch neither the slot nor
* the in-flight count -- whoever frees a slot owns its decrement, and
* a second one here would take the count below zero and leave every
* later P OFTPD draining forever.
SWEPT    LA    R15,RCDOWN
         SLR   R0,R0
         SLR   R1,R1
         BR    R14
*---------------------------------------------------------------------*
BADHDR   LA    R15,RCBAD
         B     FREEDEC
* An unusable reply, or a wake-up in a state OFTPD should never leave:
* both are OFTPD-side protocol errors, both release the slot.
BADSTATE DS    0H
BADREPLY LA    R15,RCBAD
         SLR   R0,R0
         SLR   R1,R1
         B     FREEDEC
POSTBAD  LA    R15,RCDOWN
         SLR   R0,R0
         SLR   R1,R1
         B     FREEDEC
STOPDEC  LA    R15,RCSTOP
         SLR   R0,R0
         SLR   R1,R1
         B     FREEDEC
* Woken in a state OFTPD would not leave, with ACTIVE off: OFTPD is
* going away, which is what the caller is told.
WAKEDOWN LA    R15,RCDOWN
         SLR   R0,R0
         SLR   R1,R1
FREEDEC  BAL   R4,WIPE
         LA    R3,STFREE
         ST    R3,SLSTATE(,R7)
         L     R3,ANCINFL(,R2)
FDECLP   LR    R4,R3
         BCTR  R4,0
         CS    R3,R4,ANCINFL(R2)
         BNE   FDECLP
         BR    R14
*---------------------------------------------------------------------*
* ACTIVE went off before a slot was claimed.
DOWNDEC  LA    R15,RCDOWN
         SLR   R0,R0
         SLR   R1,R1
         B     DECNO

BUSYDEC  LA    R15,RCBUSY
         SLR   R0,R0
         SLR   R1,R1
         L     R3,ANCFULL(,R2)
BFULLLP  LR    R4,R3
         LA    R4,1(,R4)
         CS    R3,R4,ANCFULL(R2)
         BNE   BFULLLP
         B     DECNO
DECNO    L     R3,ANCINFL(,R2)
DDECLP   LR    R4,R3
         BCTR  R4,0
         CS    R3,R4,ANCINFL(R2)
         BNE   DDECLP
         BR    R14

BUSY     LA    R15,RCBUSY
         SLR   R0,R0
         SLR   R1,R1
         L     R3,ANCFULL(,R2)
BFULL2   LR    R4,R3
         LA    R4,1(,R4)
         CS    R3,R4,ANCFULL(R2)
         BNE   BFULL2
         BR    R14
DOWN     LA    R15,RCDOWN
         SLR   R0,R0
         SLR   R1,R1
         BR    R14
BADREQ   LA    R15,RCBAD
         SLR   R0,R0
         SLR   R1,R1
         BR    R14
*---------------------------------------------------------------------*
* WIPE: clear the request and the reply of our slot (SLREQ up to the
* end of the slot, nine pieces of 256 bytes). R7 = our slot, link R4.
* Clobbers R3 only; R0, R1 and R15 carry the answer: not touched.
WIPE     DS    0H
         LA    R3,SLREQ(,R7)
         XC    0(256,R3),0(R3)
         XC    256(256,R3),256(R3)
         XC    512(256,R3),512(R3)
         XC    768(256,R3),768(R3)
         XC    1024(256,R3),1024(R3)
         XC    1280(256,R3),1280(R3)
         XC    1536(256,R3),1536(R3)
         XC    1792(256,R3),1792(R3)
         XC    2048(256,R3),2048(R3)
         BR    R4
*---------------------------------------------------------------------*
* MOVEOUT: R1 = length-1, R4 = caller destination, R5 = CSA source.
* R9 and R12 are SPKA operands for caller key and entry PSW key.
MOVEOUT  DS    0H
         SPKA  0(R9)
         EX    R1,MVCPIEC
         SPKA  0(R12)
         BR    R15
MVCPIEC  MVC   0(1,R4),0(R5)
* EX target for the ACEE userid: R1 = length-1, R4 = slot.user,
* R12 = aceeuser+1. Key 0 throughout -- the ACEE is not caller input.
UIDMVC   MVC   0(1,R4),0(R12)
*---------------------------------------------------------------------*
* TPRNG: does the caller's own key permit a STORE at both ends of a
* caller-supplied range? TPROT answers without faulting, which is the
* whole point -- it turns a program check inside this routine into a
* return code.
*
* In:  R4 = first byte, R11 = last byte, R9 = the caller's key as an
*      operand (bits 24-27, straight from TCBPKF).
* Out: R1 = 0 when a store is permitted at both ends, nonzero when it
*      is not. Clobbers R1 only. Link R15.
*
* CC 0 is store permitted; 1 is fetch only, 2 no access, 3 storage not
* available. Only CC 0 passes, so mask 7 rejects the other three. A
* caller in key 0 gets CC 0 everywhere, which is correct: it could
* store there itself.
TPRNG    DS    0H
         LA    R1,1                 assume refused
         TPROT 0(R4),0(R9)
         BC    7,TPRNGX
         TPROT 0(R11),0(R9)
         BC    7,TPRNGX
         SLR   R1,R1                both ends permit a store
TPRNGX   BR    R15
*---------------------------------------------------------------------*
         LTORG ,
         CVT   DSECT=YES,LIST=NO
         END   OFTPSVC
