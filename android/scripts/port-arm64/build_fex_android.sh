#!/usr/bin/env bash
# FEXCore for Android arm64 (NDK r30, API 33, libc++_shared): the libraries the loader links.
# Sources: FEX f2b679f (the same revision as in Bachata S4) with patches/fex-*.patch (the Android
# build, guest pause -> wfe, the smoke executable optional); android/scripts/build_all.sh fetches
# and patches them.
# SMOKE=<file>: also an embedding smoke test (Bachata S4's runtime/probes/fexcore-smoke.cpp; on the
# G2: adb push $OUT/Bin/fexcore-smoke + libc++_shared.so -> LD_LIBRARY_PATH=. ./fexcore-smoke).
set -euo pipefail
NDK=${NDK:-$HOME/android/android-ndk-r30}
SRC=${SRC:-$HOME/src/fex}
OUT=${OUT:-$HOME/src/fex-android}
SMOKE=${SMOKE:-}
# the libraries the port's loader links (gpu/CMakeLists.txt FEX_LIBS) and their dependencies
TARGETS=(FEXCore Common CommonTools JemallocLibs rpmalloc)
[ -n "$SMOKE" ] && TARGETS+=(fexcore-smoke)
# TUNE_CPU: FEX's default ("native") reads the build machine's /proc/cpuinfo; on an x86-64 host
# that ends in cortex-a78, so it is set here. No SVE: Qualcomm disables it (SIGILL on the G2).
cmake -S "$SRC" -B "$OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=33 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_FEXCONFIG=OFF -DBUILD_TESTING=OFF -DBUILD_THUNKS=OFF -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF \
    -DENABLE_LTO=OFF -DENABLE_OFFLINE_TELEMETRY=OFF -DENABLE_CCACHE=OFF -DTUNE_CPU="${TUNE_CPU:-cortex-a78}" \
    -DBUILD_FEXCORE_ONLY=ON -DFEXCORE_SMOKE_SOURCE="$SMOKE"
cmake --build "$OUT" --target "${TARGETS[@]}"
grep -o -- "-mcpu=[^ ]*" "$OUT/build.ninja" | sort -u
