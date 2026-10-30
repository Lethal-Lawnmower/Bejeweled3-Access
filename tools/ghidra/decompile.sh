#!/bin/sh
# Usage: tools/ghidra/decompile.sh OUT.c ADDR...
# Decompiles every function referencing each address (or containing it),
# read-only. Set GHIDRA_PROJECT to a project folder under private/ to use a
# copy (parallel runs must each use their own copy: Ghidra locks projects).
# GHIDRA_HOME: Ghidra install folder (tested with 12.0.4).
ROOT=$(cd "$(dirname "$0")/../.." && pwd -W)
G="${GHIDRA_HOME:?set GHIDRA_HOME to the Ghidra install folder}/support/analyzeHeadless.bat"
P=${GHIDRA_PROJECT:-"$ROOT/private/ghidra"}
out=$(cygpath -w "$1"); shift
"$G" "$P" Bej3 -process Bejeweled3.exe -noanalysis -readOnly \
  -scriptPath "$ROOT/tools/ghidra" \
  -postScript DecompileRefs.java "$out" "$@" > "$(cygpath -u "$P")/last-run.log" 2>&1
