#!/usr/bin/env bash
# Throttling measurement on the G2 while playing: every N seconds the CPU clocks (per cluster), GPU,
# temperatures (maximum over cpu-*, gpuss-*, skin/battery), MemAvailable and SwapFree.
#   bash scripts/g2/thermal_watch.sh <file.csv> [interval=5] [secs=1800]
set -uo pipefail
export MSYS_NO_PATHCONV=1
OUT=${1:?csv}; STEP=${2:-5}; TOTAL=${3:-1800}
ADB=${ADB:-$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)/adb.exe}
echo "time,cpu0_khz,cpu3_khz,cpu7_khz,cpu0_max,cpu3_max,cpu7_max,gpu_hz,gpu_max_hz,cpu_temp_max,gpu_temp_max,battery_temp,mem_avail_kb,swap_free_kb,game_running" > "$OUT"
end=$(( $(date +%s) + TOTAL ))
while [ "$(date +%s)" -lt "$end" ]; do
    line=$(timeout 15 "$ADB" shell '
        f(){ cat /sys/devices/system/cpu/cpu$1/cpufreq/scaling_cur_freq; }
        m(){ cat /sys/devices/system/cpu/cpu$1/cpufreq/scaling_max_freq; }
        ct=0; gt=0; bt=0
        for z in /sys/class/thermal/thermal_zone*; do t=$(cat $z/type); v=$(cat $z/temp 2>/dev/null || echo 0)
            case $t in cpu-*) [ $v -gt $ct ] && ct=$v;; gpuss-*) [ $v -gt $gt ] && gt=$v;; battery) bt=$v;; esac; done
        ma=$(grep MemAvailable /proc/meminfo | tr -s " " | cut -d" " -f2)
        sf=$(grep SwapFree /proc/meminfo | tr -s " " | cut -d" " -f2)
        run=$(pidof dev.unchartedg2.game >/dev/null && echo 1 || echo 0)
        echo "$(f 0),$(f 3),$(f 7),$(m 0),$(m 3),$(m 7),$(cat /sys/class/kgsl/kgsl-3d0/gpuclk),$(cat /sys/class/kgsl/kgsl-3d0/max_gpuclk),$ct,$gt,$bt,$ma,$sf,$run"' 2>/dev/null | tr -d '\r')
    [ -n "$line" ] && echo "$(date +%H:%M:%S),$line" >> "$OUT"
    sleep "$STEP"
done
