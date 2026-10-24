#!/usr/bin/env bash
# Our semantics/tests/patches -> the Remill tree (branch uncharted from upstream 56918a8), rebuild,
# install, the amd64_avx tests (lifted vs native on this x86 host).
# The source of truth is this directory; ~/src/remill only has copies. Run in WSL:
#   bash /mnt/c/repos/uncharted-g2/android/scripts/remill/apply.sh [--no-tests]
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
R=${REMILL:-$HOME/src/remill}
cd "$R"
inc=lib/Arch/X86/Runtime/Instructions.cpp
# Patches of upstream files (idempotent: already applied ones are skipped).
for p in "$HERE"/patches/*.patch; do
    [[ -e $p ]] || continue
    if git apply --check "$p" 2>/dev/null; then
        git apply "$p" && touch "$inc" && echo "applied $(basename "$p")"
    elif ! git apply --reverse --check "$p" 2>/dev/null; then
        echo "patch does not apply: $p" >&2; exit 1
    fi
done
# The semantics (.bc) build does not see #include dependencies: touch Instructions.cpp ourselves.
if ! cmp -s "$HERE/PS4_AVX.cpp" lib/Arch/X86/Semantics/PS4_AVX.cpp; then
    cp "$HERE/PS4_AVX.cpp" lib/Arch/X86/Semantics/PS4_AVX.cpp
    touch "$inc"
fi
grep -q 'Semantics/PS4_AVX.cpp' "$inc" ||
    sed -i 's|^#include "lib/Arch/X86/Semantics/XSAVE.cpp"$|&\n#include "lib/Arch/X86/Semantics/PS4_AVX.cpp"|' "$inc"
mkdir -p tests/X86/PS4
cp "$HERE"/tests/*.S tests/X86/PS4/
grep -q 'tests/X86/PS4/PS4.S' tests/X86/Tests.S ||
    sed -i 's|^#include "tests/X86/AVX/VMASKMOV.S"$|&\n#include "tests/X86/PS4/PS4.S"|' tests/X86/Tests.S
. .venv/bin/activate
export CC=clang-22 CXX=clang++-22
cmake --build build > "$HOME/src/remill-build.log" 2>&1 || { grep -E "error|Error" -A3 "$HOME/src/remill-build.log" | head -60; exit 1; }
cmake --install build > /dev/null
echo "remill rebuilt and installed"
[[ ${1:-} == --no-tests ]] && exit 0
cmake --build build --target run-amd64_avx-tests > "$HOME/src/remill-tests-build.log" 2>&1 ||
    { grep -E "error|Error" -A3 "$HOME/src/remill-tests-build.log" | head -60; exit 1; }
build/tests/X86/run-amd64_avx-tests > "$HOME/src/remill-tests.log" 2>&1 || true
grep -E '^\[==========\] [0-9]+ tests|^\[  PASSED  \]|^\[  FAILED  \] [0-9]+ tests' "$HOME/src/remill-tests.log"
grep -E '^\[  FAILED  \] .*SemanticsMatchNative' "$HOME/src/remill-tests.log" |
    sed -E 's/.*SemanticsMatchNative\///; s/,.*//' | sort -u | tr '\n' ' '
echo
