#!/usr/bin/env bash
# A/B runs in U2 from the owner's last checkpoint (G2, Git Bash). The game starts through
# loadexec.txt (as when U2 is picked in the collection menu) with the owner's env + KEY=VALUE;
# every run restores the saves from a backup.
#   u2_ab.sh start <tag> [KEY=VALUE ...]  owner env + variables, U2, scripted pad
#   u2_ab.sh route <tag>                  title -> Continue -> OK ("friends' statistics") ->
#                                         measure at the checkpoint; right stick up 1 s -> measure
#   u2_ab.sh measure <tag>                measure in place: fps_sample 30 s + schedstat 10 s + nice
#   u2_ab.sh stop <tag>                   close the game, saves from the backup, owner env and args back
# SAVES_BACKUP - the saves backup directory in files/, required (cp -r files/user/savedata
# files/<backup>/ before the first run). PATCHES - a preset in files/patches/ for this run (args-u2.txt
# is swapped and restored on stop). OUT - logs, screenshots, measurements (default $TMPDIR/u2-ab; not
# for git). The owner's env is taken from the G2 at the first start ($OUT/env-owner.txt) and put
# back on stop. Timings suit U2 1.02 with the u2_720f preset (logos skipped). Right after loading
# the floor at the checkpoint may still be missing textures - check $OUT/<tag>-cp.png. Example:
# notes/findings.md (2026-10-21, worker priorities).
set -euo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
F=/sdcard/Android/data/dev.unchartedg2.game/files
OUT=${OUT:-${TMPDIR:-/tmp}/u2-ab}; mkdir -p "$OUT"
: "${SAVES_BACKUP:?SAVES_BACKUP=<saves backup directory in files/>}"
export SAVES_BACKUP
OWNER_ENV=$OUT/env-owner.txt
case "$1" in
start)
    tag=$2; shift 2
    if [ ! -f "$OWNER_ENV" ]; then
        "$ADB" shell cat $F/env.txt | tr -d '\r' > "$OWNER_ENV"
        if grep -q '^BB_PAD_FILE=' "$OWNER_ENV"; then
            rm -f "$OWNER_ENV"; echo "env.txt on the G2 is a run's, not the owner's" >&2; exit 1
        fi
    fi
    { grep -v '^BB_DUMP_REQUEST=' "$OWNER_ENV"; for kv in "$@"; do echo "$kv"; done; } > "$OUT/env-$tag.txt"
    if [ -n "${PATCHES:-}" ]; then
        "$ADB" shell "test -f $F/patches/$PATCHES" || { echo "no $F/patches/$PATCHES" >&2; exit 1; }
        "$ADB" shell "test -f $F/args-u2.ab-orig.txt || cp $F/args-u2.txt $F/args-u2.ab-orig.txt;
            sed 's#/patches/[^/]*\$#/patches/$PATCHES#' $F/args-u2.ab-orig.txt > $F/args-u2.txt"
    fi
    # U2's executable as the collection menu requests it (loadexec-last.txt holds whatever game
    # started last: after a U3 run it starts U3).
    "$ADB" shell "echo /app0/big2-ps4_Shipping.elf > $F/loadexec.txt"
    bash "$R/pad.sh" start "$OUT/env-$tag.txt"
    ;;
route)
    tag=$2; P="bash $R/pad.sh"
    sleep 45; $P shot "$OUT/$tag-title.png"; $P press cross; sleep 4; $P press cross; sleep 50
    $P shot "$OUT/$tag-dlg.png"; $P press cross; sleep 55; $P shot "$OUT/$tag-cp.png"
    bash "$0" measure "$tag-cp" > /dev/null
    $P press "ry=0" 1.0; sleep 3; $P shot "$OUT/$tag-up.png"
    bash "$0" measure "$tag-up" > /dev/null
    for v in cp up; do echo "== $tag-$v"; grep -E "Frame rate|GPU busy" "$OUT/$tag-$v.txt"; done
    ;;
measure)
    tag=$2
    "$ADB" push "$(cygpath -w "$R/../schedstat.sh")" /data/local/tmp/schedstat.sh > /dev/null
    { bash "$R/../fps_sample.sh" 30
      "$ADB" shell 'sh /data/local/tmp/schedstat.sh $(pidof dev.unchartedg2.game) 10'
      "$ADB" shell 'for t in /proc/$(pidof dev.unchartedg2.game)/task/*; do echo "nice $(sed "s/.*) //" $t/stat | cut -d" " -f17) $(cat $t/comm)"; done | sort | uniq -c'
    } 2>&1 | tee "$OUT/$tag.txt"
    ;;
stop)
    tag=$2
    bash "$R/pad.sh" stop "$OUT/$tag.log" "$OWNER_ENV"
    "$ADB" shell "rm -f $F/loadexec.txt; if [ -f $F/args-u2.ab-orig.txt ]; then mv $F/args-u2.ab-orig.txt $F/args-u2.txt; fi"
    ;;
*)
    sed -n '2,18p' "$0"; exit 1
    ;;
esac
