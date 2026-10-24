#!/usr/bin/env bash
# Collecting data from the G2 over adb (stage 2 of the plan). Run from Git Bash on the PC:
#   bash android/scripts/g2/collect.sh    -> android/notes/devices/g2-<date>/
# vaprobe is built in WSL (aarch64-linux-gnu-gcc -static). The serial number is not saved.
set -euo pipefail
export MSYS_NO_PATHCONV=1
cd -- "$(dirname -- "$0")/../.."
out=notes/devices/g2-$(date +%F)
AND=/mnt/$(pwd -W | sed -E 's#^([A-Za-z]):#\L\1#')  # android/ in WSL
mkdir -p "$out"
command -v adb >/dev/null ||
    PATH+=":$(cygpath "$LOCALAPPDATA")/Microsoft/WinGet/Packages/Google.PlatformTools_Microsoft.Winget.Source_8wekyb3d8bbwe/platform-tools"
sh() { adb shell "$@" | tr -d '\r'; }

adb get-state >/dev/null

wsl -d Ubuntu-24.04 -- bash -lc "aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra \
    $AND/scripts/g2/vaprobe.c -o /tmp/vaprobe"
wsl -d Ubuntu-24.04 -- cat /tmp/vaprobe > "$out/.vaprobe"
adb push "$out/.vaprobe" /data/local/tmp/vaprobe >/dev/null
rm -f "$out/.vaprobe"
sh 'chmod 755 /data/local/tmp/vaprobe && /data/local/tmp/vaprobe' > "$out/vaprobe.txt"

{
    for p in ro.product.manufacturer ro.product.model ro.product.device ro.board.platform \
        ro.soc.manufacturer ro.soc.model ro.build.version.release ro.build.version.sdk \
        ro.build.version.security_patch ro.build.display.id ro.product.cpu.abilist \
        ro.hardware.vulkan ro.hardware.egl ro.gfx.driver.0 ro.gfx.driver.1 \
        ro.config.low_ram ro.lmk.critical ro.lmk.low ro.lmk.medium ro.lmk.use_psi \
        ro.lmk.swap_free_low_percentage ro.lmk.thrashing_limit dalvik.vm.heapsize \
        persist.sys.zram_enabled ro.zram.mark_idle_delay_mins; do
        printf '%s=%s\n' "$p" "$(sh getprop "$p")"
    done
    echo "uname=$(sh uname -a)"
    echo "selinux=$(sh getenforce)"
    echo "overcommit_memory=$(sh cat /proc/sys/vm/overcommit_memory 2>/dev/null)"
    echo "mmap_min_addr=$(sh cat /proc/sys/vm/mmap_min_addr 2>/dev/null)"
    echo "mmap_rnd_bits=$(sh cat /proc/sys/vm/mmap_rnd_bits 2>/dev/null)"
    echo "max_map_count=$(sh cat /proc/sys/vm/max_map_count 2>/dev/null)"
} > "$out/props.txt"

sh cat /proc/cpuinfo > "$out/cpuinfo.txt"
sh cat /proc/meminfo > "$out/meminfo.txt"
sh 'cat /proc/swaps; for z in /sys/block/zram*; do echo "$z disksize=$(cat $z/disksize) algo=$(cat $z/comp_algorithm)"; done' \
    > "$out/swap.txt" 2>&1 || true
sh 'for c in /sys/devices/system/cpu/cpu[0-9]*; do echo "$(basename $c) max=$(cat $c/cpufreq/cpuinfo_max_freq 2>/dev/null) cap=$(cat $c/cpu_capacity 2>/dev/null)"; done' \
    > "$out/cpufreq.txt" 2>&1 || true
sh dumpsys meminfo > "$out/dumpsys_meminfo.txt" 2>&1 || true
sh cmd gpu vkjson > "$out/vkjson.json" 2>&1 || true
sh 'ls /vendor/lib64/hw/ | grep -i vulkan; ls /vendor/lib64/egl/ 2>/dev/null' > "$out/gpu_libs.txt" 2>&1 || true
echo "Saved to $out"
ls -la "$out"
