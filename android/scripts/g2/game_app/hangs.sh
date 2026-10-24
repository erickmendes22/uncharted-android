#!/usr/bin/env bash
# A run.sh run + counting GPU hangs from dmesg (kgsl "GPU hang detected") during the run.
#   scripts/g2/game_app/hangs.sh <secs> <output dir> [VAR=value ...]
# dmesg on the G2 is readable without root (SELinux permissive). Saves <dir>/dmesg.txt.
set -euo pipefail
export MSYS_NO_PATHCONV=1
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=${ADB:-$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)/adb.exe}
OUT=${2:?directory}
T0=$("$ADB" shell cat /proc/uptime | cut -d' ' -f1)
bash "$HERE/run.sh" "$@" | tail -1
"$ADB" shell dmesg | awk -v t="$T0" '{s=$1; gsub(/\[|\]/,"",s); if (s+0>t) print}' > "$OUT/dmesg.txt"
echo "GPU hangs: $(grep -c 'hang detected' "$OUT/dmesg.txt" || true); $(grep -m1 -E 'GPU crash: the' "$OUT/bb-probe.log" || echo 'no device loss')"
