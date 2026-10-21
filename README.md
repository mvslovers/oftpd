# OFTPD

OFTPD (oftpd) is an OFTP (ODETTE File Transfer Protocol) server and client
for MVS 3.8j:
a monitor that runs as a started task, plus TSO and batch utilities.

It speaks OFTP 2.0 without its security features, and OFTP 1.4 and 1.3,
over plain TCP/IP. It is meant for hobbyist and test systems, not for
production use.

Status: 1.0.0, the first final release. Release notes: `CHANGELOG.md`.

## Building

Needs [mbt](https://github.com/mvslovers/mbt) 3 and the cc370 toolchain.

```
mbt build
mbt test
```

## License

MIT, see `LICENSE`.
