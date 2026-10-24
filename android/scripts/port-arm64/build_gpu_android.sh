#!/usr/bin/env bash
# The uncharted_pc renderer (gpu/, libbbgpu.so) and GPU tests for Android arm64.
# Dependencies: android_deps.sh. Sources: this repository (the full clone in WSL; from the Windows clone
# without the port's sources: ~/src/uncharted-android).
# bash build_gpu_android.sh [targets...]   (default: bbgpu scene-resolution-test)
# FEX_SOURCE_DIR, FEX_BUILD_DIR, ADRENOTOOLS_SOURCE_DIR, ADRENOTOOLS_BUILD_DIR: where FEXCore
# (build_fex_android.sh) and libadrenotools are (default: ~/src/fex, ~/src/fex-android,
# ~/src/libadrenotools, ~/src/adrenotools-android).
set -euo pipefail
NDK=${NDK:-$HOME/android/android-ndk-r30}
API=${API:-33}
P=${DEPS:-$HOME/src/android-deps}/prefix
ROOT=$(cd -- "$(dirname -- "$0")/../../.." && pwd)
[ -f "$ROOT/gpu/CMakeLists.txt" ] || ROOT=$HOME/src/uncharted-android
SRC=${SRC:-$ROOT}
OUT=${OUT:-$HOME/src/uncharted-android-build}
TARGETS=${*:-bbgpu scene-resolution-test}
# Source paths in the binaries (__FILE__ in shadPS4's log and assert messages) relative to the tree,
# not this machine's directories. The NDK's toolchain file puts its own flags before these.
MAP="-ffile-prefix-map=$SRC/="
DIRS=()
for v in FEX_SOURCE_DIR FEX_BUILD_DIR ADRENOTOOLS_SOURCE_DIR ADRENOTOOLS_BUILD_DIR; do
    if [ -n "${!v:-}" ]; then DIRS+=("-D$v=${!v}"); fi
done
export PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=
# The port's patches to the FSR-Vulkan submodule (gpu/patches/fsr-vulkan), applied to its working
# tree once, as the port's build.sh does on a PC (0003: the frame generation's leak).
for patch in "$SRC"/gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C "$SRC/gpu/third_party/fsr-vulkan" apply --reverse --check "$patch" 2>/dev/null; then
        git -C "$SRC/gpu/third_party/fsr-vulkan" apply "$patch"
    fi
done
cmake -S "$SRC/gpu" -B "$OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=$API -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_FIND_ROOT_PATH="$P" -DCMAKE_PREFIX_PATH="$P" \
    -DCMAKE_C_FLAGS="$MAP" -DCMAKE_CXX_FLAGS="$MAP" \
    -DVulkan_INCLUDE_DIR="$P/include" -DBB_LTO=OFF -DBB_NET=OFF \
    -DANDROID_ALLOW_UNDEFINED_SYMBOLS=ON "${DIRS[@]}"  # runtime_*: from the loader (or test stubs)
cmake --build "$OUT" -j"$(nproc)" --target $TARGETS
ls -la "$OUT"/libbbgpu.so "$OUT"/scene-resolution-test 2>/dev/null || true
