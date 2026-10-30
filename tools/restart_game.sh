#!/bin/sh
# Closes Bejeweled 3 gracefully (WM_CLOSE), installs the current build and
# starts the game through Steam. Waits until the mod reports its first screen.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
L="$LOCALAPPDATA/Bejeweled3Access/mod.log"
python -I -c "
import ctypes; u=ctypes.windll.user32; h=u.FindWindowW('MainWindow',None)
h and u.PostMessageW(h,0x10,0,0)"
for i in $(seq 1 30); do tasklist | grep -qi bejeweled3 || break; sleep 1; done
sh "$ROOT/tools/install.sh"
rm -f "$L"
cmd //c start "" "steam://rungameid/78000"
for i in $(seq 1 60); do grep -q "nav: context" "$L" 2>/dev/null && break; sleep 1; done
sleep 2
grep -v "dump:" "$L"
