#!/bin/sh
# G2 limits before a measurement: charge, GPU ceiling (thermal_pwrlevel: 0 = 950 MHz), cores turned off
# (thermal-pause-<mask>: E0 = cores 5-7, i.e. the X4 and two A720), active cooling devices.
# 2026-10-19: after a discharge to shutdown the battery protection (BCL) kept the GPU at 684 MHz and cores 5-7
# off even at 57% charge: measurements in that state are not reliable.
export MSYS_NO_PATHCONV=1
adb shell '
dumpsys battery | grep -E "level|temperature" | tr -s " " | tr "\n" " "; echo
k=/sys/class/kgsl/kgsl-3d0
echo "GPU: cur $(cat $k/devfreq/cur_freq) thermal_pwrlevel $(cat $k/thermal_pwrlevel) (0 = 950 MHz)"
for c in /sys/class/thermal/cooling_device*; do
    s=$(cat $c/cur_state); t=$(cat $c/type)
    [ "$s" != 0 ] && case $t in *backlight*) ;; *) echo "limit: $t $s/$(cat $c/max_state)";; esac
done
for p in /sys/devices/system/cpu/cpufreq/policy*; do
    echo "$(basename $p): max $(cat $p/scaling_max_freq)/$(cat $p/cpuinfo_max_freq)"
done'
