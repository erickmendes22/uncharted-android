#!/system/bin/sh
# On the G2 (adb shell): which CPU each thread runs on - the "last CPU" field of
# /proc/<pid>/task/*/stat, sampled every ~50 ms; per thread (name#tid, names starting with a
# prefix, a shell pattern) the share of samples on each CPU. Does not touch the game.
#   adb push cpu_place.sh /data/local/tmp/ &&
#   adb shell 'sh /data/local/tmp/cpu_place.sh $(pidof dev.unchartedg2.game) 100 NdJob'
# The G2: CPUs 0-2 A520, 3-6 A720, 7 X4.
p=$1; n=${2:-100}; pre=${3:-NdJob}
i=0
while [ $i -lt $n ]; do
  for t in /proc/$p/task/*; do
    read -r c < $t/comm 2>/dev/null || continue
    case "$c" in $pre*) ;; *) continue;; esac
    read -r s < $t/stat 2>/dev/null || continue
    # the name may hold spaces: the fields after its ")" start at field 3; field 39 is the CPU
    r=${s##*) }; set -- $r; shift 36; echo "$c#${t##*/} $1"
  done
  i=$((i+1)); sleep 0.05
done | awk '{ cpu=$NF; $NF=""; k[$0]++; c[$0 cpu]++ }
  END { for (nm in k) { line=sprintf("%-26s", nm); for (x=0;x<8;x++) line=line sprintf(" %3d", 100*c[nm x]/k[nm]); print line } }' | sort
echo "columns: CPU 0..7, % of samples"
