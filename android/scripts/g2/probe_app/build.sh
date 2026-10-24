#!/usr/bin/env bash
# Building the probe APK without Gradle (NDK r30 + build-tools 35 + android-35). Run in WSL:
#   bash /mnt/c/repos/uncharted-g2/android/scripts/g2/probe_app/build.sh   → ~/src/probe_app/probe.apk
# Install/run: scripts/g2/probe_app/run.sh (Git Bash on the PC).
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
NDK=${NDK:-$HOME/android/android-ndk-r30}
SDK=${SDK:-$HOME/android/sdk}
BT=$SDK/build-tools/35.0.1
JAR=$SDK/platforms/android-35/android.jar
W=${PROBE_DIR:-$HOME/src/probe_app}
CC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang
mkdir -p "$W/lib/arm64-v8a"
cd "$W"
"$CC" -O2 -shared -fPIC -Wall -Wextra -Wno-unused-parameter -I"$HERE" "$HERE/probe.c" \
    -landroid -llog -lvulkan -Wl,-z,max-page-size=16384 -o lib/arm64-v8a/libprobe.so
"$BT/aapt2" link -o base.apk -I "$JAR" --manifest "$HERE/AndroidManifest.xml" \
    --min-sdk-version 33 --target-sdk-version 35 --debug-mode
zip -q -0 base.apk lib/arm64-v8a/libprobe.so
"$BT/zipalign" -f -P 16 4 base.apk aligned.apk
[[ -f $HOME/android/debug.keystore ]] || keytool -genkeypair -keystore "$HOME/android/debug.keystore" \
    -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 \
    -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
"$BT/apksigner" sign --ks "$HOME/android/debug.keystore" --ks-pass pass:android --out probe.apk aligned.apk
ls -la probe.apk
