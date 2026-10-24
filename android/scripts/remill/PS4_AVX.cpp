/*
 * VEX (AVX/F16C/BMI1) forms used by PS4 (Jaguar, clang -march=btver2) code that upstream Remill
 * does not lift. Found by the uncharted-g2 instruction inventory (notes/inventory-remill-*.md).
 *
 * VEX.128 writes zero bits 255:128 (VV128W). Lane-crossing rules for 256-bit forms follow the
 * Intel SDM: operations are done per 128-bit lane.
 *
 * Licensed under the Apache License, Version 2.0 (same as Remill).
 */

namespace {

// ---------------------------------------------------------------------------------------------
// Permutes and blends.

// VPERMILPS imm: each 128-bit lane, dst[j] = src[lane + imm[2j+1:2j]].
template <typename D, typename S1>
DEF_SEM(VPERMILPS_IMM, D dst, S1 src1, I8 src2) {
  auto src_vec = UReadV32(src1);
  auto dst_vec = UClearV32(src_vec);
  auto imm = Read(src2);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(src_vec); ++i) {
    std::size_t lane = i & ~3ul;
    auto sel = static_cast<std::size_t>((imm >> ((i & 3u) * 2u)) & 3u);
    dst_vec = UInsertV32(dst_vec, i, UExtractV32(src_vec, lane + sel));
  }
  UWriteV32(dst, dst_vec);
  return memory;
}

// VPERMILPD imm: dst[i] = src[lane + imm[i]].
template <typename D, typename S1>
DEF_SEM(VPERMILPD_IMM, D dst, S1 src1, I8 src2) {
  auto src_vec = UReadV64(src1);
  auto dst_vec = UClearV64(src_vec);
  auto imm = Read(src2);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(src_vec); ++i) {
    std::size_t lane = i & ~1ul;
    auto sel = static_cast<std::size_t>((imm >> i) & 1u);
    dst_vec = UInsertV64(dst_vec, i, UExtractV64(src_vec, lane + sel));
  }
  UWriteV64(dst, dst_vec);
  return memory;
}

// SHUFPS for any width: per 128-bit lane, low two from src1, high two from src2.
template <typename D, typename S1, typename S2>
DEF_SEM(SHUFPS_LANES, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = UReadV32(src1);
  auto b = UReadV32(src2);
  auto dst_vec = UClearV32(a);
  auto imm = Read(src3);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    std::size_t lane = i & ~3ul;
    std::size_t j = i & 3u;
    auto sel = static_cast<std::size_t>((imm >> (j * 2u)) & 3u);
    auto v = j < 2 ? UExtractV32(a, lane + sel) : UExtractV32(b, lane + sel);
    dst_vec = UInsertV32(dst_vec, i, v);
  }
  UWriteV32(dst, dst_vec);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(BLENDPS, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = UReadV32(src1);
  auto b = UReadV32(src2);
  auto imm = Read(src3);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    if ((imm >> i) & 1u) {
      a = UInsertV32(a, i, UExtractV32(b, i));
    }
  }
  UWriteV32(dst, a);
  return memory;
}

// VBLENDVPS dst, src1, src2, mask: element from src2 where the mask element's sign bit is set.
template <typename D, typename S1, typename S2, typename S3>
DEF_SEM(BLENDVPS, D dst, S1 src1, S2 src2, S3 src3) {
  auto a = UReadV32(src1);
  auto b = UReadV32(src2);
  auto m = UReadV32(src3);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = UInsertV32(a, i, Select(SignFlag(UExtractV32(m, i)), UExtractV32(b, i), UExtractV32(a, i)));
  }
  UWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1, typename S2, typename S3>
DEF_SEM(BLENDVPD, D dst, S1 src1, S2 src2, S3 src3) {
  auto a = UReadV64(src1);
  auto b = UReadV64(src2);
  auto m = UReadV64(src3);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = UInsertV64(a, i, Select(SignFlag(UExtractV64(m, i)), UExtractV64(b, i), UExtractV64(a, i)));
  }
  UWriteV64(dst, a);
  return memory;
}

template <typename D, typename S1, typename S2, typename S3>
DEF_SEM(PBLENDVB, D dst, S1 src1, S2 src2, S3 src3) {
  auto a = UReadV8(src1);
  auto b = UReadV8(src2);
  auto m = UReadV8(src3);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = UInsertV8(a, i, Select(SignFlag(UExtractV8(m, i)), UExtractV8(b, i), UExtractV8(a, i)));
  }
  UWriteV8(dst, a);
  return memory;
}

// ---------------------------------------------------------------------------------------------
// Element insert/extract.

// INSERTPS, register source: element imm[7:6] of src2 → element imm[5:4] of src1; zero mask imm[3:0].
template <typename D, typename S1, typename S2>
DEF_SEM(INSERTPS_REG, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = UReadV32(src1);
  auto b = UReadV32(src2);
  auto imm = Read(src3);
  auto v = UExtractV32(b, static_cast<std::size_t>((imm >> 6) & 3u));
  a = UInsertV32(a, static_cast<std::size_t>((imm >> 4) & 3u), v);
  _Pragma("unroll") for (std::size_t i = 0; i < 4; ++i) {
    if ((imm >> i) & 1u) {
      a = UInsertV32(a, i, 0_u32);
    }
  }
  UWriteV32(dst, a);
  return memory;
}

// INSERTPS, memory source: the 32-bit value itself (imm[7:6] ignored).
template <typename D, typename S1>
DEF_SEM(INSERTPS_MEM, D dst, S1 src1, MV32 src2, I8 src3) {
  auto a = UReadV32(src1);
  auto v = UExtractV32(UReadV32(src2), 0);
  auto imm = Read(src3);
  a = UInsertV32(a, static_cast<std::size_t>((imm >> 4) & 3u), v);
  _Pragma("unroll") for (std::size_t i = 0; i < 4; ++i) {
    if ((imm >> i) & 1u) {
      a = UInsertV32(a, i, 0_u32);
    }
  }
  UWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1>
DEF_SEM(EXTRACTPS, D dst, S1 src1, I8 src2) {
  auto v = UExtractV32(UReadV32(src1), static_cast<std::size_t>(Read(src2) & 3u));
  WriteZExt(dst, v);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(PINSRD, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = UReadV32(src1);
  a = UInsertV32(a, static_cast<std::size_t>(Read(src3) & 3u), Read(src2));
  UWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(PINSRQ, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = UReadV64(src1);
  a = UInsertV64(a, static_cast<std::size_t>(Read(src3) & 1u), Read(src2));
  UWriteV64(dst, a);
  return memory;
}

// VEXTRACTF128 dst128, ymm, imm.
template <typename D>
DEF_SEM(VEXTRACTF128, D dst, V256 src1, I8 src2) {
  uint128v1_t res = {};
  res.elems[0] = UExtractV128(UReadV128(src1), static_cast<std::size_t>(Read(src2) & 1u));
  UWriteV128(dst, res);
  return memory;
}

// PALIGNR with separate src1 (VEX): bytes of (src1:src2) shifted right by imm bytes.
template <typename D, typename S1, typename S2>
DEF_SEM(PALIGNR_VEX, D dst, S1 src1, S2 src2, I8 src3) {
  auto hi = UReadV8(src1);
  auto lo = UReadV8(src2);
  auto dst_vec = UClearV8(hi);
  auto shift = static_cast<std::size_t>(Read(src3));
  _Pragma("unroll") for (std::size_t i = 0; i < 16; ++i) {
    std::size_t idx = i + shift;
    uint8_t v = 0;
    if (idx < 16) {
      v = UExtractV8(lo, idx);
    } else if (idx < 32) {
      v = UExtractV8(hi, idx - 16);
    }
    dst_vec = UInsertV8(dst_vec, i, v);
  }
  UWriteV8(dst, dst_vec);
  return memory;
}

// ---------------------------------------------------------------------------------------------
// Floating point.

// DPPS: products selected by imm[7:4], summed as (p0 + p1) + (p2 + p3) like the hardware,
// result broadcast to the elements selected by imm[3:0], others +0.0. Per 128-bit lane.
template <typename D, typename S1, typename S2>
DEF_SEM(DPPS, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = FReadV32(src1);
  auto b = FReadV32(src2);
  auto out = FClearV32(a);
  auto imm = Read(src3);
  _Pragma("unroll") for (std::size_t lane = 0; lane < NumVectorElems(a); lane += 4) {
    float32_t p[4];
    _Pragma("unroll") for (std::size_t j = 0; j < 4; ++j) {
      p[j] = ((imm >> (4 + j)) & 1u) ? FMul(FExtractV32(a, lane + j), FExtractV32(b, lane + j))
                                     : 0.0f;
    }
    auto sum = FAdd(FAdd(p[0], p[1]), FAdd(p[2], p[3]));
    _Pragma("unroll") for (std::size_t j = 0; j < 4; ++j) {
      out = FInsertV32(out, lane + j, ((imm >> j) & 1u) ? sum : 0.0f);
    }
  }
  FWriteV32(dst, out);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(HSUBPS, D dst, S1 src1, S2 src2) {
  auto a = FReadV32(src1);
  auto b = FReadV32(src2);
  auto out = FClearV32(a);
  out = FInsertV32(out, 0, FSub(FExtractV32(a, 0), FExtractV32(a, 1)));
  out = FInsertV32(out, 1, FSub(FExtractV32(a, 2), FExtractV32(a, 3)));
  out = FInsertV32(out, 2, FSub(FExtractV32(b, 0), FExtractV32(b, 1)));
  out = FInsertV32(out, 3, FSub(FExtractV32(b, 2), FExtractV32(b, 3)));
  FWriteV32(dst, out);
  return memory;
}

template <typename D, typename S1>
DEF_SEM(SQRTPS, D dst, S1 src1) {
  auto a = FReadV32(src1);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = FInsertV32(a, i, SquareRoot32(memory, state, FExtractV32(a, i)));
  }
  FWriteV32(dst, a);
  return memory;
}

// RCPPS / RSQRTPS: the hardware returns a ~12-bit approximation whose exact bits are
// implementation specific (Jaguar ≠ Zen ≠ Intel). We return the correctly rounded value, which
// is within the architectural error bound of every implementation.
template <typename D, typename S1>
DEF_SEM(RCPPS, D dst, S1 src1) {
  auto a = FReadV32(src1);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = FInsertV32(a, i, FDiv(1.0f, FExtractV32(a, i)));
  }
  FWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1>
DEF_SEM(RSQRTPS, D dst, S1 src1) {
  auto a = FReadV32(src1);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = FInsertV32(a, i, FDiv(1.0f, SquareRoot32(memory, state, FExtractV32(a, i))));
  }
  FWriteV32(dst, a);
  return memory;
}

// ROUND* imm: [1:0] mode (nearest-even, down, up, truncate) unless imm[2] — then MXCSR.RC.
// MXCSR.RC is not modelled per instruction here; PS4 code runs with round-to-nearest.
ALWAYS_INLINE static float32_t RoundF32(float32_t v, uint8_t imm) {
  switch ((imm & 4u) ? 0u : (imm & 3u)) {
    case 1: return __builtin_floorf(v);
    case 2: return __builtin_ceilf(v);
    case 3: return __builtin_truncf(v);
    default: return __builtin_roundevenf(v);
  }
}

ALWAYS_INLINE static float64_t RoundF64(float64_t v, uint8_t imm) {
  switch ((imm & 4u) ? 0u : (imm & 3u)) {
    case 1: return __builtin_floor(v);
    case 2: return __builtin_ceil(v);
    case 3: return __builtin_trunc(v);
    default: return __builtin_roundeven(v);
  }
}

template <typename D, typename S1>
DEF_SEM(ROUNDPS, D dst, S1 src1, I8 src2) {
  auto a = FReadV32(src1);
  auto imm = Read(src2);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    a = FInsertV32(a, i, RoundF32(FExtractV32(a, i), imm));
  }
  FWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(ROUNDSS, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = FReadV32(src1);
  a = FInsertV32(a, 0, RoundF32(FExtractV32(FReadV32(src2), 0), Read(src3)));
  FWriteV32(dst, a);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(ROUNDSD, D dst, S1 src1, S2 src2, I8 src3) {
  auto a = FReadV64(src1);
  a = FInsertV64(a, 0, RoundF64(FExtractV64(FReadV64(src2), 0), Read(src3)));
  FWriteV64(dst, a);
  return memory;
}

// F16C VCVTPH2PS: half → single is exact. SNaN is quieted (bit 22), payload kept.
ALWAYS_INLINE static uint32_t HalfToFloatBits(uint16_t h) {
  uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
  uint32_t exp = (h >> 10) & 0x1fu;
  uint32_t man = h & 0x3ffu;
  if (exp == 0x1fu) {
    return man ? (sign | 0x7fc00000u | (man << 13)) : (sign | 0x7f800000u);
  }
  if (exp == 0) {
    if (!man) {
      return sign;
    }
    // Subnormal half: normalize.
    int e = -1;
    do {
      man <<= 1;
      ++e;
    } while (!(man & 0x400u));
    return sign | (static_cast<uint32_t>(127 - 15 - e) << 23) | ((man & 0x3ffu) << 13);
  }
  return sign | ((exp + 112u) << 23) | (man << 13);
}

template <typename D, typename S1>
DEF_SEM(VCVTPH2PS, D dst, S1 src1) {
  auto h = UReadV16(src1);  // 4 (m64) or 8 (xmm) halves; the low 4 are converted
  uint32v4_t out = {};
  _Pragma("unroll") for (std::size_t i = 0; i < 4; ++i) {
    out.elems[i] = HalfToFloatBits(UExtractV16(h, i));
  }
  UWriteV32(dst, out);
  return memory;
}

// ---------------------------------------------------------------------------------------------
// Packed integer.

#define PS4_MINMAX32(name, V, ops)                                                  \
  template <typename D, typename S1, typename S2>                                    \
  DEF_SEM(name, D dst, S1 src1, S2 src2) {                                           \
    auto a = V##ReadV32(src1);                                                       \
    auto b = V##ReadV32(src2);                                                       \
    _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {          \
      auto x = V##ExtractV32(a, i);                                                  \
      auto y = V##ExtractV32(b, i);                                                  \
      a = V##InsertV32(a, i, ops);                                                   \
    }                                                                                \
    V##WriteV32(dst, a);                                                             \
    return memory;                                                                   \
  }

PS4_MINMAX32(PMAXSD, S, Select(SCmpGt(x, y), x, y))
PS4_MINMAX32(PMINSD, S, Select(SCmpLt(x, y), x, y))
PS4_MINMAX32(PMAXUD, U, Select(UCmpGt(x, y), x, y))
PS4_MINMAX32(PMINUD, U, Select(UCmpLt(x, y), x, y))
#undef PS4_MINMAX32

template <typename D, typename S1, typename S2>
DEF_SEM(PSUBSW, D dst, S1 src1, S2 src2) {
  auto a = SReadV16(src1);
  auto b = SReadV16(src2);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    int32_t d = static_cast<int32_t>(SExtractV16(a, i)) - static_cast<int32_t>(SExtractV16(b, i));
    d = d > 32767 ? 32767 : (d < -32768 ? -32768 : d);
    a = SInsertV16(a, i, static_cast<int16_t>(d));
  }
  SWriteV16(dst, a);
  return memory;
}

// Upstream PSUBUSB sizes its loop by the destination; with VV128W that is the whole YMM/ZMM
// register and the upper half gets garbage. Same operation, sized by the source.
template <typename D, typename S1, typename S2>
DEF_SEM(PSUBUSB_VEX, D dst, S1 src1, S2 src2) {
  auto a = UReadV8(src1);
  auto b = UReadV8(src2);
  _Pragma("unroll") for (std::size_t i = 0; i < NumVectorElems(a); ++i) {
    auto x = UExtractV8(a, i);
    auto y = UExtractV8(b, i);
    a = UInsertV8(a, i, Select(UCmpGt(x, y), USub(x, y), 0_u8));
  }
  UWriteV8(dst, a);
  return memory;
}

// ---------------------------------------------------------------------------------------------
// BMI1 lowest-set-bit group.

template <typename D, typename S>
DEF_SEM(BLSI, D dst, S src) {
  auto x = Read(src);
  auto res = UAnd(USub(decltype(x)(0), x), x);
  WriteZExt(dst, res);
  Write(FLAG_CF, UCmpNeq(x, decltype(x)(0)));
  Write(FLAG_ZF, ZeroFlag(res));
  Write(FLAG_SF, SignFlag(res));
  Write(FLAG_OF, false);
  Write(FLAG_PF, ParityFlag(res));  // undefined in the SDM; Zen computes it
  Write(FLAG_AF, false);            // undefined; 0 on Zen
  return memory;
}

template <typename D, typename S>
DEF_SEM(BLSR, D dst, S src) {
  auto x = Read(src);
  auto res = UAnd(USub(x, decltype(x)(1)), x);
  WriteZExt(dst, res);
  Write(FLAG_CF, UCmpEq(x, decltype(x)(0)));
  Write(FLAG_ZF, ZeroFlag(res));
  Write(FLAG_SF, SignFlag(res));
  Write(FLAG_OF, false);
  Write(FLAG_PF, ParityFlag(res));  // undefined in the SDM; Zen computes it
  Write(FLAG_AF, false);            // undefined; 0 on Zen
  return memory;
}

template <typename D, typename S>
DEF_SEM(BLSMSK, D dst, S src) {
  auto x = Read(src);
  auto res = UXor(USub(x, decltype(x)(1)), x);
  WriteZExt(dst, res);
  Write(FLAG_CF, UCmpEq(x, decltype(x)(0)));
  Write(FLAG_ZF, false);
  Write(FLAG_SF, SignFlag(res));
  Write(FLAG_OF, false);
  Write(FLAG_PF, ParityFlag(res));  // undefined in the SDM; Zen computes it
  Write(FLAG_AF, false);            // undefined; 0 on Zen
  return memory;
}

}  // namespace

#if HAS_FEATURE_AVX

// --- New semantics ---------------------------------------------------------------------------
DEF_ISEL(VPERMILPS_XMMdq_XMMdq_IMMb) = VPERMILPS_IMM<VV128W, V128>;
DEF_ISEL(VPERMILPS_XMMdq_MEMdq_IMMb) = VPERMILPS_IMM<VV128W, MV128>;
DEF_ISEL(VPERMILPS_YMMqq_YMMqq_IMMb) = VPERMILPS_IMM<VV256W, V256>;
DEF_ISEL(VPERMILPS_YMMqq_MEMqq_IMMb) = VPERMILPS_IMM<VV256W, MV256>;
DEF_ISEL(VPERMILPD_XMMdq_XMMdq_IMMb) = VPERMILPD_IMM<VV128W, V128>;
DEF_ISEL(VPERMILPD_XMMdq_MEMdq_IMMb) = VPERMILPD_IMM<VV128W, MV128>;
DEF_ISEL(VPERMILPD_YMMqq_YMMqq_IMMb) = VPERMILPD_IMM<VV256W, V256>;
DEF_ISEL(VPERMILPD_YMMqq_MEMqq_IMMb) = VPERMILPD_IMM<VV256W, MV256>;

DEF_ISEL(VSHUFPS_XMMdq_XMMdq_XMMdq_IMMb) = SHUFPS_LANES<VV128W, V128, V128>;
DEF_ISEL(VSHUFPS_XMMdq_XMMdq_MEMdq_IMMb) = SHUFPS_LANES<VV128W, V128, MV128>;
DEF_ISEL(VSHUFPS_YMMqq_YMMqq_YMMqq_IMMb) = SHUFPS_LANES<VV256W, V256, V256>;
DEF_ISEL(VSHUFPS_YMMqq_YMMqq_MEMqq_IMMb) = SHUFPS_LANES<VV256W, V256, MV256>;

DEF_ISEL(VBLENDPS_XMMdq_XMMdq_XMMdq_IMMb) = BLENDPS<VV128W, V128, V128>;
DEF_ISEL(VBLENDPS_XMMdq_XMMdq_MEMdq_IMMb) = BLENDPS<VV128W, V128, MV128>;
DEF_ISEL(VBLENDPS_YMMqq_YMMqq_YMMqq_IMMb) = BLENDPS<VV256W, V256, V256>;
DEF_ISEL(VBLENDPS_YMMqq_YMMqq_MEMqq_IMMb) = BLENDPS<VV256W, V256, MV256>;
DEF_ISEL(VBLENDVPS_XMMdq_XMMdq_XMMdq_XMMdq) = BLENDVPS<VV128W, V128, V128, V128>;
DEF_ISEL(VBLENDVPS_XMMdq_XMMdq_MEMdq_XMMdq) = BLENDVPS<VV128W, V128, MV128, V128>;
DEF_ISEL(VBLENDVPS_YMMqq_YMMqq_YMMqq_YMMqq) = BLENDVPS<VV256W, V256, V256, V256>;
DEF_ISEL(VBLENDVPS_YMMqq_YMMqq_MEMqq_YMMqq) = BLENDVPS<VV256W, V256, MV256, V256>;
DEF_ISEL(VBLENDVPD_XMMdq_XMMdq_XMMdq_XMMdq) = BLENDVPD<VV128W, V128, V128, V128>;
DEF_ISEL(VBLENDVPD_XMMdq_XMMdq_MEMdq_XMMdq) = BLENDVPD<VV128W, V128, MV128, V128>;
DEF_ISEL(VPBLENDVB_XMMdq_XMMdq_XMMdq_XMMdq) = PBLENDVB<VV128W, V128, V128, V128>;
DEF_ISEL(VPBLENDVB_XMMdq_XMMdq_MEMdq_XMMdq) = PBLENDVB<VV128W, V128, MV128, V128>;

DEF_ISEL(VINSERTPS_XMMdq_XMMdq_XMMdq_IMMb) = INSERTPS_REG<VV128W, V128, V128>;
DEF_ISEL(VINSERTPS_XMMdq_XMMdq_MEMd_IMMb) = INSERTPS_MEM<VV128W, V128>;
DEF_ISEL(VEXTRACTPS_MEMd_XMMdq_IMMb) = EXTRACTPS<M32W, V128>;
DEF_ISEL(VEXTRACTPS_GPR32_XMMdq_IMMb) = EXTRACTPS<R32W, V128>;
DEF_ISEL(VPINSRD_XMMdq_XMMdq_GPR32d_IMMb) = PINSRD<VV128W, V128, R32>;
DEF_ISEL(VPINSRD_XMMdq_XMMdq_MEMd_IMMb) = PINSRD<VV128W, V128, M32>;
IF_64BIT(DEF_ISEL(VPINSRQ_XMMdq_XMMdq_GPR64q_IMMb) = PINSRQ<VV128W, V128, R64>;)
IF_64BIT(DEF_ISEL(VPINSRQ_XMMdq_XMMdq_MEMq_IMMb) = PINSRQ<VV128W, V128, M64>;)
DEF_ISEL(VEXTRACTF128_XMMdq_YMMdq_IMMb) = VEXTRACTF128<VV128W>;
DEF_ISEL(VEXTRACTF128_MEMdq_YMMdq_IMMb) = VEXTRACTF128<MV128W>;
DEF_ISEL(VPALIGNR_XMMdq_XMMdq_XMMdq_IMMb) = PALIGNR_VEX<VV128W, V128, V128>;
DEF_ISEL(VPALIGNR_XMMdq_XMMdq_MEMdq_IMMb) = PALIGNR_VEX<VV128W, V128, MV128>;

DEF_ISEL(VDPPS_XMMdq_XMMdq_XMMdq_IMMb) = DPPS<VV128W, V128, V128>;
DEF_ISEL(VDPPS_XMMdq_XMMdq_MEMdq_IMMb) = DPPS<VV128W, V128, MV128>;
DEF_ISEL(VDPPS_YMMqq_YMMqq_YMMqq_IMMb) = DPPS<VV256W, V256, V256>;
DEF_ISEL(VDPPS_YMMqq_YMMqq_MEMqq_IMMb) = DPPS<VV256W, V256, MV256>;
DEF_ISEL(VHSUBPS_XMMdq_XMMdq_XMMdq) = HSUBPS<VV128W, V128, V128>;
DEF_ISEL(VHSUBPS_XMMdq_XMMdq_MEMdq) = HSUBPS<VV128W, V128, MV128>;
DEF_ISEL(VSQRTPS_XMMdq_XMMdq) = SQRTPS<VV128W, V128>;
DEF_ISEL(VSQRTPS_XMMdq_MEMdq) = SQRTPS<VV128W, MV128>;
DEF_ISEL(VSQRTPS_YMMqq_YMMqq) = SQRTPS<VV256W, V256>;
DEF_ISEL(VSQRTPS_YMMqq_MEMqq) = SQRTPS<VV256W, MV256>;
DEF_ISEL(VRCPPS_XMMdq_XMMdq) = RCPPS<VV128W, V128>;
DEF_ISEL(VRCPPS_XMMdq_MEMdq) = RCPPS<VV128W, MV128>;
DEF_ISEL(VRCPPS_YMMqq_YMMqq) = RCPPS<VV256W, V256>;
DEF_ISEL(VRSQRTPS_XMMdq_XMMdq) = RSQRTPS<VV128W, V128>;
DEF_ISEL(VRSQRTPS_XMMdq_MEMdq) = RSQRTPS<VV128W, MV128>;
DEF_ISEL(VRSQRTPS_YMMqq_YMMqq) = RSQRTPS<VV256W, V256>;
DEF_ISEL(VROUNDPS_XMMdq_XMMdq_IMMb) = ROUNDPS<VV128W, V128>;
DEF_ISEL(VROUNDPS_XMMdq_MEMdq_IMMb) = ROUNDPS<VV128W, MV128>;
DEF_ISEL(VROUNDPS_YMMqq_YMMqq_IMMb) = ROUNDPS<VV256W, V256>;
DEF_ISEL(VROUNDPS_YMMqq_MEMqq_IMMb) = ROUNDPS<VV256W, MV256>;
DEF_ISEL(VROUNDSS_XMMdq_XMMdq_XMMd_IMMb) = ROUNDSS<VV128W, V128, V128>;
DEF_ISEL(VROUNDSS_XMMdq_XMMdq_MEMd_IMMb) = ROUNDSS<VV128W, V128, MV32>;
DEF_ISEL(VROUNDSD_XMMdq_XMMdq_XMMq_IMMb) = ROUNDSD<VV128W, V128, V128>;
DEF_ISEL(VROUNDSD_XMMdq_XMMdq_MEMq_IMMb) = ROUNDSD<VV128W, V128, MV64>;

DEF_ISEL(VPMAXSD_XMMdq_XMMdq_XMMdq) = PMAXSD<VV128W, V128, V128>;
DEF_ISEL(VPMAXSD_XMMdq_XMMdq_MEMdq) = PMAXSD<VV128W, V128, MV128>;
DEF_ISEL(VPMINSD_XMMdq_XMMdq_XMMdq) = PMINSD<VV128W, V128, V128>;
DEF_ISEL(VPMINSD_XMMdq_XMMdq_MEMdq) = PMINSD<VV128W, V128, MV128>;
DEF_ISEL(VPMAXUD_XMMdq_XMMdq_XMMdq) = PMAXUD<VV128W, V128, V128>;
DEF_ISEL(VPMAXUD_XMMdq_XMMdq_MEMdq) = PMAXUD<VV128W, V128, MV128>;
DEF_ISEL(VPMINUD_XMMdq_XMMdq_XMMdq) = PMINUD<VV128W, V128, V128>;
DEF_ISEL(VPMINUD_XMMdq_XMMdq_MEMdq) = PMINUD<VV128W, V128, MV128>;
DEF_ISEL(VPSUBSW_XMMdq_XMMdq_XMMdq) = PSUBSW<VV128W, V128, V128>;
DEF_ISEL(VPSUBSW_XMMdq_XMMdq_MEMdq) = PSUBSW<VV128W, V128, MV128>;

// --- Upstream templates, VEX encodings -------------------------------------------------------
DEF_ISEL(VMINPS_XMMdq_XMMdq_XMMdq) = MINPS<VV128W, V128, V128>;
DEF_ISEL(VMINPS_XMMdq_XMMdq_MEMdq) = MINPS<VV128W, V128, MV128>;
DEF_ISEL(VMINPS_YMMqq_YMMqq_YMMqq) = MINPS<VV256W, V256, V256>;
DEF_ISEL(VMINPS_YMMqq_YMMqq_MEMqq) = MINPS<VV256W, V256, MV256>;
DEF_ISEL(VMAXPS_XMMdq_XMMdq_XMMdq) = MAXPS<VV128W, V128, V128>;
DEF_ISEL(VMAXPS_XMMdq_XMMdq_MEMdq) = MAXPS<VV128W, V128, MV128>;
DEF_ISEL(VMAXPS_YMMqq_YMMqq_YMMqq) = MAXPS<VV256W, V256, V256>;
DEF_ISEL(VMAXPS_YMMqq_YMMqq_MEMqq) = MAXPS<VV256W, V256, MV256>;
DEF_ISEL(VMOVMSKPS_GPR32d_XMMdq) = MOVMSKPS<R32W, V128>;
DEF_ISEL(VLDDQU_XMMdq_MEMdq) = MOVDQx<VV128W, MV128>;

DEF_ISEL(VPSHUFB_XMMdq_XMMdq_XMMdq) = PSHUFB<VV128W, V128, V128>;
DEF_ISEL(VPSHUFB_XMMdq_XMMdq_MEMdq) = PSHUFB<VV128W, V128, MV128>;
DEF_ISEL(VPMADDWD_XMMdq_XMMdq_XMMdq) = PMADDWD<VV128W, V128, V128>;
DEF_ISEL(VPMADDWD_XMMdq_XMMdq_MEMdq) = PMADDWD<VV128W, V128, MV128>;
DEF_ISEL(VPACKSSDW_XMMdq_XMMdq_XMMdq) = PACKSSDW<VV128W, V128, V128>;
DEF_ISEL(VPACKSSDW_XMMdq_XMMdq_MEMdq) = PACKSSDW<VV128W, V128, MV128>;
DEF_ISEL(VPMAXSW_XMMdq_XMMdq_XMMdq) = PMAXSW<VV128W, V128, V128>;
DEF_ISEL(VPMAXSW_XMMdq_XMMdq_MEMdq) = PMAXSW<VV128W, V128, MV128>;
DEF_ISEL(VPMINSW_XMMdq_XMMdq_XMMdq) = PMINSW<VV128W, V128, V128>;
DEF_ISEL(VPMINSW_XMMdq_XMMdq_MEMdq) = PMINSW<VV128W, V128, MV128>;
DEF_ISEL(VPMAXUB_XMMdq_XMMdq_XMMdq) = PMAXUB<VV128W, V128, V128>;
DEF_ISEL(VPMAXUB_XMMdq_XMMdq_MEMdq) = PMAXUB<VV128W, V128, MV128>;
DEF_ISEL(VPMULHRSW_XMMdq_XMMdq_XMMdq) = PMULHRSW<VV128W, V128, V128>;
DEF_ISEL(VPMULHRSW_XMMdq_XMMdq_MEMdq) = PMULHRSW<VV128W, V128, MV128>;
DEF_ISEL(VPMULLD_XMMdq_XMMdq_XMMdq) = PMULLD<VV128W, V128, V128>;
DEF_ISEL(VPMULLD_XMMdq_XMMdq_MEMdq) = PMULLD<VV128W, V128, MV128>;
DEF_ISEL(VPSIGNW_XMMdq_XMMdq_XMMdq) = PSIGNW<VV128W, V128, V128>;
DEF_ISEL(VPSIGNW_XMMdq_XMMdq_MEMdq) = PSIGNW<VV128W, V128, MV128>;
DEF_ISEL(VPSUBB_XMMdq_XMMdq_XMMdq) = PSUBB<VV128W, V128, V128>;
DEF_ISEL(VPSUBB_XMMdq_XMMdq_MEMdq) = PSUBB<VV128W, V128, MV128>;
DEF_ISEL(VPSUBW_XMMdq_XMMdq_XMMdq) = PSUBW<VV128W, V128, V128>;
DEF_ISEL(VPSUBW_XMMdq_XMMdq_MEMdq) = PSUBW<VV128W, V128, MV128>;
DEF_ISEL(VPSUBUSB_XMMdq_XMMdq_XMMdq) = PSUBUSB_VEX<VV128W, V128, V128>;
DEF_ISEL(VPSUBUSB_XMMdq_XMMdq_MEMdq) = PSUBUSB_VEX<VV128W, V128, MV128>;

DEF_ISEL(VPSLLD_XMMdq_XMMdq_IMMb) = PSLLD<VV128W, V128, I8>;
DEF_ISEL(VPSLLD_XMMdq_XMMdq_XMMdq) = PSLLD_V<VV128W, V128, V128>;
DEF_ISEL(VPSLLD_XMMdq_XMMdq_MEMdq) = PSLLD_V<VV128W, V128, MV128>;
DEF_ISEL(VPSLLW_XMMdq_XMMdq_IMMb) = PSLLW<VV128W, V128, I8>;
DEF_ISEL(VPSLLW_XMMdq_XMMdq_XMMdq) = PSLLW_V<VV128W, V128, V128>;
DEF_ISEL(VPSLLW_XMMdq_XMMdq_MEMdq) = PSLLW_V<VV128W, V128, MV128>;
DEF_ISEL(VPSRAD_XMMdq_XMMdq_IMMb) = PSRAD<VV128W, V128, I8>;
DEF_ISEL(VPSRAD_XMMdq_XMMdq_XMMdq) = PSRAD_V<VV128W, V128, V128>;
DEF_ISEL(VPSRAD_XMMdq_XMMdq_MEMdq) = PSRAD_V<VV128W, V128, MV128>;
DEF_ISEL(VPSRAW_XMMdq_XMMdq_IMMb) = PSRAW<VV128W, V128, I8>;
DEF_ISEL(VPSRAW_XMMdq_XMMdq_XMMdq) = PSRAW_V<VV128W, V128, V128>;
DEF_ISEL(VPSRAW_XMMdq_XMMdq_MEMdq) = PSRAW_V<VV128W, V128, MV128>;
DEF_ISEL(VPSRLD_XMMdq_XMMdq_IMMb) = PSRLD<VV128W, V128, I8>;
DEF_ISEL(VPSRLD_XMMdq_XMMdq_XMMdq) = PSRLD_V<VV128W, V128, V128>;
DEF_ISEL(VPSRLD_XMMdq_XMMdq_MEMdq) = PSRLD_V<VV128W, V128, MV128>;
DEF_ISEL(VPSRLW_XMMdq_XMMdq_IMMb) = PSRLW<VV128W, V128, I8>;
DEF_ISEL(VPSRLW_XMMdq_XMMdq_XMMdq) = PSRLW_V<VV128W, V128, V128>;
DEF_ISEL(VPSRLW_XMMdq_XMMdq_MEMdq) = PSRLW_V<VV128W, V128, MV128>;

DEF_ISEL(VPUNPCKLBW_XMMdq_XMMdq_XMMdq) = PUNPCKLBW<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKLBW_XMMdq_XMMdq_MEMdq) = PUNPCKLBW<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKHBW_XMMdq_XMMdq_XMMdq) = PUNPCKHBW<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKHBW_XMMdq_XMMdq_MEMdq) = PUNPCKHBW<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKLWD_XMMdq_XMMdq_XMMdq) = PUNPCKLWD<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKLWD_XMMdq_XMMdq_MEMdq) = PUNPCKLWD<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKHWD_XMMdq_XMMdq_XMMdq) = PUNPCKHWD<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKHWD_XMMdq_XMMdq_MEMdq) = PUNPCKHWD<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKLDQ_XMMdq_XMMdq_XMMdq) = PUNPCKLDQ<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKLDQ_XMMdq_XMMdq_MEMdq) = PUNPCKLDQ<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKHDQ_XMMdq_XMMdq_XMMdq) = PUNPCKHDQ<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKHDQ_XMMdq_XMMdq_MEMdq) = PUNPCKHDQ<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKLQDQ_XMMdq_XMMdq_MEMdq) = PUNPCKLQDQ<VV128W, V128, MV128>;
DEF_ISEL(VPUNPCKHQDQ_XMMdq_XMMdq_XMMdq) = PUNPCKHQDQ<VV128W, V128, V128>;
DEF_ISEL(VPUNPCKHQDQ_XMMdq_XMMdq_MEMdq) = PUNPCKHQDQ<VV128W, V128, MV128>;
DEF_ISEL(VSHUFPD_XMMdq_XMMdq_XMMdq_IMMb) = SHUFPD<VV128W, V128, V128>;
DEF_ISEL(VSHUFPD_XMMdq_XMMdq_MEMdq_IMMb) = SHUFPD<VV128W, V128, MV128>;

DEF_ISEL(VCVTPH2PS_XMMdq_XMMq) = VCVTPH2PS<VV128W, V128>;
DEF_ISEL(VCVTPH2PS_XMMdq_MEMq) = VCVTPH2PS<VV128W, MV64>;

#endif  // HAS_FEATURE_AVX

// --- BMI1 / MOVBE ----------------------------------------------------------------------------
DEF_ISEL(BLSI_GPR32d_GPR32d) = BLSI<R32W, R32>;
DEF_ISEL(BLSI_GPR32d_MEMd) = BLSI<R32W, M32>;
DEF_ISEL(BLSR_GPR32d_GPR32d) = BLSR<R32W, R32>;
DEF_ISEL(BLSR_GPR32d_MEMd) = BLSR<R32W, M32>;
DEF_ISEL(BLSMSK_GPR32d_GPR32d) = BLSMSK<R32W, R32>;
DEF_ISEL(BLSMSK_GPR32d_MEMd) = BLSMSK<R32W, M32>;
IF_64BIT(DEF_ISEL(BLSI_GPR64q_GPR64q) = BLSI<R64W, R64>;)
IF_64BIT(DEF_ISEL(BLSI_GPR64q_MEMq) = BLSI<R64W, M64>;)
IF_64BIT(DEF_ISEL(BLSR_GPR64q_GPR64q) = BLSR<R64W, R64>;)
IF_64BIT(DEF_ISEL(BLSR_GPR64q_MEMq) = BLSR<R64W, M64>;)
IF_64BIT(DEF_ISEL(BLSMSK_GPR64q_GPR64q) = BLSMSK<R64W, R64>;)
IF_64BIT(DEF_ISEL(BLSMSK_GPR64q_MEMq) = BLSMSK<R64W, M64>;)

DEF_ISEL(MOVBE_MEMv_GPRv_16) = MOVBE16<M16W, R16>;
DEF_ISEL(MOVBE_MEMv_GPRv_32) = MOVBE32<M32W, R32>;
IF_64BIT(DEF_ISEL(MOVBE_MEMv_GPRv_64) = MOVBE64<M64W, R64>;)

// ---------------------------------------------------------------------------------------------
// Atomic read-modify-write (LOCK prefix, XCHG with memory).
//
// Upstream lifts these as an ordinary read + write between __remill_atomic_begin/end, which is
// not atomic on a weakly ordered host. Here the memory update is a single fetch-and-op
// intrinsic (LDADDAL/LDCLRAL/LDSETAL/LDEORAL on ARMv8.1) or a CAS loop, and the flags are
// computed from the old value exactly like the plain forms. Names: see Arch.cpp
// (kAtomicLockPrefixes).

namespace {

template <typename D, typename S1, typename S2>
DEF_SEM(ADD_ATOMIC, D dst, S1, S2 src2) {
  auto rhs = Read(src2);
  auto lhs = UFetchAdd(dst, rhs);
  WriteFlagsAddSub<tag_add>(state, lhs, rhs, UAdd(lhs, rhs));
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(SUB_ATOMIC, D dst, S1, S2 src2) {
  auto rhs = Read(src2);
  auto lhs = UFetchSub(dst, rhs);
  WriteFlagsAddSub<tag_sub>(state, lhs, rhs, USub(lhs, rhs));
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(AND_ATOMIC, D dst, S1, S2 src2) {
  auto rhs = Read(src2);
  auto lhs = UFetchAnd(dst, rhs);
  SetFlagsLogical(state, lhs, rhs, UAnd(lhs, rhs));
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(OR_ATOMIC, D dst, S1, S2 src2) {
  auto rhs = Read(src2);
  auto lhs = UFetchOr(dst, rhs);
  SetFlagsLogical(state, lhs, rhs, UOr(lhs, rhs));
  UndefFlag(af);
  return memory;
}

template <typename D, typename S1, typename S2>
DEF_SEM(XOR_ATOMIC, D dst, S1, S2 src2) {
  auto rhs = Read(src2);
  auto lhs = UFetchXor(dst, rhs);
  SetFlagsLogical(state, lhs, rhs, UXor(lhs, rhs));
  UndefFlag(af);
  return memory;
}

template <typename D, typename S1>
DEF_SEM(INC_ATOMIC, D dst, S1 src) {
  decltype(Read(src)) rhs = 1;
  auto lhs = UFetchAdd(dst, rhs);
  WriteFlagsIncDec<tag_add>(state, lhs, rhs, UAdd(lhs, rhs));
  return memory;
}

template <typename D, typename S1>
DEF_SEM(DEC_ATOMIC, D dst, S1 src) {
  decltype(Read(src)) rhs = 1;
  auto lhs = UFetchSub(dst, rhs);
  WriteFlagsIncDec<tag_sub>(state, lhs, rhs, USub(lhs, rhs));
  return memory;
}

// XADD mem, reg: reg gets the old memory value.
template <typename D1, typename S1, typename D2, typename S2>
DEF_SEM(XADD_ATOMIC, D1 dst1, S1, D2 dst2, S2 src2) {
  auto rhs = Read(src2);
  auto old = UFetchAdd(dst1, rhs);
  WriteFlagsAddSub<tag_add>(state, old, rhs, UAdd(old, rhs));
  WriteZExt(dst2, old);
  return memory;
}

// XCHG mem, reg: there is no exchange intrinsic, so CAS until it sticks.
template <typename D1, typename S1, typename D2, typename S2>
DEF_SEM(XCHG_ATOMIC, D1 dst, S1 dst_val, D2 src, S2 src_val) {
  auto desired = Read(src_val);
  auto expected = Read(dst_val);
  while (!UCmpXchg(dst, expected, desired)) {
  }
  WriteZExt(src, expected);
  return memory;
}

}  // namespace

#define PS4_LOCK_ALU(OP, R80, R82)                                              \
  DEF_ISEL(OP##_LOCK_MEMb_IMMb_80##R80) = OP##_ATOMIC<M8W, M8, I8>;              \
  DEF_ISEL(OP##_LOCK_MEMb_IMMb_82##R82) = OP##_ATOMIC<M8W, M8, I8>;              \
  DEF_ISEL(OP##_LOCK_MEMb_GPR8) = OP##_ATOMIC<M8W, M8, R8>;                      \
  DEF_ISEL_MnW_Mn_In(OP##_LOCK_MEMv_IMMz, OP##_ATOMIC);                          \
  DEF_ISEL_MnW_Mn_In(OP##_LOCK_MEMv_IMMb, OP##_ATOMIC);                          \
  DEF_ISEL_MnW_Mn_Rn(OP##_LOCK_MEMv_GPRv, OP##_ATOMIC);

PS4_LOCK_ALU(ADD, r0, r0)
PS4_LOCK_ALU(OR, r1, r1)
PS4_LOCK_ALU(AND, r4, r4)
PS4_LOCK_ALU(SUB, r5, r5)
PS4_LOCK_ALU(XOR, r6, r6)
#undef PS4_LOCK_ALU

DEF_ISEL(INC_LOCK_MEMb) = INC_ATOMIC<M8W, M8>;
DEF_ISEL_MnW_Mn(INC_LOCK_MEMv, INC_ATOMIC);
DEF_ISEL(DEC_LOCK_MEMb) = DEC_ATOMIC<M8W, M8>;
DEF_ISEL_MnW_Mn(DEC_LOCK_MEMv, DEC_ATOMIC);
DEF_ISEL(XADD_LOCK_MEMb_GPR8) = XADD_ATOMIC<M8W, M8, R8W, R8>;
DEF_ISEL_MnW_Mn_RnW_Rn(XADD_LOCK_MEMv_GPRv, XADD_ATOMIC);
DEF_ISEL(XCHG_LOCK_MEMb_GPR8) = XCHG_ATOMIC<M8W, M8, R8W, R8>;
DEF_ISEL_MnW_Mn_RnW_Rn(XCHG_LOCK_MEMv_GPRv, XCHG_ATOMIC);

// ---------------------------------------------------------------------------------------------
// x87 environment (libc fenv: feraiseexcept/feclearexcept/fegetenv). 28-byte protected-mode
// layout: CW, SW, TW (each in a dword), FIP, FCS|opcode, FDP, FDS. Instruction/data pointers are
// not modelled (stored as 0); the tag word is reported as all-empty. FNSTENV then masks all x87
// exceptions, like the hardware.

namespace {

ALWAYS_INLINE static void SyncStatusWord(State &state) {
  auto &sw = state.x87.fxsave.swd;
  sw.c0 = state.sw.c0;
  sw.c1 = state.sw.c1;
  sw.c2 = state.sw.c2;
  sw.c3 = state.sw.c3;
  sw.pe = state.sw.pe;
  sw.ue = state.sw.ue;
  sw.oe = state.sw.oe;
  sw.ze = state.sw.ze;
  sw.de = state.sw.de;
  sw.ie = state.sw.ie;
}

DEF_SEM(FNSTENV, M16W dst) {
  auto &cw = state.x87.fxsave.cwd;
  cw.rc = (FPURoundingControl) __remill_fpu_get_rounding();
  SyncStatusWord(state);
  Write(dst, cw.flat);
  Write(M16W{dst.addr + 2}, static_cast<uint16_t>(0xffff));
  Write(M16W{dst.addr + 4}, state.x87.fxsave.swd.flat);
  Write(M16W{dst.addr + 6}, static_cast<uint16_t>(0xffff));
  Write(M16W{dst.addr + 8}, static_cast<uint16_t>(0xffff));
  Write(M16W{dst.addr + 10}, static_cast<uint16_t>(0xffff));
  Write(M32W{dst.addr + 12}, 0_u32);
  Write(M32W{dst.addr + 16}, 0_u32);
  Write(M32W{dst.addr + 20}, 0_u32);
  Write(M32W{dst.addr + 24}, 0_u32);
  cw.flat = static_cast<uint16_t>(cw.flat | 0x3f);
  return memory;
}

DEF_SEM(FLDENV, M16 src) {
  auto &cw = state.x87.fxsave.cwd;
  cw.flat = Read(src);
  __remill_fpu_set_rounding(cw.rc);
  auto &sw = state.x87.fxsave.swd;
  sw.flat = Read(M16{src.addr + 4});
  state.sw.c0 = sw.c0;
  state.sw.c1 = sw.c1;
  state.sw.c2 = sw.c2;
  state.sw.c3 = sw.c3;
  state.sw.pe = sw.pe;
  state.sw.ue = sw.ue;
  state.sw.oe = sw.oe;
  state.sw.ze = sw.ze;
  state.sw.de = sw.de;
  state.sw.ie = sw.ie;
  return memory;
}

}  // namespace

DEF_ISEL(FNSTENV_MEMmem28) = FNSTENV;
DEF_ISEL(FLDENV_MEMmem28) = FLDENV;
