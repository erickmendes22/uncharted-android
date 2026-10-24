#!/usr/bin/env bash
# Every 2 s: if the KGSL fault count grew, read the snapshot (that frees the slot for the next fault).
export MSYS_NO_PATHCONV=1
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
OUT=$1
last=$("$ADB" shell cat /sys/class/kgsl/kgsl-3d0/snapshot/faultcount | tr -d '\r')
while true; do
    sleep 2
    n=$("$ADB" shell cat /sys/class/kgsl/kgsl-3d0/snapshot/faultcount 2>/dev/null | tr -d '\r')
    [ -n "$n" ] || continue
    if [ "$n" != "$last" ]; then
        f="$OUT/snap-$(date +%H%M%S)-$n.bin"
        "$ADB" exec-out cat /sys/class/kgsl/kgsl-3d0/snapshot/dump > "$f" 2>/dev/null
        [ -s "$f" ] && echo "$(date +%T) faultcount $last -> $n: $(basename "$f") $(stat -c %s "$f")" || rm -f "$f"
        last=$n
    fi
done
