#!/usr/bin/env bash
# In-game measurement on the G2: N seconds of "Frame rate" lines from the port's log (BB_FPS_LOG=1) and the
# GPU load (kgsl gpu_busy_percentage) once a second. Does not touch the game.
# Usage: fps_sample.sh [secs=20]
set -euo pipefail
export MSYS_NO_PATHCONV=1
PT=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1 || true)
[ -n "$PT" ] && export PATH="$PATH:$PT"
N=${1:-20}
D=/sdcard/Android/data/dev.unchartedg2.game/files
adb shell "pidof dev.unchartedg2.game >/dev/null || echo NO-GAME
before=\$(grep -c 'Frame rate' $D/bb-probe.log)
busy=''
i=0
while [ \$i -lt $N ]; do
  busy=\"\$busy \$(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage | tr -d ' %')\"
  sleep 1
  i=\$((i + 1))
done
grep 'Frame rate' $D/bb-probe.log | tail -n +\$((before + 1))
echo \"GPU busy %:\$busy\"
echo \"GPU MHz: \$(( \$(cat /sys/class/kgsl/kgsl-3d0/gpuclk) / 1000000 )), pwrlevel \$(cat /sys/class/kgsl/kgsl-3d0/thermal_pwrlevel)\""
