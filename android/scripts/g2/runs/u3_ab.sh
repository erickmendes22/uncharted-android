#!/usr/bin/env bash
# A/B runs in U3 (G2, Git Bash). The game starts through loadexec.txt (as when U3 is picked in the
# collection menu) with the owner's env + KEY=VALUE; every run restores the saves from a backup.
#   u3_ab.sh start <tag> <preset> [KEY=VALUE ...]  owner env + variables, args-u3 with the preset, start
#   u3_ab.sh bench <tag> <secs>                     title -> Continue -> OK, then <secs> without input
#                                                   (the owner's checkpoint: a static scene is a bench),
#                                                   a screenshot every 30 s, the last frame rates; stop
#   u3_ab.sh fight <tag> <secs>                     title -> New Game -> OK -> Campaign -> Explorer ->
#                                                   Yes -> the intro movie -> pause -> Skip cinematic ->
#                                                   the pub fight, fight.sh on the device for <secs>; stop
#   u3_ab.sh stop <tag>                             close the game, saves from the backup, owner env and
#                                                   args back, the log to $OUT/<tag>.log
# SAVES_BACKUP - the saves backup directory in files/, required (cp -r files/user files/<backup>
# before the first run). OUT - logs, screenshots (default $TMPDIR/u3-ab; not for git). The owner's
# env is taken from the G2 at the first start ($OUT/env-owner.txt) and put back on stop. Menus
# checked 2026-10-23 (U3 1.02, Russian, a save present: New Game is the 3rd item). The crash
# lines and the Memory line are printed after a stop.
set -euo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
P="bash $R/pad.sh"
PKG=dev.unchartedg2.game
F=/sdcard/Android/data/$PKG/files
OUT=${OUT:-${TMPDIR:-/tmp}/u3-ab}; mkdir -p "$OUT"
: "${SAVES_BACKUP:?SAVES_BACKUP=<saves backup directory in files/>}"
export SAVES_BACKUP
OWNER_ENV=$OUT/env-owner.txt

game_gone() {
    # adb misses an answer now and then: three checks.
    for i in 1 2 3; do
        if "$ADB" shell pidof $PKG > /dev/null; then return 1; fi
        sleep 3
    done
    return 0
}

watch() {
    # <tag> <secs>: a screenshot and the last frame rate every 30 s until <secs> or the game is gone.
    local tag=$1 secs=$2 t=0
    while [ $t -lt $secs ]; do
        sleep 30; t=$((t + 30))
        if game_gone; then echo "$tag: the game is gone at ~${t}s"; return; fi
        $P shot "$OUT/$tag-t$t.png" > /dev/null
        echo "$tag t=$t: $("$ADB" shell "grep 'Frame rate' $F/bb-probe.log | tail -1" | tr -d '\r')"
    done
}

case "$1" in
start)
    tag=$2; preset=$3; shift 3
    if [ ! -f "$OWNER_ENV" ]; then
        "$ADB" shell cat $F/env.txt | tr -d '\r' > "$OWNER_ENV"
        if grep -q BB_PAD_FILE "$OWNER_ENV"; then
            rm -f "$OWNER_ENV"; echo "env.txt on the G2 is a run's, not the owner's" >&2; exit 1
        fi
    fi
    { grep -v '^BB_DUMP_REQUEST=' "$OWNER_ENV"; for kv in "$@"; do echo "$kv"; done; } > "$OUT/env-$tag.txt"
    "$ADB" shell "test -f $F/patches/$preset" || { echo "no preset $preset" >&2; exit 1; }
    "$ADB" shell "test -f $F/args-u3.ab-orig.txt || cp $F/args-u3.txt $F/args-u3.ab-orig.txt;
        sed 's#/patches/[^/]*\$#/patches/$preset#' $F/args-u3.ab-orig.txt > $F/args-u3.txt"
    "$ADB" shell "echo /app0/big3-ps4_Shipping.elf > $F/loadexec.txt"
    $P start "$OUT/env-$tag.txt"
    ;;
bench)
    tag=$2; secs=$3
    sleep 45; $P shot "$OUT/$tag-title.png" > /dev/null; $P press cross; sleep 6   # the title
    $P press cross; sleep 4; $P press cross                                          # Continue; OK
    watch "$tag" "$secs"
    bash "$0" stop "$tag"
    ;;
fight)
    tag=$2; secs=$3
    sleep 50; $P press cross; sleep 6                                                      # the title
    $P press down 0.2; sleep 1.5; $P press down 0.2; sleep 1.5; $P press cross; sleep 5     # New Game
    $P press cross; sleep 4                                                                # friends' statistics: OK
    $P press cross; sleep 4                                                                # Campaign
    $P press up 0.2; sleep 1.5; $P press up 0.2; sleep 1.5; $P press cross; sleep 4         # Explorer
    $P press up 0.2; sleep 1.5; $P press cross; sleep 25                                   # Yes; the movie
    $P press options; sleep 3                                                              # pause
    $P press down 0.2; sleep 1.5; $P press down 0.2; sleep 1.5; $P press cross; sleep 20    # Skip cinematic
    $P shot "$OUT/$tag-fight0.png" > /dev/null
    "$ADB" push "$(cygpath -w "$R/fight.sh")" /data/local/tmp/fight.sh > /dev/null
    "$ADB" shell "sh /data/local/tmp/fight.sh $secs" > /dev/null 2>&1 &
    watch "$tag" "$secs"
    # pgrep -f would match this adb shell's own command line: the bracket keeps it out.
    "$ADB" shell 'p=$(pgrep -f "/data/local/tmp/[f]ight.sh"); [ -n "$p" ] && kill $p; echo > '"$F"'/pad.txt'
    wait 2> /dev/null || true
    bash "$0" stop "$tag"
    ;;
stop)
    tag=$2
    $P stop "$OUT/$tag.log" "$OWNER_ENV"
    "$ADB" shell "rm -f $F/loadexec.txt; if [ -f $F/args-u3.ab-orig.txt ]; then mv $F/args-u3.ab-orig.txt $F/args-u3.txt; fi"
    grep -nE "^(Fault|Guest fault|Host fault)|ASSERTION|STOP:" "$OUT/$tag.log" | head -5 || true
    grep "Memory:" "$OUT/$tag.log" | tail -1 | cut -c1-200 || true
    grep "Frame rate" "$OUT/$tag.log" | tail -3 || true
    ;;
*)
    sed -n '2,15p' "$0"; exit 1
    ;;
esac
