#!/usr/bin/env bash
# The pilot, building three variants (after lift.sh). Run in WSL:
#   bash /mnt/c/repos/uncharted-g2/android/scripts/pilot/build.sh
# out/run-x86      x86-64 native (-march=btver2): the reference for results
# out/run-arm      aarch64 native (-O2 -mcpu=cortex-x4): the reference for speed
# out/run-lifted   aarch64 from the lifted Remill IR + runtime.c (static, run on the G2)
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
W=${PILOT_DIR:-$HOME/src/pilot}
cd "$W"
cp "$HERE/runner.c" "$HERE/runtime.c" "$HERE/guest.c" .
mkdir -p out build
# G2: SVE/SVE2 are off (vaprobe), while -mcpu=cortex-x4 turns them on -> SIGILL.
ARM=(--target=aarch64-linux-gnu -mcpu=cortex-x4+nosve+nosve2)
LINK_ARM() { aarch64-linux-gnu-gcc -static "$@" -lm; }

# A. x86 native
clang-22 -O2 -march=btver2 -fno-math-errno runner.c guest.c -lpthread -o out/run-x86

# B. aarch64 native
clang-22 "${ARM[@]}" -O2 -fno-math-errno -c runner.c -o build/runner-arm.o
clang-22 "${ARM[@]}" -O2 -fno-math-errno -c guest.c -o build/guest-arm.o
LINK_ARM build/runner-arm.o build/guest-arm.o -lpthread -o out/run-arm

# C. lifted code: retarget the IR to aarch64, link with the runtime, -O3, into an object.
aarch64_dl=$(echo 'int x;' | clang-22 --target=aarch64-linux-gnu -x c - -S -emit-llvm -o - |
    sed -n 's/^target datalayout = //p')
{
    echo '#pragma once'
    while read -r name addr; do echo "u64 call_sub_${addr#0x}(u64, u64);"; done < lifted/functions.txt
    echo 'static const struct { const char *name; fn2 f; } tests[] = {'
    while read -r name addr; do echo "    {\"$name\", call_sub_${addr#0x}},"; done < lifted/functions.txt
    echo '};'
} > lifted_table.h
for ll in lifted/t_*.ll; do
    # x86 specifics of the module: triple/datalayout, noinline/optnone attributes on intrinsics,
    # dereferenceable(N) (the State size in the x86 layout), target-cpu/features.
    sed -E -e "s/^target triple = .*/target triple = \"aarch64-unknown-linux-gnu\"/" \
        -e "s/^target datalayout = .*/target datalayout = $aarch64_dl/" \
        -e 's/\b(noduplicate|noinline|optnone)\b//g' \
        -e 's/dereferenceable\([0-9]+\)//g' \
        -e 's/"(target-cpu|target-features|tune-cpu)"="[^"]*"//g' \
        "$ll" > "build/$(basename "$ll")"
done
clang-22 "${ARM[@]}" -O2 -DLIFTED -I. -c runner.c -o build/runner-lifted.o
# Two runtimes: plain loads/stores (run-lifted) and TSO emulation (run-lifted-tso).
for variant in plain tso; do
    defs=(); suffix=""
    [[ $variant == tso ]] && { defs=(-DPILOT_TSO); suffix="-tso"; }
    clang-22 "${ARM[@]}" -O2 -fno-math-errno "${defs[@]}" -emit-llvm -c runtime.c -o "build/runtime$suffix.bc"
    llvm-link-22 build/t_*.ll "build/runtime$suffix.bc" -o "build/lifted_all$suffix.bc"
    opt-22 -O3 "build/lifted_all$suffix.bc" -o "build/lifted_opt$suffix.bc"
    llvm-dis-22 "build/lifted_opt$suffix.bc" -o "build/lifted_opt$suffix.ll"
    clang-22 "${ARM[@]}" -O2 -c "build/lifted_opt$suffix.bc" -o "build/lifted$suffix.o"
    LINK_ARM build/runner-lifted.o "build/lifted$suffix.o" -lpthread -o "out/run-lifted$suffix"
    llvm-objdump-22 -d --no-show-raw-insn "build/lifted$suffix.o" > "build/lifted$suffix.dis"
done
ls -la out
