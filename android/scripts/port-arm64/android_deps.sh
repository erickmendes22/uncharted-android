#!/usr/bin/env bash
# Dependencies of the uncharted_pc renderer (gpu/) for Android arm64 (NDK r30, API 33, libc++_shared).
# Versions as in the MSYS2 reference build. Result: $DEPS/prefix (lib/pkgconfig, lib/cmake).
# Run in WSL: bash android_deps.sh [name...]  (no arguments: all; finished ones are skipped)
set -euo pipefail
NDK=${NDK:-$HOME/android/android-ndk-r30}
API=${API:-33}
DEPS=${DEPS:-$HOME/src/android-deps}
P=$DEPS/prefix
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
J=$(nproc)
mkdir -p "$DEPS/src" "$DEPS/build" "$P"

cmake_dep() {  # name srcdir [cmake args...]
    local name=$1 src=$2; shift 2
    cmake -S "$src" -B "$DEPS/build/$name" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=$API -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_FIND_ROOT_PATH="$P" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON "$@" > "$DEPS/build/$name.log" 2>&1
    cmake --build "$DEPS/build/$name" -j$J >> "$DEPS/build/$name.log" 2>&1
    cmake --install "$DEPS/build/$name" >> "$DEPS/build/$name.log" 2>&1
}
fetch() {  # name url tag
    [ -d "$DEPS/src/$1" ] || git clone -q --depth 1 --branch "$3" --recurse-submodules --shallow-submodules "$2" "$DEPS/src/$1"
}
done_mark() { touch "$DEPS/build/$1.done"; echo "ok  $1"; }
want() { [ ! -f "$DEPS/build/$1.done" ] && { [ -z "$SEL" ] || [[ " $SEL " == *" $1 "* ]]; }; }
SEL="$*"

if want vulkan-headers; then
    fetch vulkan-headers https://github.com/KhronosGroup/Vulkan-Headers.git v1.4.363
    cmake_dep vulkan-headers "$DEPS/src/vulkan-headers"; done_mark vulkan-headers
fi
if want fmt; then
    fetch fmt https://github.com/fmtlib/fmt.git 12.2.0
    cmake_dep fmt "$DEPS/src/fmt" -DFMT_TEST=OFF -DFMT_DOC=OFF; done_mark fmt
fi
if want robin-map; then
    fetch robin-map https://github.com/Tessil/robin-map.git v1.4.1
    cmake_dep robin-map "$DEPS/src/robin-map"; done_mark robin-map
fi
if want vma; then
    fetch vma https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git v3.3.0
    cmake_dep vma "$DEPS/src/vma"; done_mark vma
fi
if want xxhash; then
    fetch xxhash https://github.com/Cyan4973/xxHash.git v0.8.4
    cmake_dep xxhash "$DEPS/src/xxhash/build/cmake" -DBUILD_SHARED_LIBS=OFF -DXXHASH_BUILD_XXHSUM=OFF
    done_mark xxhash
fi
if want zycore; then
    fetch zydis https://github.com/zyantific/zydis.git v4.1.1
    # Zydis installs Zycore only inside itself; zydis-config.cmake needs its package.
    cmake_dep zycore "$DEPS/src/zydis/dependencies/zycore" -DZYCORE_BUILD_SHARED_LIB=OFF
    done_mark zycore
fi
if want zydis; then
    fetch zydis https://github.com/zyantific/zydis.git v4.1.1
    cmake_dep zydis "$DEPS/src/zydis" -DZYDIS_BUILD_TOOLS=OFF -DZYDIS_BUILD_EXAMPLES=OFF -DZYDIS_BUILD_DOXYGEN=OFF
    done_mark zydis
fi
if want sdl3; then
    fetch sdl3 https://github.com/libsdl-org/SDL.git release-3.4.18
    cmake_dep sdl3 "$DEPS/src/sdl3" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF
    done_mark sdl3
fi
if want boost; then
    # Headers only (Boost::headers): take the release archive.
    if [ ! -d "$P/include/boost" ]; then
        curl -sL -o "$DEPS/src/boost.tar.gz" https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz
        tar -xzf "$DEPS/src/boost.tar.gz" -C "$DEPS/src"
        cp -r "$DEPS/src/boost_1_92_0/boost" "$P/include/"
    fi
    done_mark boost
fi
if want ffmpeg; then
    fetch ffmpeg https://github.com/FFmpeg/FFmpeg.git n9.0.2
    mkdir -p "$DEPS/build/ffmpeg"; cd "$DEPS/build/ffmpeg"
    "$DEPS/src/ffmpeg/configure" --prefix="$P" --enable-cross-compile --target-os=android --arch=aarch64 \
        --cc="$TC/aarch64-linux-android$API-clang" --cxx="$TC/aarch64-linux-android$API-clang++" \
        --nm="$TC/llvm-nm" --ar="$TC/llvm-ar" --ranlib="$TC/llvm-ranlib" --strip="$TC/llvm-strip" \
        --enable-shared --disable-static --enable-pic --disable-programs --disable-doc \
        --disable-avdevice --disable-network --disable-debug > "$DEPS/build/ffmpeg.log" 2>&1
    # avutil_configuration() and the data directories would carry this machine's paths
    sed -i "s#$P#/prefix#g; s#$TC#<ndk-toolchain>#g" config.h
    make -j$J >> "$DEPS/build/ffmpeg.log" 2>&1 && make install >> "$DEPS/build/ffmpeg.log" 2>&1
    done_mark ffmpeg
fi
echo "prefix: $P"
