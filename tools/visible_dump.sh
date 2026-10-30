#!/bin/sh
# Triggers an F12 widget dump and prints only visible widgets from it.
L="$LOCALAPPDATA/Bejeweled3Access/mod.log"
n=$(wc -l < "$L")
python -I "$(dirname "$0")/send_keys.py" f12
sleep 1
tail -n +$((n + 1)) "$L" | grep "dump:" | grep -vE "vis=0" | grep -vE "^\S+ dump:\s+(header|lines|footer|text): $"
