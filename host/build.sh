#!/bin/sh
# Builds host/oftph, the session engine on the workstation, into build/host/.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
${CC:-cc} -std=gnu99 -Wall -Wextra -Werror -Iinclude -o build/host/oftph \
    host/oftph.c 'src/oftp#cmd.c' 'src/oftp#dat.c' 'src/oftp#fld.c' \
    'src/oftp#opr.c' 'src/oftp#ses.c' 'src/oftp#xlt.c'
echo build/host/oftph
