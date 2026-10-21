# Maintaining the partner table

OFTPD keeps its partners in two layers.

- **`OFTPPTN`** in parmlib is the seed. OFTPD reads it at start and when
  you give `F OFTPD,REFRESH`.
- **Partners kept by command** are the ones you add, change or delete with
  the `OFTP` utility or the ISPF dialog. OFTPD keeps them in its spool, so
  they need no parmlib edit and no restart, and they survive a restart.

A partner in both layers is the one kept by command: it wins by name. A
partner that only `OFTPPTN` holds gets into both layers the first time a
command changes it (`CHANGE`, `ENABLE` or `DISABLE` that alters the entry):
OFTPD copies the entry, with your change, into its spool, and from then on
the copy wins. `PARTNER SHOW` says `kept by command, overrides OFTPPTN`.
`REFRESH` reads the parmlib layer only and leaves partners kept by command
alone. A partner you removed from `OFTPPTN` leaves the table; while a
session, a call or a transfer in the spool still holds it, it is disabled
instead.

To get the parmlib entry of a name back, delete the partner kept by
command. **That `DELETE` is followed by a full `REFRESH`, not by the reading
of that one entry.** OFTPD reads `OFTPPTN` and the receive rules
(`OFTPRCV0`) again, as for `F OFTPD,REFRESH`, and writes `OFTP016I` and
`OFTP024I` (or `OFTP025E`/`OFTP026W`). So everything that changed in the
member since the last start or `REFRESH` takes effect now, for all partners:
new entries are added, changed entries replace the entries of the table,
partners that left the member are disabled or removed, and the receive rules
are replaced. Look at the member before you delete. The answer ends with
`the OFTPPTN entry is back`; if OFTPD cannot read `OFTPPTN`, the partner is
deleted all the same and the answer says `OFTPPTN cannot be read, its entry
is back after the next REFRESH`. (That case writes no `OFTP017E`.)

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
its keyword, again never the value (message reference, `docs/books/ref`).

A value cannot hold a blank or a comma, not even in quotes: OFTPD reads the
request word by word. `PASSWORD('a b')` reaches it as `PASSWORD=A B`, and
OFTPD refuses the request with return code 8, changing nothing:

```
word 3 is not KEYWORD=VALUE
```

The number is the place of the word in the request, counted from the first
word after `PARTNER ADD` or `PARTNER CHANGE`, in the order you wrote them.
The partner name counts as a word where you wrote it: it is word 1 only when
it comes first, so in `PARTNER ADD ACME PASSWORD('a b')` the word `B` is
word 3, but in `PARTNER ADD PASSWORD('a b') ACME` it is word 2. The text
never shows the word: it may be the rest of a password.

OFTPD reads every request in upper case, so values (passwords and the
host name included) are upper case. `SHOW` never prints a password: it says
`PASSWORD=SET` or `NONE`. To change a password, give the new one. To clear a host, an address or a
`VIA`, give an empty value (`HOST()`).

`SHOW` prints `-` for `OURID` and `ADDRESS` to a caller who is not named in
`ADMIN=`; the line keeps its shape. An administrator sees both. This is
defence in depth, not confidentiality: `ODETTEID`, `HOST`, `PORT`, the
status and `PASSWORD` and `OURPASSWORD` (`SET` or `NONE`) stay visible to
everyone. `OURID` defaults to the local Odette ID, which `OFTP STATUS`
shows to everyone, so the `-` hides only an `OURID` that differs from it.
Only `PARTNER SHOW` is masked. The display of a transfer (`OFTP SHOW` of a
key, its `from ... to ...` line) shows whoever may see that transfer the
Odette IDs the transfer carries, the local one included.

**A password is as secret as the place you type it.** In batch `OFTP`
echoes each command of `SYSIN` to `SYSPRINT` and prints the value after
`PASSWORD` and after `OURPASSWORD` as eight stars (`PASSWORD(********)`),
whatever its length. The echo masks one word: the value ends at the first
blank that is not inside quotes. So it hides a password written as one word
(`PASSWORD(x)`, `PASSWORD=x`) or in quotes (`PASSWORD('x y')`). A password
with a blank in it must therefore be quoted. Unquoted, `PASSWORD(ab cd)` is
echoed as `PASSWORD(******** cd)`: the rest of the password stands in
`SYSPRINT`. (`OFTP` then refuses the command, `OFTP007E` above, but the echo
has been written by then.) A password in quotes is masked whole, yet OFTPD
still refuses it, because a value cannot hold a blank (above). The echo
looks for the text `PASSWORD` in any case and anywhere in the line, so a
keyword that ends in it is masked as well. The echo is cut into lines of at
most 72 columns; a line that goes on ends in ` -`, as it does in `SYSIN`.
But the password is in your JCL, and under TSO it is on your terminal and
in your session's history. Keep the JCL in a data set only you can read.

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
it erases every slot, except when the stop cannot finish cleanly: if a
caller is still inside OFTPD's routine after the 10-second drain
(`OFTP037W ... CSA RETAINED`), or the SVC entry cannot be given back
(`OFTP036W`), the slots stay as they are.

### A partner without a PASSWORD

A partner without `PASSWORD` is allowed. OFTPD then expects the partner to
send an empty password, so every peer that knows the Odette ID of the
partner and sends an empty password logs on. A peer that sends a password is
refused. With an `ADDRESS` in the entry the call must also come from that
address (`OFTP201W` otherwise); without one it may come from anywhere.

This holds in both directions. When OFTPD calls the partner, the partner's
answer must carry an empty password too, so a partner that insists on a
password while its entry has none fails the call. `PASSWORD` is what OFTPD
expects, `OURPASSWORD` what it sends.

A partner with `VIA` is never matched by its own Odette ID at log-on, so it
cannot log on itself. For such a partner the warning below is only a hint.

`ADD` and `CHANGE` warn when an enabled partner has no `PASSWORD` after the
command. `ENABLE` is a `CHANGE` of `ENABLED`, so enabling a partner without
a password warns as well.

```
OFTP PARTNER ADD DOCPTN ODETTEID(O0000000000DOCP) HOST(127.0.0.1) -
     CALL(N)
WARNING: without a PASSWORD every peer that knows the Odette ID of the partner can log on
partner DOCPTN added, but it has no PASSWORD
```

The warning is no refusal: the partner is added or changed. OFTPD answers
with the code `210` where it would answer `200`, and the batch `OFTP` step
ends with RC 4. The warning has two texts. It says what the partner
allows: with no `ADDRESS` in the entry any peer that knows the Odette ID,
with an `ADDRESS` only a peer that calls from that address, because the
address check still applies. The `210` line is the same in both cases.

```
no ADDRESS:  100 WARNING: without a PASSWORD every peer that knows the Odette ID of the partner can log on
ADDRESS set: 100 WARNING: without a PASSWORD every peer calling from the ADDRESS of the partner that knows the Odette ID can log on
```

(`OFTP` prints the text after the code, as in the example above.)

There is no warning when the partner has a `PASSWORD` after the command
(`CHANGE ... PASSWORD(...)` answers a plain `partner DOCPTN changed`, RC 0),
when the partner is disabled, and when the command changes nothing
(`unchanged`).

A partner from the `OFTPPTN` member without `PASSWORD` is just as open and
gets no warning: reading the member, at start or at `REFRESH`, is not a
command. Check the member for entries without `PASSWORD`.

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
| `PARTNER=` missing, longer than 8, or with a character that is not a letter, digit, `#`, `@`, `$`, `_` or `-` | `PARTNER= is a name of 1 to 8 letters, digits, # @ $ _ -` | 8 |
| `ADD` of a name that exists | `partner exists: use CHANGE` | 8 |
| `ADD` when the table holds 32 partners | `the partner table is full` | 8 |
| a keyword that is none, or a value that does not fit | for example `unknown keyword TOKEN`, `invalid value for TOKEN`, `ODETTEID is required` | 8 |
| a word that is not `KEYWORD=VALUE`, such as the second half of a value with a blank | `word n is not KEYWORD=VALUE`, `n` the place of the word | 8 |
| `CHANGE`, `ENABLE`, `DISABLE` or `DELETE` of a name that is not there | `OFTP010E no such partner` | 12 |
| `CHANGE` while a session with the partner runs (or OFTPD is calling it); only `ENABLE` and `DISABLE` are taken at once | `the partner has a session now, try again` | 4 |
| `DELETE` while a session with it runs | `the partner has a session now, try again` | 4 |
| `CHANGE`, `ENABLE` or `DISABLE` that changes nothing (the partner is as asked) | `partner NAME unchanged` | 0 |
| `DISABLE` of a partner that an enabled partner is reached through (`VIA`) | `partner X is reached through it: disable that first` | 8 |
| `DELETE` of a partner that another partner is reached through | `partner X is reached through it` | 8 |
| `DELETE` of a partner that is only in `OFTPPTN` | `the partner is in OFTPPTN: remove it there, then REFRESH` | 8 |
| `DELETE` while the spool holds a transfer for, to or from it | `n transfers name partner NAME: PURGE or DELETE them first`, with ` (disabled)` directly after the name when this `DELETE` disabled it | 8 |
| `DELETE` when the partner changed while OFTPD looked through the spool | `the partner was changed meanwhile, try again` | 4 |
| any request that gets past the userid and the name check, when OFTPD cannot use its spool | `spool not available` | 4 |
| `SEND` to a disabled partner | `OFTP010E unknown or disabled partner` | 12 |
| `ADD`, `CHANGE` or `ENABLE` that changes an entry and leaves the enabled partner without `PASSWORD` | **taken**: `partner NAME added, but it has no PASSWORD` (or `changed, ...`), after the warning line | 4 |

`X` is the partner that is reached through the one you named. The
dialog shows the same text, without `OFTP010E`.

RC 4 has two meanings: the request was not done and may be tried again, or
it was done with a warning. The last row of the table is the second case.

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
| `S` | select it for a send (only in the list that the Send panel opens, below) |

`S` works only in the partner list that the *Send* panel opens when you type
`?` in its *Partner* field. There `S` puts the name into the *Partner* field
and returns to the Send panel, and `C` calls the partner as usual. `M`, `E`,
`X` and `D` change nothing there (the message line says `S selects a partner,
C calls it`), and a name in *New partner* is refused (`Partners are added from
option 3 of the menu`). In option 3 `S` does nothing: the message line says
`Line commands: C call, M modify, E enable, X disable, D delete`. The panel
shows the same list of letters in both cases.

The entry panel shows both passwords as `(set)` or `(none)`. A field you
leave as it is stays as it is; type a new value into a password input
field to replace the password, and leave it blank to keep it. The new partner starts with level 5, mode both, no compression,
may be called, one session and enabled. The refusals above come back as the
panel's message.

When OFTPD takes the entry, the entry panel ends and the answer stands in
the message line of the partner list. Any `2xx` answer counts as taken,
`210` included (`partner PARTNER2 added, but it has no PASSWORD`). The
dialog shows only that last line, not the `WARNING:` line before it. The
line commands `E` and `X` show their answer in the same place. Any answer
that is not `2xx` keeps the entry panel open: a refusal, OFTPD busy or not
reached, or an answer that ends before its last line (`incomplete answer
from OFTPD`), which carries no code.

The panels are made for the 43-row screen (`docs/installation.md`, "What you need").
