#!/usr/bin/env bash
# A profile of the game on the G2 (simpleperf, the APK is profileable): record N seconds, a report by threads and
# functions in WSL with symbols from the unstripped libmain/libbbgpu (matched by build-id).
#   bash scripts/g2/profile.sh <dir> [secs=10] [extra record arguments, e.g. "-g"]
# Result: <dir>/perf.data, report-threads.txt, report-symbols.txt.
set -euo pipefail
export MSYS_NO_PATHCONV=1
OUT=${1:?directory}; SECS=${2:-10}; EXTRA=${3:-}
ADB=${ADB:-$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)/adb.exe}
mkdir -p "$OUT"
"$ADB" shell "simpleperf record --app dev.unchartedg2.game -e cpu-clock -f 2000 $EXTRA \
    --duration $SECS -o /data/local/tmp/bb-perf.data" 2>&1 | grep -v kallsyms | tail -2
"$ADB" pull /data/local/tmp/bb-perf.data "$(cygpath -m "$OUT")/perf.data" >/dev/null
W=$(wsl -d Ubuntu-24.04 -e wslpath -u "$(cygpath -m "$OUT")")
wsl -d Ubuntu-24.04 -e bash -c "
  SP=~/android/android-ndk-r30/simpleperf/bin/linux/x86_64/simpleperf
  SYM=~/src/uncharted-android-build
  cd '$W'
  \$SP report -i perf.data --symdir \$SYM --sort comm --percent-limit 0.5 > report-threads.txt 2>/dev/null
  \$SP report -i perf.data --symdir \$SYM --sort comm,dso,symbol --percent-limit 0.3 > report-symbols.txt 2>/dev/null
  head -40 report-threads.txt"
