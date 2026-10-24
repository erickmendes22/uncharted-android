#!/usr/bin/env bash
# One A/B run: owner env + extra KEY=VALUE lines, reach gameplay (nav.sh), measure, stop, restore
# the owner's env.  abrun.sh <dir> <tag> [KEY=VALUE ...]
# A game already running (the owner playing) is never touched: the run is skipped.
set -euo pipefail
export MSYS_NO_PATHCONV=1
S=$(dirname "$0")
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
DIR=$1; TAG=$2; shift 2
mkdir -p "$DIR"
if "$ADB" shell pidof dev.unchartedg2.game >/dev/null; then echo "$TAG: game already running - skipped"; exit 1; fi
{ cat "${OWNER_ENV:-$S/../env-g2.txt}"; for kv in "$@"; do echo "$kv"; done; } > "$DIR/env-$TAG.txt"
bash "$S/nav.sh" "$DIR/env-$TAG.txt" "$DIR/$TAG" > "$DIR/$TAG-nav.txt" 2>&1 || { echo "$TAG: nav failed"; exit 1; }
sleep 10
busy=$("$ADB" shell 'for i in $(seq 55); do cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage; sleep 1; done' | tr -d ' %\r' | awk '{s+=$1; n++} END {printf "%.0f", s/n}')
temps=$("$ADB" shell 'for z in /sys/class/thermal/thermal_zone*/; do t=$(cat $z/type); case $t in cpu-1-*|cpu-2-*|gpuss-0) echo "$t $(cat $z/temp)";; esac; done' | awk '{s+=$2; n++; if ($2>m) m=$2} END {printf "CPU/GPU avg %.0f max %.0f C", s/n/1000, m/1000}')
bash "$S/pad.sh" shot "$DIR/$TAG-end.png"
bash "$S/pad.sh" stop "$DIR/$TAG.log" "${OWNER_ENV:-$S/../env-g2.txt}" >/dev/null
echo "$TAG: $temps; GPU busy $busy%; $(python -I "$(cygpath -w "$S/absum.py")" "$DIR/$TAG.log")"
