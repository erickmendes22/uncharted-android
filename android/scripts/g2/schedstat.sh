#!/system/bin/sh
# On the G2 (adb shell): each thread's share of time on a CPU and in the run queue ("ready, waiting
# for a core") over an interval (% of one core; /proc/<pid>/task/*/schedstat), plus PSI CPU. Does not
# touch the game.
#   adb push schedstat.sh /data/local/tmp/ && adb shell 'sh /data/local/tmp/schedstat.sh $(pidof dev.unchartedg2.game) 10'
# Thread names are cut at the colon (shadPS4:GpuComm -> shadPS4): tell them apart by tid.
p=$1; s=${2:-5}
snap() { cut -d' ' -f1 /proc/uptime; grep -H . /proc/$p/task/*/schedstat; grep -H . /proc/$p/task/*/comm; }
snap > /data/local/tmp/ss1; sleep $s; snap > /data/local/tmp/ss2
awk '
FNR==1 { f++; t[f]=$1; next }
{ split($0, a, ":"); n=split(a[1], q, "/"); tid=q[5]; kind=q[6] }
kind=="comm" { name[tid]=a[2]; next }
{ split(a[2], v, " "); if (f==1) { c1[tid]=v[1]; w1[tid]=v[2] } else { c2[tid]=v[1]; w2[tid]=v[2] } }
END { dt=t[2]-t[1]; for (i in c2) if (i in c1) { cpu=(c2[i]-c1[i])/1e9/dt*100; rq=(w2[i]-w1[i])/1e9/dt*100;
  if (cpu>3||rq>5) printf "%-24s %6s cpu %5.1f%%  wait %5.1f%%\n", name[i], i, cpu, rq }
  printf "interval %.2f s\n", dt }' /data/local/tmp/ss1 /data/local/tmp/ss2 | sort -t'%' -k1 -r
cat /proc/pressure/cpu
