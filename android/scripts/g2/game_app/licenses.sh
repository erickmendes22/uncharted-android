#!/usr/bin/env bash
# licenses.sh <out file>: the license texts of everything the APK contains (android/LICENSES.md) in
# one file - the public build's assets/licenses.txt, shown by the launcher. Run in WSL after the
# builds (the source trees: the port, FEX, Mesa, libadrenotools, android_deps.sh's sources, the NDK).
set -euo pipefail
OUT=${1:?usage: licenses.sh <out file>}
PORT=${PORT:-$HOME/src/uncharted-android}
FEX=${FEX:-$HOME/src/fex}
MESA=${MESA:-$HOME/src/mesa}
ADRENOTOOLS=${ADRENOTOOLS:-$HOME/src/libadrenotools}
DEPS=${DEPS:-$HOME/src/android-deps}/src
NDK=${NDK:-$HOME/android/android-ndk-r30}
GT=$PORT/gpu/third_party

section() { # <title> <file> [lines]
    # (short rules: the launcher shows the file in a dialog ~55 columns wide)
    printf '\n================================================\n%s\n' "$1"
    printf '================================================\n\n'
    if [ -n "${3:-}" ]; then sed -n "$3p" "$2"; else cat "$2"; fi
}
{
    echo "This app: uncharted_pc for Android (uncharted-android), GNU GPL version 3 or later (LSFG build); it"
    echo "includes the shadPS4 video core and shader recompiler (GPL-2.0-or-later). The source code:"
    echo "the uncharted-android repository with its FEX and Mesa patches."
    section "uncharted_pc, uncharted-android, shadPS4 - GNU General Public License v2" "$PORT/LICENSE"
    section "LSFG: Eden, lsfg-vk, ARMSX2, RedVorteX Android integration - GNU GPL v3 or later" "$PORT/LICENSE.GPL-3.0"
    section "FEXCore - MIT" "$FEX/LICENSE"
    section "fmt (FEX) - MIT" "$FEX/External/fmt/LICENSE"
    section "xxHash - BSD-2-Clause" "$FEX/External/xxhash/LICENSE"
    section "Cephes (FEX) - BSD" "$FEX/External/cephes/LICENSE"
    section "Berkeley SoftFloat 3e (FEX) - BSD-3-Clause" "$(ls "$FEX"/External/SoftFloat-3e/src/*.c | head -1)" "3,33"
    section "rpmalloc (FEX)" "$FEX/External/rpmalloc/LICENSE"
    section "tiny-json (FEX) - MIT" "$FEX/External/tiny-json/LICENSE"
    section "unordered_dense (FEX) - MIT" "$FEX/External/unordered_dense/LICENSE"
    section "range-v3 (FEX) - Boost Software License 1.0" "$FEX/External/range-v3/LICENSE.txt"
    section "jemalloc (FEX) - BSD-2-Clause" "$FEX/External/jemalloc_glibc/COPYING"
    section "cpp-optparse (FEX) - MIT" "$FEX/Source/Common/cpp-optparse/LICENSE"
    section "Mesa (Turnip) - MIT and others" "$MESA/docs/license.rst"
    section "libadrenotools - BSD-2-Clause" "$ADRENOTOOLS/LICENSE"
    section "SDL3 - Zlib" "$DEPS/sdl3/LICENSE.txt"
    section "FFmpeg - GNU Lesser General Public License v2.1" "$DEPS/ffmpeg/COPYING.LGPLv2.1"
    section "Vulkan Memory Allocator - MIT" "$DEPS/vma/LICENSE.txt"
    section "Vulkan headers" "$DEPS/vulkan-headers/LICENSE.md"
    section "Boost - Boost Software License 1.0" "$DEPS/boost_1_92_0/LICENSE_1_0.txt"
    section "Zydis - MIT" "$DEPS/zydis/LICENSE"
    section "robin-map - MIT" "$DEPS/robin-map/LICENSE"
    section "LibAtrac9 - MIT" "$PORT/third_party/LibAtrac9/LICENSE"
    section "sirit - BSD-3-Clause" "$GT/sirit/LICENSE.txt"
    section "magic_enum - MIT" "$GT/magic_enum/LICENSE"
    section "miniz - MIT" "$GT/miniz/LICENSE"
    section "half - MIT" "$GT/half/LICENSE.txt"
    section "Dear ImGui - MIT" "$GT/imgui/LICENSE.txt"
    section "FSR for Vulkan - MIT" "$GT/fsr-vulkan/LICENSE.txt"
    section "xbyak - BSD-3-Clause" "$GT/xbyak/COPYRIGHT"
    section "DejaVu fonts" "$GT/fonts/LICENSE-DejaVu.txt"
    # LLVM's license from the NDK's toolchain notices (the whole file is ~0.8 MB of other projects)
    awk '/The LLVM Project is under the Apache License/ {on = 1}
         /Software from third parties included in the LLVM Project/ {exit} on' \
        "$NDK/NOTICE.toolchain" > "$OUT.llvm"
    section "LLVM libc++ (the NDK's libc++_shared.so) - Apache-2.0 WITH LLVM-exception" "$OUT.llvm"
} > "$OUT"
rm -f "$OUT.llvm"
ls -la "$OUT"
