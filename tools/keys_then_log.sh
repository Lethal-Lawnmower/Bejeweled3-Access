#!/bin/sh
# Test helper: posts keys, waits, prints speech/context log lines produced
# since. Usage: keys_then_log.sh WAIT_SECONDS KEY...
L="$LOCALAPPDATA/Bejeweled3Access/mod.log"
wait=$1; shift
n=$(wc -l < "$L")
python -I "$(dirname "$0")/send_keys.py" "$@"
sleep "$wait"
tail -n +$((n + 1)) "$L" | grep -E "say|nav: context|announcement|BLOCKER|never reported" | cut -c1-600
