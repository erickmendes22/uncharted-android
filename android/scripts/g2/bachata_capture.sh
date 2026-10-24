#!/usr/bin/env bash
# Bachata S4 metrics on the G2: bachata_capture.sh start | stop [name] | status
# start runs bachata_cap_device.sh on the device in the background; stop stops it and fetches
# the data into notes/g2-bachata/<name>/ (metrics and logs only, no game files).
set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
pt=$(ls -d "$(cygpath "$LOCALAPPDATA")"/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)
[ -n "$pt" ] && export PATH="$PATH:$pt"
export MSYS_NO_PATHCONV=1
DEV=/data/local/tmp/bcap
case "${1:-}" in
start)
    adb push "$(cygpath -w "$here/bachata_cap_device.sh")" /data/local/tmp/bachata_cap_device.sh > /dev/null
    adb shell "chmod 755 /data/local/tmp/bachata_cap_device.sh; rm -rf $DEV; mkdir -p $DEV; \
        nohup /data/local/tmp/bachata_cap_device.sh $DEV > $DEV/cap.log 2>&1 < /dev/null &"
    echo "capture started"
    ;;
stop)
    name=${2:-$(date +%Y%m%d-%H%M%S)}
    dst="$repo/notes/g2-bachata/$name"
    adb shell "touch $DEV/stop"; sleep 3
    mkdir -p "$dst"
    adb pull "$DEV/." "$(cygpath -w "$dst")" > /dev/null
    echo "pulled to $dst"; ls -la "$dst"
    ;;
status)
    adb shell "ls -la $DEV; tail -2 $DEV/samples.txt"
    ;;
*) echo "usage: $0 start | stop [name] | status"; exit 1 ;;
esac
