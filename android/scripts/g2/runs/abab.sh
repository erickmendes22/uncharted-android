#!/usr/bin/env bash
# abab.sh <u2|u3> <tag> [KEY=VALUE ...]: an interleaved A/B on the G2 - B A B A, where A runs with
# the given variables and B without, each run after a cool-down with the screen off (COOL
# seconds, 180). Back-to-back runs drift with heat (the first of a series flatters): compare
# the pairs, not the runs.
#   u2: u2_ab.sh start/route/stop from the owner's palace checkpoint (checkpoint + camera up)
#   u3: u3_ab.sh start/bench from the owner's U3 checkpoint (a static scene), SECS seconds (150)
# Per run: the frame rates, and from bb-probe.log the GPU thread's idle, write faults and hot
# pages (BB_FRAME_STATS=1 is added), the host protections (the Memory line).
# SAVES_BACKUP required (see u2_ab.sh/u3_ab.sh); OUT for the logs; PRESET for u3 (u3_720f.bin).
set -uo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
game=$1; tag=$2; shift 2
COOL=${COOL:-180}; SECS=${SECS:-150}; PRESET=${PRESET:-u3_720f.bin}
export OUT=${OUT:-${TMPDIR:-/tmp}/$game-ab}
for r in B1 A1 B2 A2; do
    "$ADB" shell input keyevent KEYCODE_SLEEP; sleep "$COOL"
    if [ "${r:0:1}" = A ]; then vars=("$@"); else vars=(); fi
    case $game in
    u2)
        bash "$R/u2_ab.sh" start "$tag$r" BB_FRAME_STATS=1 "${vars[@]}" 2>&1 | grep -v pushed
        bash "$R/u2_ab.sh" route "$tag$r" 2>&1 | grep -E "^== |Frame rate" | head -8
        bash "$R/u2_ab.sh" stop "$tag$r" 2>&1 | tail -1
        ;;
    u3)
        bash "$R/u3_ab.sh" start "$tag$r" "$PRESET" BB_FRAME_STATS=1 "${vars[@]}" 2>&1 | grep -v pushed
        bash "$R/u3_ab.sh" bench "$tag$r" "$SECS" 2>&1 | grep -v pulled | tail -4
        ;;
    *) echo "u2 or u3" >&2; exit 1;;
    esac
    L=$OUT/$tag$r.log
    echo "== $tag$r: $(grep 'Frame rate' "$L" | tail -6 | sed -E 's/.*: ([0-9.]+) FPS.*/\1/' | awk '{s+=$1;n++} END {printf "%.1f fps (last 6)", s/n}'); $(grep 'Frame stats' "$L" | tail -1 | grep -o -E 'idle [0-9.]+%' | head -1); $(grep 'Frame stats' "$L" | tail -1 | grep -o -E '[0-9]+ write faults/s'); $(grep 'Frame stats' "$L" | tail -1 | grep -o -E '[0-9]+ hot pages'); $(grep 'Memory:' "$L" | tail -1 | grep -o -E 'host protects [0-9]+/s')"
done
"$ADB" shell input keyevent KEYCODE_SLEEP
