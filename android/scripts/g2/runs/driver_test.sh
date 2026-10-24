#!/usr/bin/env bash
# driver_test.sh <tag> zip:<path on device>|turnip|system [install] [KEY=VALUE ...]: the public app
# with that Vulkan driver: the launcher gets the driver (driver_zip or driver extra), a screenshot of
# it, Play, 70 s, a screenshot of the game and the driver / frame rate lines of the log, which is
# pulled to OUT (default $TMPDIR/drivers). KEY=VALUE pairs go into the launcher's extra environment;
# install: APK=<file> is installed first.
set -uo pipefail
export MSYS_NO_PATHCONV=1
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
P=dev.unchartedandroid; G=/sdcard/Android/data/$P/files; OUT=${OUT:-${TMPDIR:-/tmp}/drivers}; mkdir -p "$OUT"
tag=$1 spec=$2; shift 2
install=0; [ "${1:-}" = install ] && { install=1; shift; }
# A game writes its log every few seconds: a process with a stale log is a dormant one (Android
# keeps an empty process after a force-stop), not the owner playing.
if "$ADB" shell pidof $P > /dev/null; then
    age=$("$ADB" shell 'echo $(( $(date +%s) - $(stat -c %Y '"$G"'/bb-probe.log) ))' | tr -d '\r')
    [ "$age" -lt 30 ] && { echo "the public app is running and writing its log ($age s ago): abort"; exit 1; }
    "$ADB" shell am force-stop $P; sleep 1
fi
[ $install = 1 ] && "$ADB" install -r "$(cygpath -w "${APK:?APK=<the public APK to install>}")" 2>&1 | tail -1
"$ADB" shell input keyevent KEYCODE_WAKEUP
case "$spec" in
    zip:*) extra="--es driver_zip ${spec#zip:}" ;;
    *) extra="--es driver $spec" ;;
esac
# the device's shell eats one backslash: two of them arrive as "\n", which the launcher turns into a line break
env=""; for kv in "$@"; do env="$env$kv\\\\n"; done
[ -n "$env" ] && extra="$extra --es extra_env $env"
"$ADB" shell am start -n $P/dev.unchartedg2.game.Launcher $extra > /dev/null; sleep 6
"$ADB" shell input swipe 960 900 960 300 400; sleep 2
"$ADB" exec-out screencap -p > "$OUT/$tag-launcher.png"
"$ADB" shell am start -n $P/dev.unchartedg2.game.Launcher --ez play true > /dev/null; sleep 70
"$ADB" exec-out screencap -p > "$OUT/$tag-game.png"
pid=$("$ADB" shell pidof $P | tr -d '\r')
"$ADB" shell "grep -E '^(BB_VK|TU_)' $G/env.txt; grep -E 'Vulkan: custom driver' $G/bb-probe.log | head -2; grep -E 'Frame rate' $G/bb-probe.log | tail -2"
[ -n "$pid" ] && "$ADB" shell "logcat -d -t 30000 | grep -E '\($pid\)' | grep -i -E 'AdrenoVK|adrenotools|hook|freedreno|Driver Path' | head -4 | cut -c1-200" || echo "GAME NOT RUNNING"
"$ADB" shell am force-stop $P
"$ADB" shell "cp $G/bb-probe.log /sdcard/Download/drv-$tag.log"
"$ADB" pull /sdcard/Download/drv-$tag.log "$(cygpath -w $OUT/$tag.log)" > /dev/null; "$ADB" shell rm -f /sdcard/Download/drv-$tag.log
echo DRIVER-TEST-DONE
