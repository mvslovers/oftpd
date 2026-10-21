# Maintaining the partner table

OFTPD keeps its partners in two layers.

- **`OFTPPTN`** in parmlib is the seed. OFTPD reads it at start and when
  you give `F OFTPD,REFRESH`.
- **Partners kept by command** are the ones you add, change or delete with
  the `OFTP` utility or the ISPF dialog. OFTPD keeps them in its spool, so
  they need no parmlib edit and no restart, and they survive a restart.

A partner in both layers is the one kept by command: it wins by name.
`REFRESH` reads the parmlib layer only and leaves partners kept by command
alone. A partner you removed from `OFTPPTN` leaves the table; while a
session, a call or a transfer in the spool still holds it, it is disabled
instead. To get the parmlib entry of a name back, delete the partner kept by
command; the entry in `OFTPPTN` is in force again at once.

**Protect the spool like `OFTPPTN`.** A partner kept by command is stored
in the spool with everything you gave it, **both passwords in clear text**,
exactly as `OFTPPTN` holds them. The spool cluster, and every backup, copy
or dump of it, therefore needs the same protection as the parmlib member:
give it a profile that only OFTPD's userid and the administrators may read.
This is also why `PARTNER SHOW` never prints a password.

Anyone may look at the table (`PARTNERS`, `PARTNER SHOW`). Adding,
changing, enabling, disabling and deleting is for the userids named in
`ADMIN=` in `OFTPPRM0`; anyone else is refused with a message that begins
`needs ADMIN=` (return code 8, see "What is refused").

## From the OFTP utility

In batch the commands go into `SYSIN` (`OFTPD.SAMPLIB(OFTPBAT)` shows the
frame); under TSO they follow `OFTP`.

```
PARTNER LIST
PARTNER SHOW PARTNER1
PARTNER ADD PARTNER2 ODETTEID(O0000000000PARTNER2) -
        PASSWORD(SECRET1) OURPASSWORD(SECRET2) -
        HOST(partner2.example.org) PORT(3305) ENABLED(N)
PARTNER CHANGE PARTNER2 PORT(3306) SESSIONS(2)
PARTNER ENABLE PARTNER2
PARTNER DISABLE PARTNER2
PARTNER DELETE PARTNER2
```

`ADD` and `CHANGE` take the keywords of `OFTPPTN0`: `ODETTEID` (required for
`ADD`), `PASSWORD`, `OURPASSWORD`, `OURID`, `HOST`, `PORT`, `LEVEL`, `MODE`,
`COMPRESS`, `CALL`, `SESSIONS`, `ADDRESS`, `VIA`, `XLATE`, `SDEB`, `CREDIT`
and `ENABLED`. `KEYWORD=VALUE` works as well as `KEYWORD(VALUE)`. `CHANGE`
alters only what you name. A keyword that is none, or a value that does not
fit, is refused (return code 8) and nothing is changed.

A word of more than 99 characters is never cut. In a line of `OFTPPTN` it
is refused with `OFTP010W OFTPPTN: line n: a word is longer than 99
characters`, and the partner entry being read is disabled. `OFTP` does not
get that far: it refuses an operand much earlier (`OFTP003E`, below).

Write every value after `=` or in parentheses. A word without one, such as
`PARTNER CHANGE ACME OURPASS`, is refused by `OFTP` itself, before OFTPD
sees it, with return code 8:

```
OFTP007E OPERAND 2 HAS NO VALUE: WRITE KEYWORD(VALUE)
```

The number is the place of the word, counted from the first word after
`PARTNER CHANGE`. The message never shows the word, because it may be a
password. `XLATE` alone is taken as `XLATE(Y)`. An operand whose value is
longer than 79 characters is refused with `OFTP003E OPERAND TOO LONG:` and
its keyword, again never the value (`docs/messages.md`).

OFTPD reads every request in upper case, so values (passwords and the
host name included) are upper case. `SHOW` never prints a password: it says
`PASSWORD=SET` or `NONE`. To change a password, give the new one. To clear a host, an address or a
`VIA`, give an empty value (`HOST()`).

**A password is as secret as the place you type it.** In batch `OFTP`
echoes each command of `SYSIN` to `SYSPRINT` and prints the value after
`PASSWORD` and after `OURPASSWORD` as eight stars (`PASSWORD(********)`),
whatever its length. It does so in every way the command can be written:
`PASSWORD(x)`, `PASSWORD=x`, `PASSWORD('x y')`, and with blanks between the
keyword and the value (`PASSWORD x`, `PASSWORD (x)`, `PASSWORD = x`). The
echo looks for the text `PASSWORD` in any case and anywhere in the line,
so a keyword that ends in it is masked as well. A `PASSWORD` that stands
alone masks the word after it, whatever that word is. The echo is cut into
lines of at most 72 columns; a line that goes on ends in ` -`, as it does in
`SYSIN`. But the password is in your JCL, and under TSO it is on your
terminal and in your session's history. Keep the JCL in a data set only you
can read.

A password in a `PARTNER` command also travels through common storage
(CSA): `OFTP` hands the request to OFTPD in a slot there, and OFTPD
erases the slot as soon as it has copied the request. Common storage is
not fetch-protected: a program of any user can read it, though not change
it (measured on MVS/CE; not on TK5). Until OFTPD has copied the request,
any program on the system could therefore read the password. That window
is short, but it is there; it is a limit of the way the utilities reach
OFTPD, not something a setting closes.
A request that nobody takes, or an answer that nobody collects, stays in
the slot until its caller leaves or the slot is freed. Whenever a slot is
freed, its request and its answer are erased first, and when OFTPD stops
it erases every slot.

### What is refused

`OFTP` prints the text of OFTPD's final answer and nothing else: not the
three-digit number OFTPD sends in front of it, and not a return code. The
return code is what the command ends with (in batch the condition code of
the step, the highest of all commands in `SYSIN`; the ISPF dialog shows the
text only). It follows the answer: 0 for done, 4 for "try again" and for an
answer that is a warning, 8 for a refusal, 12 for an unknown partner or
OFTPD not reached. An answer with return code 12 is printed after
`OFTP010E`.

| Request | Text shown | Return code |
|---|---|---|
| any change by a userid not in `ADMIN=` | `needs ADMIN= in OFTPPRM` | 8 |
| `ADD` of a name that exists | `partner exists: use CHANGE` | 8 |
| `ADD` when the table holds 32 partners | `the partner table is full` | 8 |
| a keyword that is none, or a value that does not fit | for example `unknown keyword TOKEN`, `invalid value for TOKEN`, `ODETTEID is required` | 8 |
| `CHANGE`, `ENABLE`, `DISABLE` or `DELETE` of a name that is not there | `OFTP010E no such partner` | 12 |
| `CHANGE` while a session with the partner runs (or OFTPD is calling it); only `ENABLE` and `DISABLE` are taken at once | `the partner has a session now, try again` | 4 |
| `DELETE` while a session with it runs | `the partner has a session now, try again` | 4 |
| `CHANGE`, `ENABLE` or `DISABLE` that changes nothing (the partner is as asked) | `partner NAME unchanged` | 0 |
| `DISABLE` of a partner that an enabled partner is reached through (`VIA`) | `partner X is reached through it: disable that first` | 8 |
| `DELETE` of a partner that another partner is reached through | `partner X is reached through it` | 8 |
| `DELETE` of a partner that is only in `OFTPPTN` | `the partner is in OFTPPTN: remove it there, then REFRESH` | 8 |
| `DELETE` while the spool holds a transfer for, to or from it | `n transfers name partner NAME: PURGE or DELETE them first`, with `(disabled)` after the name when this `DELETE` disabled it | 8 |
| `DELETE` when the partner changed while OFTPD looked through the spool | `the partner was changed meanwhile, try again` | 4 |
| any of the above when OFTPD cannot use its spool | `spool not available` | 4 |
| `SEND` to a disabled partner | `OFTP010E unknown or disabled partner` | 12 |

`X` is the partner that is reached through the one you named. The
dialog shows the same text, without `OFTP010E`.

**A partner with transfers in the spool is never deleted.** That counts
every transfer, finished ones too (a finished transfer is the record of what
was sent). `DELETE` first **disables** the partner, so that nothing new is
queued while you decide, and then refuses with the number of transfers it
found. Delete or `PURGE` those transfers (`OFTP LIST PARTNER(name)`,
`OFTP DELETE key`, `OFTP PURGE DAYS(0)`), then `DELETE` the partner again.
Disabling a partner keeps its queue; enable it to carry on.

OFTPD looks through the spool without holding up the sessions of other
partners. If the partner changes meanwhile (a session or a call starts,
someone enables it, another partner is now reached through it, or the entry
is a different one), `DELETE` deletes nothing and answers `the partner was
changed meanwhile, try again` (return code 4). A partner that this `DELETE`
disabled stays disabled, and the refusal does not enable it again. Give the
command again.

OFTPD writes the console messages `OFTP243I` (added), `OFTP244I` (changed)
and `OFTP245I` (deleted) with the userid, and at start `OFTP018I` with the
number of partners kept by command.

## From the ISPF dialog

Option 3, *Partners*, lists the table. Type a name into *New partner* to add
one; the entry panel follows. Line commands on a partner:

| | |
|---|---|
| `M` | modify: the entry panel with the values as they are |
| `E`, `X` | enable, disable |
| `D` | delete: asks for confirmation first |
| `C` | call the partner now |
| `S` | select it for a send |

The entry panel shows both passwords as `(set)` or `(none)`. A field you
leave as it is stays as it is; type a new value into a password input
field to replace the password, and leave it blank to keep it. The new partner starts with level 5, mode both, no compression,
may be called, one session and enabled. The refusals above come back as the
panel's message.

The panels are made for the 43-row screen (`docs/installation.md`, "What you need").
