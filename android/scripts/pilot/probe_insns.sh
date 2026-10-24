#!/usr/bin/env bash
# Does Remill support particular encodings (SSE vs VEX, BMI1, MOVBE...)? Run in WSL.
L=${REMILL_LIFT:-$HOME/src/remill/install/bin/remill-lift-22}
probe() {
    local name=$1 bytes=$2 out r
    out=$("$L" --arch amd64_avx --os linux --bytes "$bytes" --ir_out /dev/stdout 2>&1)
    if grep -q '__remill_error\|i32 257' <<< "$out"; then r=NO
    elif grep -q '^define' <<< "$out"; then r=ok
    else r="lift failed: $(tail -1 <<< "$out" | cut -c1-80)"; fi
    printf '%-14s %-24s %s\n' "$name" "$bytes" "$r"
}
probe pshufb       660f3800e1
probe vpshufb      c4e26100e1
probe pmaddwd      660ff5eb
probe vpmaddwd     c5d9f5eb
probe roundsd      660f3a0bec09
probe vroundsd     c4e3590bec09
probe shufps       0fc6c800
probe vshufps      c5f8c6c800
probe blsr         c4e2b8f3ca
probe andn         c4e2e8f2c1
probe bextr        c4e2f0f7c2
probe tzcnt        f3480fbcc1
probe lzcnt        f3480fbdc1
probe popcnt       f3480fb8c1
probe movbe_load   480f38f007
probe vmulps_ymm   c5fc59c1
probe vinsertf128  c4e37d18c101
probe vextractf128 c4e37d19c101
probe vpextrd      c4e37916c001
probe vcvttsd2si   c4e1fb2cc0
probe vmaxsd       c5fb5fc1
probe vbroadcastss c4e27d1807
probe vperm2f128   c4e37d06c101
probe vblendvps    c4e37d4ac110
probe vcvtph2ps    c4e2791307
probe vzeroupper   c5f877
probe lock_xadd    f0480fc107
probe lock_add     f0480107
probe pause        f390
probe rdtsc        0f31
probe cpuid        0fa2
