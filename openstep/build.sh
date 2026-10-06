#!/usr/local/bin/ksh
# The supervisor bounds compiler memory and stops before swap fills the disk.
set -e
cd "$(dirname "$0")/.."
test -d openstep/output || mkdir openstep/output
if test ! -x openstep/output/build-guard || test openstep/tools/build-guard.c -nt openstep/output/build-guard; then
    gcc-4.2 -O1 -o openstep/output/build-guard openstep/tools/build-guard.c
fi
exec openstep/output/build-guard gnumake -j1 -f openstep/Makefile.openstep "$@"
