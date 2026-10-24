// The pilot's guest code: compiled twice:
//   x86-64 -march=btver2 (the PS4 CPU) -> Remill -> aarch64  and  native aarch64 (the reference for results/speed).
// Leaf functions without libc only: arguments in RDI/RSI, the result in RAX.
#include <stdint.h>
#if defined(__x86_64__)
#include <immintrin.h>
#endif

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef uint8_t u8;
#define EXPORT __attribute__((noinline, used))

// 1. A carry chain: a 128-bit sum (ADD/ADC).
EXPORT u64 t_carry(const u64 *p, u64 n) {
    unsigned __int128 s = 0;
    for (u64 i = 0; i < n; i++) s += (unsigned __int128)p[i] * (p[i] | 1);
    return (u64)s ^ (u64)(s >> 64);
}

// 2. Flags: overflow, setcc, cmov, shifts, rotations.
EXPORT u64 t_flags(const u64 *p, u64 n) {
    u64 acc = 0;
    for (u64 i = 0; i + 1 < n; i++) {
        i64 a = (i64)p[i], b = (i64)p[i + 1], r;
        acc += __builtin_add_overflow(a, b, &r);
        acc = acc * 31 + (u64)r;
        acc += (u64)__builtin_sub_overflow(a, b, &r) * 7;
        acc ^= a < b ? (u64)a : (u64)b;
        acc += (u64)a < (u64)b;
        acc += (u64)(a >> (b & 63)) + ((u64)a >> (b & 31));
        acc = __builtin_rotateleft64(acc, (unsigned)(b & 63));
    }
    return acc;
}

// 3. Bits: BMI1 (andn, blsr, tzcnt), LZCNT, POPCNT, MOVBE (load+bswap).
EXPORT u64 t_bits(const u64 *p, u64 n) {
    u64 acc = 0;
    for (u64 i = 0; i < n; i++) {
        u64 x = p[i];
        acc += (u64)__builtin_ctzll(x | 1) + (u64)__builtin_clzll(x | 1) + (u64)__builtin_popcountll(x);
        acc ^= ~x & acc;           // andn
        acc += x & (x - 1);        // blsr
        acc += __builtin_bswap64(p[n - 1 - i]);  // movbe
    }
    return acc;
}

// 4. SIMD float: saxpy on 8 floats (AVX 256 on x86) + a horizontal sum; the result is the float bits.
struct saxpy { float a; u32 n; float *x; float *y; };
EXPORT u64 t_saxpy(struct saxpy *s) {
    float a = s->a;
    for (u32 i = 0; i < s->n; i++) s->y[i] = a * s->x[i] + s->y[i];
    float sum = 0;
    for (u32 i = 0; i < s->n; i++) sum += s->y[i];
    union { float f; u32 u; } v = {sum};
    return v.u;
}

// 5. SIMD int: SSSE3 pshufb, pmaddwd, packus: what game code loves (unpacking, colour).
EXPORT u64 t_simd_int(const u8 *p, u64 n) {
    u64 acc = 0;
#if defined(__x86_64__)
    const __m128i shuf = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    __m128i sum = _mm_setzero_si128();
    for (u64 i = 0; i + 16 <= n; i += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(p + i));
        v = _mm_shuffle_epi8(v, shuf);
        __m128i lo = _mm_unpacklo_epi8(v, _mm_setzero_si128());
        __m128i hi = _mm_unpackhi_epi8(v, _mm_setzero_si128());
        sum = _mm_add_epi32(sum, _mm_madd_epi16(lo, hi));
        sum = _mm_xor_si128(sum, _mm_packus_epi16(lo, hi));
    }
    u32 t[4];
    _mm_storeu_si128((__m128i *)t, sum);
    acc = (u64)t[0] + t[1] * 3ull + t[2] * 5ull + t[3] * 7ull;
#else
    u32 sum[4] = {0};
    for (u64 i = 0; i + 16 <= n; i += 16) {
        u8 v[16];
        for (int k = 0; k < 16; k++) v[k] = p[i + (k & ~3) + 3 - (k & 3)];
        u32 m[4];
        for (int k = 0; k < 4; k++)
            m[k] = (u32)((int32_t)(int16_t)v[2 * k] * (int16_t)v[8 + 2 * k] +
                         (int32_t)(int16_t)v[2 * k + 1] * (int16_t)v[8 + 2 * k + 1]);
        u8 pk[16];
        for (int k = 0; k < 16; k++) pk[k] = v[k];
        for (int k = 0; k < 4; k++) {
            sum[k] += m[k];
            sum[k] ^= (u32)pk[4 * k] | (u32)pk[4 * k + 1] << 8 | (u32)pk[4 * k + 2] << 16 |
                      (u32)pk[4 * k + 3] << 24;
        }
    }
    acc = (u64)sum[0] + sum[1] * 3ull + sum[2] * 5ull + sum[3] * 7ull;
#endif
    return acc;
}

// 6. switch -> a jump table (an indirect jmp through .rodata).
EXPORT u64 t_switch(const u64 *p, u64 n) {
    u64 acc = 1;
    for (u64 i = 0; i < n; i++) {
        switch (p[i] & 7) {
        case 0: acc += p[i]; break;
        case 1: acc ^= p[i] << 3; break;
        case 2: acc *= 0x9E3779B97F4A7C15ull; break;
        case 3: acc -= p[i] >> 5; break;
        case 4: acc = __builtin_rotateleft64(acc, 13); break;
        case 5: acc += acc >> 7; break;
        case 6: acc ^= 0xDEADBEEFull; break;
        default: acc |= 1; break;
        }
    }
    return acc;
}

// 7. Atomics: lock xadd, lock cmpxchg.
EXPORT u64 t_atomic(u64 *cell, u64 n) {
    u64 acc = 0;
    for (u64 i = 0; i < n; i++) {
        acc += __atomic_fetch_add(cell, i, __ATOMIC_SEQ_CST);
        u64 expected = __atomic_load_n(cell, __ATOMIC_RELAXED);
        __atomic_compare_exchange_n(cell, &expected, expected ^ acc, 0, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST);
    }
    return acc ^ *cell;
}

// 8. Scalar double: division, sqrt, cvttsd2si, rounding. min/max without NaN (the x86 minsd semantics
//    with NaN differ from fmin: a separate topic).
struct dbl { const double *v; u64 n; };
EXPORT u64 t_double(const struct dbl *d) {
    double acc = 0.5;
    i64 iacc = 0;
    for (u64 i = 0; i < d->n; i++) {
        double x = d->v[i];
        acc = acc * 0.999 + x / (1.0 + (x < 0 ? -x : x));
        acc += __builtin_sqrt(x * x + 1.0);
        iacc += (i64)(x * 1000.0);
        iacc ^= (i64)__builtin_floor(acc);
    }
    union { double f; u64 u; } v = {acc};
    return v.u ^ (u64)iacc;
}

// 9. Multithreading (the runner starts it in several threads on shared memory):
//    s[0]: lock xadd; s[8]: a spinlock on xchg, s[16]: a plain counter under it (the release is
//    a plain mov: on x86 that is a release, on ARM without TSO it is not); s[24]: a lock cmpxchg loop;
//    s[32]: lock add (the result is not needed). A step of 8 x u64 = different cache lines.
EXPORT u64 t_mt(u64 *s, u64 n) {
    u64 acc = 0;
    for (u64 i = 0; i < n; i++) {
        acc += __atomic_fetch_add(&s[0], 1, __ATOMIC_SEQ_CST);
        while (__atomic_exchange_n(&s[8], 1, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__)
            __builtin_ia32_pause();
#endif
        }
        s[16] = s[16] + 1;
        __atomic_store_n(&s[8], 0, __ATOMIC_RELEASE);
        u64 old = __atomic_load_n(&s[24], __ATOMIC_RELAXED);
        while (!__atomic_compare_exchange_n(&s[24], &old, old + 1, 1, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        }
        __atomic_fetch_add(&s[32], 2, __ATOMIC_SEQ_CST);
    }
    return acc;
}

// 10. The "message passing" litmus test without lock: the writer data=i; flag=i (plain movs), the reader
//     f=flag; d=data. On x86 (TSO) always d >= f; on ARM without barriers not. The role comes from lock xadd.
EXPORT u64 t_mp(u64 *s, u64 n) {
    volatile u64 *data = &s[48], *flag = &s[56];
    u64 role = __atomic_fetch_add(&s[40], 1, __ATOMIC_SEQ_CST);
    u64 errors = 0;
    if ((role & 1) == 0) {
        for (u64 i = 1; i <= n; i++) {
            *data = i;
            *flag = i;
        }
    } else {
        for (u64 i = 0; i < n * 4; i++) {
            u64 f = *flag;
            u64 d = *data;
            errors += d < f;
        }
    }
    return errors;
}

#if defined(__x86_64__) && defined(GUEST_IMAGE)
// The image's entry point (not executed: the harness only maps the image into memory).
void _start(void) { for (;;) __builtin_ia32_pause(); }
#endif
