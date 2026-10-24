#!/usr/bin/env bash
# All of the game's modules in a row (a scale check). Run in WSL: bash lift_all.sh > lift_all.log
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
for m in eboot big2 big3 libc libSceFios2; do
    rm -rf "$HOME/src/lifted/$m"
    echo "=== $m"
    bash "$HERE/lift_module.sh" "$m" 500 "$(nproc)" 2>&1 | grep -v "couldn.t allocate\|^ *[0-9]* *lib\|Stack dump\|PLEASE" | tail -6
done
