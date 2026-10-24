#!/usr/bin/env bash
# Boot check: owner env + KEY=VALUE lines, start, wait N s, list the game's threads, stop,
# owner env back.  boot.sh <secs> [KEY=VALUE ...]   (skipped while the game runs)
set -euo pipefail
export MSYS_NO_PATHCONV=1
S=$(dirname "$0")
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
SECS=$1; shift
LOG=${LOG:-${TMPDIR:-/tmp}/g2-boot.log}
if "$ADB" shell pidof dev.unchartedg2.game >/dev/null; then echo "game already running - skipped"; exit 1; fi
tmp=$(mktemp)
{ cat "${OWNER_ENV:-$S/../env-g2.txt}"; for kv in "$@"; do echo "$kv"; done; } > "$tmp"
bash "$S/pad.sh" start "$tmp" >/dev/null
rm -f "$tmp"
"$ADB" shell "sleep $SECS; P=\$(pidof dev.unchartedg2.game); echo threads \$(ls /proc/\$P/task | wc -l); for t in /proc/\$P/task/*; do cat \$t/comm; done | grep -E 'bb:|shadPS4|Gpu' | sort | uniq -c | tr '\n' ' '; echo"
bash "$S/pad.sh" stop "$LOG" "${OWNER_ENV:-$S/../env-g2.txt}" >/dev/null
echo "log: $LOG, $(wc -l < "$LOG") lines"
