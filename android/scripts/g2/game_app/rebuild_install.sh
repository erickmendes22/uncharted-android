#!/usr/bin/env bash
# Rebuild libmain/libbbgpu (WSL) -> APK -> install on the G2. Stops on a build error.
#   bash android/scripts/g2/game_app/rebuild_install.sh
# EXTRA_LIBS (WSL paths separated by spaces): more .so files in the APK; TURNIP=1 adds Turnip and
# the libadrenotools hook libraries (enabled in the game with BB_VK_DRIVER=libvulkan_freedreno.so);
# TURNIP_SO: another Turnip build (a WSL path). Leaves a running game alone (FORCE=1: close it).
set -euo pipefail
export MSYS_NO_PATHCONV=1
ADB=${ADB:-$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools 2>/dev/null | head -1)/adb.exe}
if [ "${FORCE:-0}" != 1 ] && "$ADB" shell pidof dev.unchartedg2.game >/dev/null; then
    echo "the game is running: installing would close it (FORCE=1: install anyway)" >&2
    exit 1
fi
TMP=$(cygpath -m "${TMPDIR:-/tmp}")
# android/ of this clone as a WSL path (C:/x → /mnt/c/x)
AND=/mnt/$(cd -- "$(dirname -- "$0")/../../.." && pwd -W | sed -E 's#^([A-Za-z]):#\L\1#')
if [ "${TURNIP:-0}" = 1 ]; then
    AT='$HOME/src/adrenotools-android/src/hook'
    TS=${TURNIP_SO:-'$HOME/src/turnip-android/libvulkan_freedreno.so'}
    EXTRA_LIBS="${EXTRA_LIBS:-} $TS $AT/libmain_hook.so $AT/libfile_redirect_hook.so $AT/libgsl_alloc_hook.so $AT/libhook_impl.so"
fi
wsl -d Ubuntu-24.04 -e bash -c "set -o pipefail; cd $AND && \
  bash scripts/port-arm64/build_gpu_android.sh main > /tmp/g2-build.log 2>&1 || { grep -E 'error|FAILED' /tmp/g2-build.log | head -20; exit 1; } && \
  EXTRA_LIBS=\"${EXTRA_LIBS:-}\" bash scripts/g2/game_app/build.sh > /tmp/g2-apk.log 2>&1 || { tail -20 /tmp/g2-apk.log; exit 1; } && \
  cp ~/src/game_app/uncharted-g2.apk \"\$(wslpath -u '$TMP')/uncharted-g2.apk\""
"$ADB" install -r "$TMP/uncharted-g2.apk" >/dev/null
"$ADB" shell appops set dev.unchartedg2.game MANAGE_EXTERNAL_STORAGE allow
echo "installed"
