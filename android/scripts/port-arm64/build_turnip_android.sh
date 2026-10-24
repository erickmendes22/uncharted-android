#!/usr/bin/env bash
# Turnip (Mesa Vulkan for Adreno, KGSL backend) for Android arm64 -> $OUT/libvulkan_freedreno.so.
# Run in WSL. Sources: Mesa 26.2.5 (git clone --depth 1 -b mesa-26.2.5 https://gitlab.freedesktop.org/mesa/mesa.git)
# with scripts/port-arm64/patches/mesa-*.patch applied in order (KGSL fixes, the A735 workarounds,
# diagnostics: patches/README.md); meson and mako in a venv (python3 -m venv ...; pip install
# meson==1.12.1 mako==1.4.3 pyyaml==6.0.3 packaging==26.3); bison and flex; ccache if installed.
# android/scripts/build_all.sh does all of it.
set -euo pipefail
NDK=${NDK:-$HOME/android/android-ndk-r30}
API=${API:-33}
SRC=${SRC:-$HOME/src/mesa}
OUT=${OUT:-$HOME/src/turnip-android}
VENV=${VENV:-$HOME/src/mesa-venv}
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
export PATH=$VENV/bin:$PATH
CCACHE=
if command -v ccache > /dev/null; then CCACHE="'ccache', "; fi
mkdir -p "$OUT"
cat > "$OUT/android-aarch64.ini" <<EOF
[binaries]
ar = '$TC/llvm-ar'
c = [$CCACHE'$TC/aarch64-linux-android$API-clang']
cpp = [$CCACHE'$TC/aarch64-linux-android$API-clang++', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables']
c_ld = 'lld'
cpp_ld = 'lld'
strip = '$TC/llvm-strip'
pkg-config = ['env', 'PKG_CONFIG_LIBDIR=$OUT/pkgconfig', '/usr/bin/pkg-config']

[built-in options]
cpp_link_args = ['-static-libstdc++']

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
EOF
mkdir -p "$OUT/pkgconfig"
cd "$SRC"
if [ ! -d "$OUT/build" ]; then
    meson setup "$OUT/build" --cross-file "$OUT/android-aarch64.ini" \
        -Dbuildtype=release -Dplatforms=android -Dplatform-sdk-version=$API -Dandroid-stub=true \
        -Dgallium-drivers= -Degl=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Dglx=disabled \
        -Dvulkan-drivers=freedreno -Dfreedreno-kmds=kgsl \
        -Dvulkan-beta=true -Db_lto=false -Dstrip=true -Dllvm=disabled -Dvalgrind=disabled \
        -Dandroid-libbacktrace=disabled
fi
ninja -C "$OUT/build" src/freedreno/vulkan/libvulkan_freedreno.so
cp "$OUT/build/src/freedreno/vulkan/libvulkan_freedreno.so" "$OUT/"
ls -la "$OUT/libvulkan_freedreno.so"
