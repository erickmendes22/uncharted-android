#!/usr/bin/env bash
# Device-loss repro: N long sessions with extra env; after each, the GPU kernel lines since the
# run started (GPU faults counted apart from the periodic AHB noise), KGSL preemptions and
# resets during the run, and the run's summary.
#   [WAIT=300] [NO_IFPC=1] dlbatch.sh <dir> <n> [KEY=VALUE ...]
# NO_IFPC=1: the GPU kept powered during each session (KGSL_PROP_PWRCTRL 0 via
# /data/local/tmp/kgslpwr; global and sticky, so it is set back to 1 after each session and on exit).
set -uo pipefail
export MSYS_NO_PATHCONV=1
R=/c/repos/uncharted-g2/android/scripts/g2/runs
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
K=/sys/class/kgsl/kgsl-3d0
DIR=$1; N=$2; shift 2
mkdir -p "$DIR"
stamp() { "$ADB" shell 'dmesg | tail -1' | tr -d '\r' | sed -E 's/^\[ *([0-9]+)\..*/\1/'; }
counters() { "$ADB" shell "printf '%d %d %d' \$(cat $K/preempt_count) \$(cat $K/reset_count) \$(cat $K/snapshot/faultcount)" | tr -d '\r'; }
pwr() { "$ADB" shell /data/local/tmp/kgslpwr "$1" >/dev/null 2>&1 || echo "kgslpwr $1 failed"; }
[ "${NO_IFPC:-0}" = 1 ] && trap 'pwr 1' EXIT
for i in $(seq "$N"); do
    [ "${NO_IFPC:-0}" = 1 ] && pwr 0
    since=$(stamp)
    i0=$("$ADB" shell cat $K/ifpc_count | tr -d '')
    read -r p0 r0 f0 <<< "$(counters)"
    WAIT=${WAIT:-300} bash "$R/session.sh" "$DIR" "r$i" "$@"
    read -r p1 r1 f1 <<< "$(counters)"
    i1=$("$ADB" shell cat $K/ifpc_count | tr -d '')
    [ "${NO_IFPC:-0}" = 1 ] && pwr 1
    "$ADB" shell 'dmesg' | tr -d '\r' | grep -i -E 'kgsl|adreno|gmu|iommu|smmu' |
        awk -v t="$since" '{ s = $0; sub(/^\[ */, "", s); sub(/\..*/, "", s); if (s + 0 >= t + 0) print }' \
        > "$DIR/r$i-dmesg.txt"
    lost=$(grep -c -i -E 'device lost|device was lost' "$DIR/r$i.log" 2>/dev/null)
    faults=$(grep -c -E 'opcode error|Protected mode|page fault' "$DIR/r$i-dmesg.txt")
    ahb=$(grep -c 'AHB bus error' "$DIR/r$i-dmesg.txt")
    # The snapshot KGSL keeps until read (the first fault since the last read); reading frees it.
    "$ADB" exec-out cat $K/snapshot/dump > "$DIR/r$i-snap.bin" 2>/dev/null || true
    [ -s "$DIR/r$i-snap.bin" ] || rm -f "$DIR/r$i-snap.bin"
    sum=$(python -I "$(cygpath -w "$R/absum.py")" "$(cygpath -w "$DIR/r$i.log")" 60 2>/dev/null)
    echo "r$i: device lost $lost, GPU faults $faults, AHB $ahb, IFPC $((i1 - i0)), preemptions $((p1 - p0)), resets $((r1 - r0)); $sum"
done
