#!/usr/bin/env bash
# Building the differential test of libc.prx: x86 native + aarch64 from lifted code. Run in WSL:
#   bash build_libc_diff.sh            → ~/src/libcdiff/{libc-diff-x86,libc-diff-arm}
# Needs: extract_code.py (libc.*), libc_prep.py (libc_funcs.h, libc_relocs.bin), ps4lift.
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
W=${LIBCDIFF_DIR:-$HOME/src/libcdiff}
INV=$HOME/src/inventory/code
LIFT=$HOME/src/ps4lift-build/ps4lift
BASE=0x900000000
ARM=(--target=aarch64-linux-gnu -mcpu=cortex-x4+nosve+nosve2)
LLC=(-O2 -filetype=obj -mtriple=aarch64-linux-gnu -mcpu=cortex-x4 -mattr=-sve,-sve2)
mkdir -p "$W/chunks"
cd "$W"

# x86 native
clang-22 -O2 -I. -I"$HERE" "$HERE/libc_diff.c" -o libc-diff-x86

# Lifting libc in pieces (addresses = BASE + vaddr, as in the harness)
N=$(wc -l < "$INV/libc.ranges")
seq 0 500 $((N - 1)) | xargs -P "$(nproc)" -I{} sh -c \
    "$LIFT --seg $INV/libc.seg0.bin --seg_vaddr 0 --load_base $BASE --ranges $INV/libc.ranges \
       --image $INV/libc.image.bin --table_prefix libc --first {} --count 500 \
       --out chunks/c{}.bc > chunks/c{}.log 2>&1"
cat chunks/*.log | grep '^chunk' | awk '{print}' | sed 's/lift_ms.*//'

# Runtime
clang-22 "${ARM[@]}" -O2 -emit-llvm -c "$HERE/ps4rt_intrinsics.c" -o ps4rt_intrinsics.bc
"$LIFT" --state_module state.bc > /dev/null
llc-22 "${LLC[@]}" state.bc -o state.o
clang-22 "${ARM[@]}" -O2 -c "$HERE/ps4rt.c" -o ps4rt.o
for bc in chunks/c*.bc; do
    [[ $bc == *.opt.bc || $bc == *.linked.bc ]] && continue
    b=${bc%.bc}
    llvm-link-22 "$bc" --internalize ps4rt_intrinsics.bc -o "$b.linked.bc"
    opt-22 -O2 "$b.linked.bc" -o "$b.opt.bc"
    llc-22 "${LLC[@]}" "$b.opt.bc" -o "$b.o" &
done
wait
clang-22 "${ARM[@]}" -O2 -DLIFTED -I. -I"$HERE" -c "$HERE/libc_diff.c" -o libc_diff_arm.o
aarch64-linux-gnu-gcc -static libc_diff_arm.o ps4rt.o state.o chunks/c*.o -lm -o libc-diff-arm 2> link.err ||
    { grep -oE "undefined reference to \`[^']+'" link.err | sort | uniq -c | sort -rn | head -30; exit 1; }
ls -la libc-diff-x86 libc-diff-arm

# The same lifted code compiled for x86-64 (run in WSL): separates semantics bugs
# (differs here too) from ARM problems: alignment, memory order (matches here).
X86DL="e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
X86T=(--target_triple x86_64-unknown-linux-gnu --target_datalayout "$X86DL")
mkdir -p chunks-x86
seq 0 500 $((N - 1)) | xargs -P "$(nproc)" -I{} sh -c \
    "$LIFT --seg $INV/libc.seg0.bin --seg_vaddr 0 --load_base $BASE --ranges $INV/libc.ranges \
       --image $INV/libc.image.bin --table_prefix libc --first {} --count 500 \
       --target_triple x86_64-unknown-linux-gnu --target_datalayout '$X86DL' \
       --out chunks-x86/c{}.bc > chunks-x86/c{}.log 2>&1"
clang-22 -O2 -mcx16 --target=x86_64-unknown-linux-gnu -emit-llvm -c "$HERE/ps4rt_intrinsics.c" -o ps4rt_intrinsics-x86.bc
"$LIFT" "${X86T[@]}" --state_module state-x86.bc > /dev/null
llc-22 -O2 -filetype=obj state-x86.bc -o state-x86.o
clang-22 -O2 -c "$HERE/ps4rt.c" -o ps4rt-x86.o
for bc in chunks-x86/c*.bc; do
    [[ $bc == *.opt.bc || $bc == *.linked.bc ]] && continue
    b=${bc%.bc}
    llvm-link-22 "$bc" --internalize ps4rt_intrinsics-x86.bc -o "$b.linked.bc"
    opt-22 -O2 "$b.linked.bc" -o "$b.opt.bc"
    llc-22 -O2 -filetype=obj "$b.opt.bc" -o "$b.o" &
done
wait
clang-22 -O2 -DLIFTED -I. -I"$HERE" -c "$HERE/libc_diff.c" -o libc_diff_lx86.o
clang-22 -no-pie libc_diff_lx86.o ps4rt-x86.o state-x86.o chunks-x86/c*.o -lm -o libc-diff-lifted-x86
ls -la libc-diff-lifted-x86
