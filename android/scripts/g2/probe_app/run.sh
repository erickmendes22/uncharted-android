#!/usr/bin/env bash
# Install the probe on the G2, run it, fetch the report. Git Bash on the PC:
#   bash scripts/g2/probe_app/run.sh   -> notes/devices/g2-<date>/app_probe.txt
# Remove from the device: adb uninstall dev.unchartedg2.probe
set -euo pipefail
export MSYS_NO_PATHCONV=1
cd -- "$(dirname -- "$0")/../../.."
command -v adb >/dev/null ||
    PATH+=":$(cygpath "$LOCALAPPDATA")/Microsoft/WinGet/Packages/Google.PlatformTools_Microsoft.Winget.Source_8wekyb3d8bbwe/platform-tools"
PKG=dev.unchartedg2.probe
out=notes/devices/g2-$(date +%F)
mkdir -p "$out"
tmp=$(mktemp -d)
wsl -d Ubuntu-24.04 -e sh -c 'cat ~/src/probe_app/probe.apk' > "$tmp/probe.apk"
adb install -r "$(cygpath -w "$tmp/probe.apk")" >/dev/null
rm -rf "$tmp"
adb shell run-as $PKG rm -f files/report.txt
adb shell am start -W -n $PKG/android.app.NativeActivity >/dev/null
for _ in $(seq 30); do
    adb exec-out run-as $PKG cat files/report.txt 2>/dev/null | grep -q '^=== done' && break
    sleep 1
done
adb exec-out run-as $PKG cat files/report.txt | tr -d '\r' > "$out/app_probe.txt"
echo "Saved $out/app_probe.txt ($(wc -l < "$out/app_probe.txt") lines)"
