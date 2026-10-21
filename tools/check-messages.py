#!/usr/bin/env python3
# tools/check-messages.py [-s DIR]... REFERENCE...
# tools/check-messages.py --self-test
#
# Every message id OFTPnnnC found in the source (default: src include asm)
# must be defined in a reference file, and every id a reference defines must
# still exist in the source. Exit 0 if both hold, 1 if not, 2 on a usage error.
#
# A reference defines an id in one of two forms:
#   Markdown   a table row whose first cell is the id in backticks
#   Typst      a #msg("OFTPnnnC", ...) call
# An id that only appears in running text defines nothing.
# The source scan sees literal ids only (comments count): an id built at run
# time, as "OFTP24%cI", needs its literal in a comment beside it.
import os
import re
import sys
import tempfile

ID = r"OFTP[0-9]{3}[A-Z]"
SRC_RE = re.compile(r"(?<![A-Za-z0-9])(" + ID + r")(?![A-Za-z0-9])")
DEF_RES = (
    re.compile(r"^\|\s*`(" + ID + r")`\s*\|", re.M),
    re.compile(r"#msg\(\s*\"(" + ID + r")\""),
)
SRC_EXT = (".c", ".h", ".asm", ".mac", ".s")


def source_ids(dirs):
    found = {}
    nfiles = 0
    for d in dirs:
        for root, _, names in os.walk(d):
            for n in sorted(names):
                if not n.endswith(SRC_EXT):
                    continue
                path = os.path.join(root, n)
                nfiles += 1
                with open(path, encoding="latin-1") as f:
                    for no, line in enumerate(f, 1):
                        for m in SRC_RE.finditer(line):
                            found.setdefault(m.group(1), []).append((path, no))
    return found, nfiles


def reference_ids(paths):
    defined = {}
    for p in paths:
        with open(p, encoding="utf-8") as f:
            text = f.read()
        for rx in DEF_RES:
            for m in rx.finditer(text):
                defined.setdefault(m.group(1), []).append(p)
    return defined


def check(dirs, refs, out):
    src, nfiles = source_ids(dirs)
    ref = reference_ids(refs)
    missing = sorted(set(src) - set(ref))
    stale = sorted(set(ref) - set(src))
    dup = sorted(i for i, w in ref.items() if len(w) > 1)
    out.write("source: %d ids (%d files scanned); reference: %d ids defined\n"
              % (len(src), nfiles, len(ref)))
    for i in missing:
        path, no = src[i][0]
        out.write("MISSING from the reference: %s (%s:%d)\n" % (i, path, no))
    for i in stale:
        out.write("STALE, not in the source: %s (%s)\n" % (i, ref[i][0]))
    for i in dup:
        out.write("DEFINED TWICE: %s (%s)\n" % (i, ", ".join(ref[i])))
    return 0 if not (missing or stale or dup) and src else 1


def self_test():
    sink = open(os.devnull, "w")
    with tempfile.TemporaryDirectory() as t:
        os.mkdir(os.path.join(t, "src"))
        with open(os.path.join(t, "src", "a.c"), "w") as f:
            f.write('wto("OFTP001I READY");\n'
                    'wto("OFTP002E %s", x);\n'
                    '/* OFTP003W in a comment still counts */\n'
                    'x = "OFTP0045 is not an id, nor is XOFTP005I";\n')
        with open(os.path.join(t, "src", "b.txt"), "w") as f:
            f.write('"OFTP009E" is in a file of no source type\n')
        md = os.path.join(t, "r.md")
        ty = os.path.join(t, "r.typ")
        with open(md, "w") as f:
            f.write("| Id | Text |\n|---|---|\n"
                    "| `OFTP001I` | READY |\n"
                    "| `OFTP002E` | ERROR |\n"
                    "Prose mentions `OFTP777E` and OFTP003W only.\n")
        with open(ty, "w") as f:
            f.write('#msg("OFTP003W", [text], [cause], [action])\n')
        s = [os.path.join(t, "src")]
        cases = (
            ("complete over both forms", s, [md, ty], 0),
            ("one missing", s, [md], 1),
            ("stale entry", s, [md, ty, ty], 1),
        )
        bad = 0
        for name, d, r, want in cases:
            got = check(d, r, sink)
            if got != want:
                sys.stderr.write("self-test FAILED: %s: exit %d, want %d\n"
                                 % (name, got, want))
                bad += 1
        # exact counts, not only the exit code
        src, _ = source_ids(s)
        if sorted(src) != ["OFTP001I", "OFTP002E", "OFTP003W"]:
            sys.stderr.write("self-test FAILED: source ids %r\n" % sorted(src))
            bad += 1
        ref = reference_ids([md, ty])
        if sorted(ref) != ["OFTP001I", "OFTP002E", "OFTP003W"]:
            sys.stderr.write("self-test FAILED: reference ids %r\n" % sorted(ref))
            bad += 1
        stale = os.path.join(t, "stale.md")
        with open(stale, "w") as f:
            f.write("| `OFTP001I` | x |\n| `OFTP002E` | x |\n| `OFTP003W` | x |\n"
                    "| `OFTP100I` | gone |\n")
        if check(s, [stale], sink) != 1:
            sys.stderr.write("self-test FAILED: a stale id passed\n")
            bad += 1
    if bad:
        return 1
    print("self-test passed")
    return 0


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    dirs = []
    refs = []
    i = 0
    while i < len(argv):
        if argv[i] == "-s" and i + 1 < len(argv):
            dirs.append(argv[i + 1])
            i += 2
        elif argv[i].startswith("-"):
            sys.stderr.write(__doc__.split("\n\n")[0] + "\n")
            return 2
        else:
            refs.append(argv[i])
            i += 1
    if not refs:
        sys.stderr.write("check-messages: no reference file given\n")
        return 2
    return check(dirs or ["src", "include", "asm"], refs, sys.stdout)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
