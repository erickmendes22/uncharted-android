#!/usr/bin/env bash
# ARM code size of lifted pieces depending on the runtime and the optimization level.
# Run in WSL: bash size_experiment.sh [mod=eboot] [chunks="c00000 c00010 c00020"]
set -euo pipefail
MOD=${1:-eboot}
CHUNKS=${2:-"c00000 c00010 c00020"}
HERE=$(cd -- "$(dirname -- "$0")/../rt" && pwd)
L=$HOME/src/lifted/$MOD
T=$(mktemp -d)
ARM=(--target=aarch64-linux-gnu -mcpu=cortex-x4+nosve+nosve2)
LLC=(-filetype=obj -mtriple=aarch64-linux-gnu -mcpu=cortex-x4 -mattr=-sve,-sve2)
clang-22 "${ARM[@]}" -O2 -DPS4RT_NO_TSO -emit-llvm -c "$HERE/ps4rt_intrinsics.c" -o "$T/plain.bc"
clang-22 "${ARM[@]}" -O2 -emit-llvm -c "$HERE/ps4rt_intrinsics.c" -o "$T/tso.bc"
x86=0
for c in $CHUNKS; do
    x86=$((x86 + $(grep -o 'funcs=[0-9]*' "$L/$c.log" | head -1 | cut -d= -f2)))
done
printf '%-28s %10s\n' variant "text KiB"
for rt in plain tso; do
    for O in O2 Os; do
        total=0
        for c in $CHUNKS; do
            llvm-link-22 "$L/$c.bc" --internalize "$T/$rt.bc" -o "$T/$c.l.bc"
            opt-22 -"$O" "$T/$c.l.bc" -o "$T/$c.o.bc"
            llc-22 -"${O/s/2}" "${LLC[@]}" "$T/$c.o.bc" -o "$T/$c.o"
            total=$((total + $(size -A "$T/$c.o" | awk '$1 ~ /^\.text/ {s += $2} END {print s}')))
        done
        printf '%-28s %10d\n' "$rt opt-$O" $((total / 1024))
    done
done
echo "($x86 functions in $CHUNKS)"
rm -rf "$T"
