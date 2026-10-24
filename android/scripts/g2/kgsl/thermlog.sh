#!/usr/bin/env bash
# Every 15 s while the game runs: time, hottest gold (cpu-1-*), prime (cpu-2-*) and GPU zones
# (milli-C), cpu3..7 current frequency (MHz).   thermlog.sh <out> [max_seconds]
export MSYS_NO_PATHCONV=1
OUT=$1; MAX=${2:-900}
end=$(( $(date +%s) + MAX ))
while [ "$(date +%s)" -lt "$end" ]; do
    line=$(adb shell 'pidof dev.unchartedg2.game >/dev/null || exit 0
      mx() { m=0; for z in /sys/class/thermal/thermal_zone*; do case "$(cat $z/type)" in $1) v=$(cat $z/temp); [ "$v" -gt "$m" ] && m=$v;; esac; done; echo $((m/1000)); }
      f=""; for c in 3 4 5 6 7; do f="$f $(( $(cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_cur_freq) / 1000 ))"; done
      echo "gold $(mx "cpu-1-*") prime $(mx "cpu-2-*") gpu $(mx "gpuss-*") MHz$f"' 2>/dev/null | tr -d '\r')
    [ -n "$line" ] && echo "$(date +%H:%M:%S) $line" >> "$OUT"
    sleep 15
done
