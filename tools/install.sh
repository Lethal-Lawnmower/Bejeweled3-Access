#!/bin/sh
# Copies the built mod into the Steam game folder (adds two files, overwrites
# nothing of the game's). Uninstall: tools/install.sh --remove
set -e
GAME="/c/Program Files (x86)/Steam/steamapps/common/Bejeweled 3"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [ "$1" = "--remove" ]; then
  rm -f "$GAME/winmm.dll" "$GAME/prism.dll"
  echo "removed mod files"
  exit 0
fi
cp "$ROOT/build/winmm.dll" "$GAME/winmm.dll"
cp "$ROOT/deps/prism-x86/bin/prism.dll" "$GAME/prism.dll"
echo "installed winmm.dll and prism.dll into $GAME"
