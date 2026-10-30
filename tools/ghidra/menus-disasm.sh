#!/bin/sh
# Usage: tools/ghidra/menus-disasm.sh OUT.txt ITEM...
# Runs MenusDisasm.java (read-only) on the menus agent's Ghidra copy.
# Items: ADDR (whole function), ADDR:N (N instructions), callers:ADDR.
# GHIDRA_HOME: Ghidra install folder (tested with 12.0.4).
ROOT=$(cd "$(dirname "$0")/../.." && pwd -W)
G="${GHIDRA_HOME:?set GHIDRA_HOME to the Ghidra install folder}/support/analyzeHeadless.bat"
P=${GHIDRA_PROJECT:-"$ROOT/private/ghidra-menus"}
out=$(cygpath -w "$1"); shift
"$G" "$P" Bej3 -process Bejeweled3.exe -noanalysis -readOnly \
  -scriptPath "$ROOT/tools/ghidra" \
  -postScript MenusDisasm.java "$out" "$@" > "$(cygpath -u "$P")/last-run.log" 2>&1
