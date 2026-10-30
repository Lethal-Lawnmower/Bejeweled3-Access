#!/bin/sh
# Test helper: presses Down until the mod speaks an item starting with TEXT
# (max 30 presses). Usage: focus_item.sh "RECORDS" [key, default down; use tab on a board]
L="$LOCALAPPDATA/Bejeweled3Access/mod.log"
for i in $(seq 1 30); do
  n=$(wc -l < "$L")
  python -I "$(dirname "$0")/send_keys.py" "${2:-down}"
  sleep 0.5
  if tail -n +$((n + 1)) "$L" | grep -q "say! $1"; then echo "focused $1"; exit 0; fi
done
echo "not found: $1"; exit 1
