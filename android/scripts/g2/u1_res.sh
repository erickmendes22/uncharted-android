#!/bin/sh
# U1 patch for the next APK start: u1_res.sh <name|none> [movie WxH]. The name is the file
# files/patches/<name>.bin on the G2 (built by scripts/g2/u1_patches.py from the Bachata catalog). Writes
# files/args.txt: the default arguments (as in GameActivity) + --patches; WxH sets
# BB_VDEC_CONTENT in env.txt (movies at an output below 1080p, port patch 0015), without it removes it.
# Leaves a running game alone: takes effect on the next start.
#   720p: u1_res.sh o720n 1280x720   (o720n: see notes/findings.md, "720p preset")
set -e
export MSYS_NO_PATHCONV=1
D=/sdcard/Android/data/dev.unchartedg2.game/files
name=${1:?usage: u1_res.sh <patch name|none> [movie WxH]}
movie=${2:-}
# The game's folder on the device: GAME_DIR, else the --app0 of the current args.txt.
app0=${GAME_DIR:-$(adb shell "grep -A1 '^--app0' $D/args.txt 2>/dev/null" | tail -1 | tr -d '\r')}
case "$app0" in /*) ;; *) echo "set GAME_DIR=<the game's folder on the device>" >&2; exit 1;; esac
args="$D/boot-linked.bin
--content-profile
$D/content.bin
--app0
$app0
--user
$D/user
--timeout
0
--log
$D/bb-probe.log"
if [ "$name" != none ]; then
    adb shell "test -f $D/patches/$name.bin" || { echo "no $D/patches/$name.bin" >&2; exit 1; }
    args="$args
--patches
$D/patches/$name.bin"
fi
printf '%s\n' "$args" | adb shell "cat > $D/args.txt"
adb shell "grep -v '^BB_VDEC_CONTENT=' $D/env.txt > $D/env.new 2>/dev/null; \
    [ -n '$movie' ] && echo 'BB_VDEC_CONTENT=$movie' >> $D/env.new; mv $D/env.new $D/env.txt"
adb shell "cat $D/args.txt" | tail -1
adb shell "grep BB_VDEC_CONTENT $D/env.txt || echo 'BB_VDEC_CONTENT: -'"
