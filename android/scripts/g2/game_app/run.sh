#!/usr/bin/env bash
# A run of the APK dev.unchartedg2.game on the G2: env.txt from the arguments, start, wait, fetch the logs.
#   scripts/g2/game_app/run.sh <secs> <output dir> [VAR=value ...]
# In the directory: bb-probe.log (the game's log) and logcat.txt (all of logcat during the run).
set -euo pipefail
export MSYS_NO_PATHCONV=1  # otherwise Git Bash turns /sdcard/... into a Windows path
SECS=${1:?seconds}; OUT=${2:?directory}; shift 2
ADB=${ADB:-$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)/adb.exe}
PKG=dev.unchartedg2.game
FILES=/sdcard/Android/data/$PKG/files
mkdir -p "$OUT"
: > "$OUT/env.txt"
for kv in "$@"; do echo "$kv" >> "$OUT/env.txt"; done
"$ADB" push "$OUT/env.txt" $FILES/env.txt >/dev/null
"$ADB" shell am force-stop $PKG
"$ADB" shell rm -f $FILES/bb-probe.log
"$ADB" logcat -c
"$ADB" logcat -v threadtime > "$OUT/logcat.txt" &
LOGCAT=$!
"$ADB" shell am start -n $PKG/.GameActivity >/dev/null
sleep "$SECS"
"$ADB" shell am force-stop $PKG
kill $LOGCAT 2>/dev/null || true
"$ADB" pull $FILES/bb-probe.log "$OUT/bb-probe.log" >/dev/null || true
echo "$OUT: $(wc -l < "$OUT/bb-probe.log" 2>/dev/null || echo 0) log lines, $(wc -l < "$OUT/logcat.txt") logcat lines"
