#!/usr/bin/env bash
# WSL2 Ubuntu 24.04: LLVM from apt.llvm.org + Remill (as in Remill's CI, .github/workflows/build.yml).
# The system part, as root:  wsl -u root -- bash setup_remill.sh root
# The build, as the user:    wsl -- bash setup_remill.sh build
set -euo pipefail
LLVM_VERSION=${LLVM_VERSION:-22}
SRC=${SRC:-$HOME/src}

case ${1:-} in
root)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq lsb-release wget software-properties-common gnupg \
        cmake ninja-build python-is-python3 python3-pip python3-setuptools python3-venv git
    wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
    bash /tmp/llvm.sh "$LLVM_VERSION"
    apt-get install -y -qq "llvm-$LLVM_VERSION-dev" "lld-$LLVM_VERSION" "clang-$LLVM_VERSION"
    ;;
build)
    export CC=clang-$LLVM_VERSION CXX=clang++-$LLVM_VERSION
    LLVM_PREFIX=$(llvm-config-"$LLVM_VERSION" --prefix)
    mkdir -p "$SRC"
    [[ -d $SRC/remill ]] || git clone https://github.com/lifting-bits/remill.git "$SRC/remill"
    cd "$SRC/remill"
    git log -1 --format='remill %h %cs %s'
    cmake -G Ninja -S dependencies -B dependencies/build -DUSE_EXTERNAL_LLVM=ON -DENABLE_SLEIGH=ON \
        "-DCMAKE_PREFIX_PATH:PATH=$LLVM_PREFIX"
    cmake --build dependencies/build
    [[ -d .venv ]] || { python3 -m venv .venv && .venv/bin/pip install -q scripts/diff_tester_export_insns; }
    . .venv/bin/activate
    cmake -G Ninja -B build -DREMILL_FETCH_SLEIGH=OFF -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_PREFIX_PATH:PATH=$PWD/dependencies/install" "-DCMAKE_INSTALL_PREFIX=$PWD/install"
    cmake --build build
    cmake --install build
    # Smoke test from the CI: mov dword [rdx+1], 1 -> IR
    "install/bin/remill-lift-$LLVM_VERSION" --arch amd64 --ir_out /dev/stdout --bytes c704ba01000000 | tail -20
    ;;
*) echo "usage: $0 root|build" >&2; exit 1 ;;
esac
