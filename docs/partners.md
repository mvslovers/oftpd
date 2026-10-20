# Maintaining the partner table

OFTPD keeps its partners in two layers.

- **`OFTPPTN`** in parmlib is the seed. OFTPD reads it at start and when
  you give `F OFTPD,REFRESH`.
- **Partners kept by command** are the ones you add, change or delete with
  the `OFTP` utility or the ISPF dialog. OFTPD keeps them in its spool, so
  they need no parmlib edit and no restart, and they survive a restart.

A partner in both layers is the one kept by command: it wins by name.
`REFRESH` reads the parmlib layer only and leaves partners kept by command
alone. To get the parmlib entry of a name back, delete the partner kept by
command; the entry in `OFTPPTN` is in force again at once.

Anyone may look at the table (`PARTNERS`, `PARTNER SHOW`). Adding,
changing, enabling, disabling and deleting is for the userids named in
`ADMIN=` in `OFTPPRM0`; anyone else gets RC 8 (`needs ADMIN=`).

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
fit, is refused (RC 8) and nothing is changed.

OFTPD reads every request in upper case, so values (passwords and the
host name included) are upper case. `SHOW` never prints a password: it says
`PASSWORD=SET` or `NONE`. To change a password, give the new one. To clear a host, an address or a
`VIA`, give an empty value (`HOST()`).

**A password is as secret as the place you type it.** `OFTP` masks it in
the echo of `SYSIN`, but it is in your JCL, and under TSO it is on your
terminal and in your session's history. Keep the JCL in a data set only you
can read.

### What is refused

| Request | Answer |
|---|---|
| `ADD` of a name that exists | RC 8, use `CHANGE` |
| `CHANGE` or `DELETE` of a name that is not there | RC 12 |
| `CHANGE` while a session with the partner runs (or OFTPD is calling it) | RC 4, try again; only `ENABLE` and `DISABLE` are taken at once |
| `CHANGE` that changes nothing | RC 8 |
| `DISABLE` or `DELETE` of a partner that another partner is reached through (`VIA`) | RC 8 |
| `DELETE` of a partner that is only in `OFTPPTN` | RC 8: remove it from the member and `REFRESH` |
| `DELETE` while a session with it runs | RC 4, try again |
| `DELETE` while the spool holds a transfer for, to or from it | RC 8 |
| `SEND` to a disabled partner | RC 12, "unknown or disabled partner" |

**A partner with transfers in the spool is never deleted.** That counts
every transfer, finished ones too (a finished transfer is the record of what
was sent). `DELETE` first **disables** the partner, so that nothing new is
queued while you decide, and then refuses with the number of transfers it
found. Delete or `PURGE` those transfers (`OFTP LIST PARTNER(name)`,
`OFTP DELETE key`, `OFTP PURGE DAYS(0)`), then `DELETE` the partner again.
Disabling a partner keeps its queue; enable it to carry on.

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
