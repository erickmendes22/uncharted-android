#!/usr/bin/env bash
# run_bench_g2.sh <dir> [reps]: the pilot's tests on the G2 (Git Bash, adb) pinned to one X4 core
# (cpu 7) and one A720 core (cpu 3): native aarch64, lifted (plain, TSO) and under FEXCore. <dir>
# holds run-arm, run-lifted, run-lifted-tso (build.sh), guest.x86.elf (~/src/pilot), fex-bench
# (fexcore-smoke built from fex_bench.cpp) and the NDK's libc++_shared.so. Refuses while the game
# runs.
set -uo pipefail
export MSYS_NO_PATHCONV=1
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
dir=$1; reps=${2:-300}
if "$ADB" shell pidof dev.unchartedg2.game > /dev/null; then echo "the game is running: stop" >&2; exit 1; fi
D=/data/local/tmp/fexbench
"$ADB" shell "mkdir -p $D"
for f in fex-bench libc++_shared.so guest.x86.elf run-arm run-lifted run-lifted-tso; do
    "$ADB" push "$(cygpath -w "$dir/$f")" $D/$f > /dev/null
done
"$ADB" shell "chmod 755 $D/fex-bench $D/run-arm $D/run-lifted $D/run-lifted-tso"
for cpu in 7 3; do
    mask=$(printf '%x' $((1 << cpu)))
    for v in run-arm run-lifted run-lifted-tso; do
        echo "== cpu $cpu $v"
        "$ADB" shell "cd $D && taskset $mask ./$v $reps guest.x86.elf 2>&1 | grep -v '^t_m'"
    done
    echo "== cpu $cpu fex"
    "$ADB" shell "cd $D && LD_LIBRARY_PATH=. taskset $mask ./fex-bench $reps guest.x86.elf 2>&1"
done
