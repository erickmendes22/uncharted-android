#!/usr/bin/env bash
# Pilot: guest.c -> an x86-64 image @0x8_0000_0000 -> Remill (each t_* with the signature RAX(RDI,RSI)) -> .ll
# Run in WSL: bash /mnt/c/repos/uncharted-g2/android/scripts/pilot/lift.sh   (works in ~/src/pilot)
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
W=${PILOT_DIR:-$HOME/src/pilot}
LIFT=${REMILL_LIFT:-$HOME/src/remill/install/bin/remill-lift-22}
mkdir -p "$W/lifted"
cd "$W"
cp "$HERE/guest.c" .

clang-22 --target=x86_64-linux-gnu -march=btver2 -O2 -DGUEST_IMAGE -ffreestanding -fno-math-errno \
    -fPIE -fno-stack-protector -fno-asynchronous-unwind-tables -nostdlib -static -fuse-ld=lld \
    -Wl,--image-base=0x800000000 -Wl,-e,_start guest.c -o guest.x86.elf
llvm-objdump-22 -d --no-show-raw-insn -M intel guest.x86.elf > guest.x86.dis

read -r text_addr < <(llvm-readelf-22 -SW guest.x86.elf | awk '{for (i = 1; i < NF; i++) if ($i == ".text") print "0x" $(i + 2)}')
llvm-objcopy-22 -O binary --only-section=.text guest.x86.elf text.bin
hex=$(xxd -p text.bin | tr -d '\n')

: > lifted/functions.txt
while read -r addr _ name; do
    [[ $name == t_* ]] || continue
    echo "lift $name @0x$addr"
    if "$LIFT" --arch amd64_avx --os linux --address "$text_addr" --entry_address "0x$addr" \
        --bytes "$hex" --signature "RAX(RDI,RSI)" --ir_out "lifted/$name.ll" \
        > "lifted/$name.log" 2>&1; then
        printf '%s 0x%x\n' "$name" "0x$addr" >> lifted/functions.txt
    else
        echo "  FAILED (lifted/$name.log)"; tail -3 "lifted/$name.log"
    fi
done < <(llvm-nm-22 --defined-only guest.x86.elf | sort)
echo "lifted: $(wc -l < lifted/functions.txt) functions in $W/lifted"
