#!/usr/bin/env bash
set -euo pipefail
: "${WORK:?}" "${NDK:?}"
src="$WORK/orbis-pkg"
if [ ! -d "$src/.git" ]; then git init -q "$src"; git -C "$src" remote add origin https://github.com/videobitva/orbis.git; fi
git -C "$src" fetch -q --depth 1 origin 13f1294a87a53d54f82feec56acbfd03f50667a9
git -C "$src" checkout -q --detach FETCH_HEAD
rustup target add aarch64-linux-android
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang"
export CC_aarch64_linux_android="$CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER"
export AR_aarch64_linux_android="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
cargo build --manifest-path "$src/Cargo.toml" --locked --release --target aarch64-linux-android --target-dir "$WORK/orbis-cargo" -p orbis-pkg-util
mkdir -p "$WORK/pkg"
cp "$WORK/orbis-cargo/aarch64-linux-android/release/orbis-pkg-util" "$WORK/pkg/liborbis_pkg.so"
