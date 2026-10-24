#!/usr/bin/env bash
# Does lifted x87 code build for aarch64? (fld qword [rdi]; fmul st0, st0; fstp qword [rsi])
set -euo pipefail
L=${REMILL_LIFT:-$HOME/src/remill/install/bin/remill-lift-22}
T=$(mktemp -d)
"$L" --arch amd64_avx --os linux --address 0x1000 --bytes dd07d8c8dd1e --ir_out "$T/x87.ll" 2>/dev/null
grep -o 'x86_fp80' "$T/x87.ll" | sort | uniq -c || echo "no x86_fp80 in IR"
grep -oE '(fpext|fptrunc|fmul) [a-z0-9_]+ ' "$T/x87.ll" | sort | uniq -c
dl=$(echo 'int x;' | clang-22 --target=aarch64-linux-gnu -x c - -S -emit-llvm -o - | sed -n 's/^target datalayout = //p')
sed -E -e 's/^target triple = .*/target triple = "aarch64-unknown-linux-gnu"/' \
    -e "s/^target datalayout = .*/target datalayout = $dl/" \
    -e 's/\b(noduplicate|noinline|optnone)\b//g' -e 's/dereferenceable\([0-9]+\)//g' \
    -e 's/"(target-cpu|target-features|tune-cpu)"="[^"]*"//g' "$T/x87.ll" > "$T/x87.arm.ll"
if llc-22 -O2 "$T/x87.arm.ll" -o "$T/x87.s" 2> "$T/llc.err"; then
    echo "llc aarch64: OK"; grep -oE 'bl\s+\S+' "$T/x87.s" | sort | uniq -c
else
    echo "llc aarch64: FAILED"; head -3 "$T/llc.err"
fi
rm -rf "$T"
