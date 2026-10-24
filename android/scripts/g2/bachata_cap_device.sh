#!/system/bin/sh
# Metrics on the G2 while playing in Bachata S4 (runs on the device, see bachata_capture.sh).
# Writes into $OUT: samples.txt (every 2 s: GPU busy/clock, CPU clocks, temperatures),
# top.txt (every 6 s: the busiest threads), mem.txt (every 30 s: dumpsys meminfo),
# fps.txt (FPS over the last 127 SurfaceFlinger frames), logcat.txt.
OUT=${1:-/data/local/tmp/bcap}
PKG=com.bachatas4.android.github
mkdir -p "$OUT"
echo $$ > "$OUT/pid"
logcat -c
logcat -v threadtime > "$OUT/logcat.txt" &
KG=/sys/class/kgsl/kgsl-3d0
n=0
while [ ! -f "$OUT/stop" ]; do
    t=$(date +%s.%N | cut -c1-14)
    f=$(cat /sys/devices/system/cpu/cpu[0-7]/cpufreq/scaling_cur_freq | tr '\n' ' ')
    tc=0; tg=0
    for z in /sys/class/thermal/thermal_zone*; do
        ty=$(cat $z/type); v=$(cat $z/temp)
        case $ty in cpu-*) [ $v -gt $tc ] && tc=$v ;; gpuss-*) [ $v -gt $tg ] && tg=$v ;; esac
    done
    echo "$t gpu_busy=$(cat $KG/gpu_busy_percentage | tr -d ' %') gpuclk=$(cat $KG/gpuclk) cpu_mhz=[$f] t_cpu=$tc t_gpu=$tg" >> "$OUT/samples.txt"
    # Android 15: --list prints "RequestedLayerState{<name> parentId=...}"; FPS from desiredPresent
    layer=$(dumpsys SurfaceFlinger --list | grep -i bachata | grep "SurfaceView\[" | grep -iv background \
        | head -1 | sed -e "s/^RequestedLayerState{//" -e "s/ parentId=.*//")
    if [ -n "$layer" ]; then
        dumpsys SurfaceFlinger --latency "$layer" | awk -v t=$t 'NR>1 && $2>0 && $2<1e17 {if(!a)a=$2; b=$2; n++}
            END {if (n > 1) printf "%s fps=%.1f window=%.1fs\n", t, (n-1)/((b-a)/1e9), (b-a)/1e9}' >> "$OUT/fps.txt"
    fi
    if [ $((n % 3)) -eq 0 ]; then
        echo "=== $t" >> "$OUT/top.txt"
        top -b -n1 -H -d1 -s 3 -m 30 -o TID,PID,%CPU,CPU,S,NAME,CMDLINE >> "$OUT/top.txt" 2>&1
    fi
    if [ $((n % 15)) -eq 0 ]; then
        echo "=== $t" >> "$OUT/mem.txt"
        cat /proc/meminfo | head -5 >> "$OUT/mem.txt"
        for p in $(pidof $PKG) $(pgrep -f shadps4); do
            echo "--- pid $p $(tr '\0' ' ' < /proc/$p/cmdline | cut -c1-120)" >> "$OUT/mem.txt"
            dumpsys meminfo $p | grep -E "TOTAL|Native Heap|Graphics|GL mtrack|EGL mtrack|Unknown|TOTAL SWAP" >> "$OUT/mem.txt"
        done
    fi
    n=$((n + 1))
    sleep 2
done
kill %1 2>/dev/null
