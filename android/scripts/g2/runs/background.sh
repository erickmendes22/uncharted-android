#!/usr/bin/env bash
# background.sh <tag> home|sleep [secs]: the app in the background in U2 from the owner's
# checkpoint (u2_ab.sh with BB_AUDIO_STATS=1): 20 s of play, then <secs> (default 30) on the home
# screen (back through am start) or with the sleep button (back through the wake key), 30 s more.
# Prints the lifecycle lines and port 0's audio queue: right after the return its minimum should
# be tens of KB again (11 MB, 28.7 s of sound, after 30 s on the home screen before port 0078).
# SAVES_BACKUP (required) and OUT: as for u2_ab.sh.
set -uo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
export OUT=${OUT:-${TMPDIR:-/tmp}/u2-ab}
: "${SAVES_BACKUP:?SAVES_BACKUP=<saves backup directory in files/>}"
export SAVES_BACKUP
tag=$1 mode=$2 secs=${3:-30}
"$ADB" shell pidof dev.unchartedg2.game > /dev/null && { echo "the dev game is running: not starting"; exit 1; }
bash "$R/u2_ab.sh" start "$tag" BB_AUDIO_STATS=1 2>&1 | grep -v pushed
P="bash $R/pad.sh"
sleep 45; $P shot "$OUT/$tag-title.png"; $P press cross; sleep 4; $P press cross; sleep 50
$P shot "$OUT/$tag-dlg.png"; $P press cross; sleep 55; $P shot "$OUT/$tag-cp.png"
sleep 20
if [ "$mode" = home ]; then
    "$ADB" shell input keyevent KEYCODE_HOME; sleep "$secs"
    "$ADB" shell am start -n dev.unchartedg2.game/.GameActivity > /dev/null
else
    "$ADB" shell input keyevent KEYCODE_SLEEP; sleep "$secs"
    "$ADB" shell input keyevent KEYCODE_WAKEUP; sleep 1; "$ADB" shell wm dismiss-keyguard
fi
sleep 30; $P shot "$OUT/$tag-after.png"
bash "$R/u2_ab.sh" stop "$tag" 2>&1 | tail -1
grep -E "^Window: the app|^Audio: |^Audio stats port 0" "$OUT/$tag.log" | tail -24
