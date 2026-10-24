#!/usr/bin/env bash
# Lifting a whole game module in pieces + compiling for aarch64 (a scale check: time, size).
# Run in WSL:  bash lift_module.sh eboot [chunk=500] [jobs=16]
# Input: ~/src/inventory/code/<mod>.{json,ranges,seg*.bin} (scripts/inventory/extract_code.py).
# Output (outside git): ~/src/lifted/<mod>/cNNNNN.{bc,o,log}, a summary on stdout.
set -euo pipefail
MOD=$1
CHUNK=${2:-500}
JOBS=${3:-$(nproc)}
INV=$HOME/src/inventory/code
OUT=$HOME/src/lifted/$MOD
LIFT=$HOME/src/ps4lift-build/ps4lift
RT=$HOME/src/lift-rt/ps4rt_intrinsics.bc   # ps4rt (TSO), built below
ARMFLAGS=(-mtriple=aarch64-linux-gnu -mcpu=cortex-x4 -mattr=-sve,-sve2)
mkdir -p "$OUT" "$(dirname "$RT")"
clang-22 --target=aarch64-linux-gnu -mcpu=cortex-x4+nosve+nosve2 -O2 -emit-llvm \
    -c "$(cd -- "$(dirname -- "$0")/../rt" && pwd)/ps4rt_intrinsics.c" -o "$RT"
read -r SEG VADDR < <(python3 -I -c "import json,sys; s=json.load(open('$INV/$MOD.json'))['segments'][0]; print(s['file'], s['vaddr'])")
N=$(wc -l < "$INV/$MOD.ranges")
echo "$MOD: $N functions, chunk $CHUNK, $JOBS jobs"

export LIFT INV SEG VADDR MOD OUT CHUNK RT
t0=$(date +%s.%N)
seq 0 "$CHUNK" $((N - 1)) | xargs -P "$JOBS" -I{} bash -c \
    'f=$(printf "%05d" $(({} / CHUNK))); "$LIFT" --seg "$INV/$SEG" --seg_vaddr "$VADDR" --ranges "$INV/$MOD.ranges" \
        --image "$INV/$MOD.image.bin" --table_prefix "$MOD" \n        --first {} --count "$CHUNK" --out "$OUT/c$f.bc" > "$OUT/c$f.log" 2>&1 || echo "lift failed: chunk {}"'
t1=$(date +%s.%N)

compile_one() {
    local bc=$1 base=${1%.bc}
    llvm-link-22 "$bc" --internalize "$RT" -o "$base.linked.bc" &&
        opt-22 -O2 "$base.linked.bc" -o "$base.opt.bc" &&
        llc-22 -O2 -filetype=obj "${ARMFLAGS[@]}" "$base.opt.bc" -o "$base.o" &&
        rm -f "$base.linked.bc" || echo "compile failed: $bc"
}
export -f compile_one
export ARMFLAGS_STR="${ARMFLAGS[*]}"
ls "$OUT"/c*.bc | grep -v '\.\(linked\|opt\)\.bc$' | xargs -P "$JOBS" -I{} bash -c \
    'ARMFLAGS=($ARMFLAGS_STR); '"$(declare -f compile_one)"'; compile_one {}'
t2=$(date +%s.%N)

cat "$OUT"/c*.log | awk '
    /^chunk/ { for (i = 2; i <= NF; i++) { split($i, kv, "="); s[kv[1]] += kv[2] } n++ }
    END { printf "lifted chunks %d: funcs %d traces %d switch_targets %d failed %d | jump %d icall %d error %d missing %d hyper %d async %d | lift cpu %.0fs\n",
                 n, s["funcs"], s["traces"], s["switch_targets"], s["failed"], s["jump"], s["icall"], s["error"], s["missing"], s["hyper"], s["async"], s["total_ms"] / 1000 }'
printf 'wall: lift %.0fs, compile %.0fs\n' "$(echo "$t1 - $t0" | bc)" "$(echo "$t2 - $t1" | bc)"
du -cb "$OUT"/c?????.bc | tail -1 | awk '{printf "bitcode (lifted, pre-opt): %.1f MiB\n", $1 / 2^20}'
size -t "$OUT"/*.o 2>/dev/null | tail -1 | awk '{printf "aarch64 objects: text %.1f MiB, data %.1f MiB\n", $1 / 2^20, ($2 + $3) / 2^20}' ||
    du -cb "$OUT"/*.o | tail -1
