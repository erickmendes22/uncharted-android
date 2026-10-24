// ps4rt: Remill intrinsics for aarch64. Compiled to bitcode and linked with every piece
// of lifted code before opt (everything gets inlined). Memory: guest address == host address (1:1).
// TSO emulation by default (LDAPR/STLR): without it lifted multithreaded x86 code breaks
// (notes/findings.md, 2026-10-18, t_mt on the G2). -DPS4RT_NO_TSO: plain ld/st (only for measurements).
#include <stdbool.h>
#include <stdint.h>

#define INL __attribute__((always_inline))
typedef struct Memory Memory;
typedef struct State State;

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;

#ifdef PS4RT_NO_TSO
#define RD(N, T, U) INL T __remill_read_memory_##N(Memory *m, uint64_t a) { (void)m; return *(T *)(uintptr_t)a; }
#define WR(N, T, U) INL Memory *__remill_write_memory_##N(Memory *m, uint64_t a, T v) { *(T *)(uintptr_t)a = v; return m; }
#else
// LDAPR/STLR need alignment (with LSE2: within 16 bytes), while x86 code reads unaligned
// all the time (memcpy, string parsing): libc_diff on qemu got SIGBUS and wrong values. An unaligned
// access is a plain ld/st with a barrier: dmb ishld (acquire) after a load, dmb ish before a store.
// The slow path is inline: moving it into a cold function came out bigger in code (eboot: 4126 ->
// 4611 KiB for 1500 functions) because of the call setup and saving registers.
#define ALIGNED(a, U) __builtin_expect(((a) & (sizeof(U) - 1)) == 0, 1)
#define RD(N, T, U) INL T __remill_read_memory_##N(Memory *m, uint64_t a) { \
    (void)m; U u; \
    if (sizeof(U) == 1 || ALIGNED(a, U)) u = __atomic_load_n((U *)(uintptr_t)a, __ATOMIC_ACQUIRE); \
    else { __builtin_memcpy(&u, (void *)(uintptr_t)a, sizeof u); __atomic_thread_fence(__ATOMIC_ACQUIRE); } \
    T t; __builtin_memcpy(&t, &u, sizeof t); return t; }
#define WR(N, T, U) INL Memory *__remill_write_memory_##N(Memory *m, uint64_t a, T v) { \
    U u; __builtin_memcpy(&u, &v, sizeof u); \
    if (sizeof(U) == 1 || ALIGNED(a, U)) __atomic_store_n((U *)(uintptr_t)a, u, __ATOMIC_RELEASE); \
    else { __atomic_thread_fence(__ATOMIC_RELEASE); __builtin_memcpy((void *)(uintptr_t)a, &u, sizeof u); } \
    return m; }
#endif

// Thread-local accesses (stack: address = RSP + constant; ps4lift -> __ps4_*_stack_*):
// no TSO ordering and no alignment check.
#define SRD(N, T) INL T __ps4_read_stack_##N(Memory *m, uint64_t a) { (void)m; T t; __builtin_memcpy(&t, (void *)(uintptr_t)a, sizeof t); return t; }
#define SWR(N, T) INL Memory *__ps4_write_stack_##N(Memory *m, uint64_t a, T v) { __builtin_memcpy((void *)(uintptr_t)a, &v, sizeof v); return m; }
SRD(8, uint8_t) SRD(16, uint16_t) SRD(32, uint32_t) SRD(64, uint64_t) SRD(f32, float) SRD(f64, double)
SWR(8, uint8_t) SWR(16, uint16_t) SWR(32, uint32_t) SWR(64, uint64_t) SWR(f32, float) SWR(f64, double)
RD(8, uint8_t, u8) RD(16, uint16_t, u16) RD(32, uint32_t, u32) RD(64, uint64_t, u64)
RD(f32, float, u32) RD(f64, double, u64)
WR(8, uint8_t, u8) WR(16, uint16_t, u16) WR(32, uint32_t, u32) WR(64, uint64_t, u64)
WR(f32, float, u32) WR(f64, double, u64)

// 80-bit x87 in memory: raw 10 bytes for now (x87 arithmetic is not ported yet, see findings).
typedef struct { uint8_t b[10]; } f80_bytes;
INL Memory *__remill_read_memory_f80(Memory *m, uint64_t a, f80_bytes *out) {
    __builtin_memcpy(out, (void *)(uintptr_t)a, 10); return m;
}
INL Memory *__remill_write_memory_f80(Memory *m, uint64_t a, const f80_bytes *in) {
    __builtin_memcpy((void *)(uintptr_t)a, in, 10); return m;
}
INL Memory *__remill_read_memory_f128(Memory *m, uint64_t a, double *out) { *out = *(double *)(uintptr_t)a; return m; }
INL Memory *__remill_write_memory_f128(Memory *m, uint64_t a, double in) { *(double *)(uintptr_t)a = in; return m; }

#define CMPXCHG(N, T)                                                                          \
    INL Memory *__remill_compare_exchange_memory_##N(Memory *m, uint64_t a, T *expected, T desired) { \
        __atomic_compare_exchange_n((T *)(uintptr_t)a, expected, desired, false, __ATOMIC_SEQ_CST, \
                                    __ATOMIC_SEQ_CST);                                         \
        return m;                                                                              \
    }
CMPXCHG(8, uint8_t) CMPXCHG(16, uint16_t) CMPXCHG(32, uint32_t) CMPXCHG(64, uint64_t)
CMPXCHG(128, unsigned __int128)  // CASPAL with LSE

#define FETCH(OP, N, T)                                                                        \
    INL Memory *__remill_fetch_and_##OP##_##N(Memory *m, uint64_t a, T *v) {                   \
        *v = __atomic_fetch_##OP((T *)(uintptr_t)a, *v, __ATOMIC_SEQ_CST);                     \
        return m;                                                                              \
    }
#define FETCH_ALL(OP) FETCH(OP, 8, uint8_t) FETCH(OP, 16, uint16_t) FETCH(OP, 32, uint32_t) FETCH(OP, 64, uint64_t)
FETCH_ALL(add) FETCH_ALL(sub) FETCH_ALL(or) FETCH_ALL(and) FETCH_ALL(xor) FETCH_ALL(nand)

#ifdef PS4RT_NO_TSO
INL Memory *__remill_atomic_begin(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_atomic_end(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
#else
// RMW: LDADDAL/CASAL (RCsc), plain accesses: LDAPR/STLR: no barriers needed around them.
INL Memory *__remill_atomic_begin(Memory *m) { return m; }
INL Memory *__remill_atomic_end(Memory *m) { return m; }
#endif
INL Memory *__remill_barrier_load_load(Memory *m) { __atomic_thread_fence(__ATOMIC_ACQUIRE); return m; }
INL Memory *__remill_barrier_load_store(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_barrier_store_load(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_barrier_store_store(Memory *m) { __atomic_thread_fence(__ATOMIC_RELEASE); return m; }
INL Memory *__remill_delay_slot_begin(Memory *m) { return m; }
INL Memory *__remill_delay_slot_end(Memory *m) { return m; }

INL uint8_t __remill_undefined_8(void) { return 0; }
INL uint16_t __remill_undefined_16(void) { return 0; }
INL uint32_t __remill_undefined_32(void) { return 0; }
INL uint64_t __remill_undefined_64(void) { return 0; }
INL float __remill_undefined_f32(void) { return 0; }
INL double __remill_undefined_f64(void) { return 0; }

INL bool __remill_flag_computation_zero(bool r, ...) { return r; }
INL bool __remill_flag_computation_sign(bool r, ...) { return r; }
INL bool __remill_flag_computation_overflow(bool r, ...) { return r; }
INL bool __remill_flag_computation_carry(bool r, ...) { return r; }
#define CMP(N) INL bool __remill_compare_##N(bool r) { return r; }
CMP(eq) CMP(neq) CMP(ult) CMP(ule) CMP(ugt) CMP(uge) CMP(slt) CMP(sle) CMP(sgt) CMP(sge)

// x87: ps4lift turns x86_fp80 into fp128 (LowerX87ToF128). Conversions through the 80-bit format:
// exact unpacking and packing with the fraction rounded 112 -> 63 bits to nearest even.
#if defined(__x86_64__)
typedef __float128 f128;
#else
typedef long double f128;  // aarch64 Linux/Android: IEEE quad
#endif
typedef union { f128 f; struct { uint64_t lo, hi; } u; } q128;

INL f128 __ps4_f80_to_f128(uint64_t m, uint16_t se) {
    q128 q;
    uint64_t sign = (uint64_t)(se >> 15) << 63;
    uint64_t e = se & 0x7fff;
    uint64_t frac = m & 0x7fffffffffffffffull;
    if (e == 0 && (m >> 63)) e = 1;  // pseudo-denormal
    q.u.hi = sign | (e << 48) | (frac >> 15);
    q.u.lo = frac << 49;
    return q.f;
}

static inline __attribute__((always_inline)) uint64_t f128_to_f80(f128 x, uint16_t *se) {
    q128 q;
    q.f = x;
    uint64_t sign = q.u.hi >> 63, e = (q.u.hi >> 48) & 0x7fff;
    uint64_t fhi = q.u.hi & 0xffffffffffffull;
    uint64_t frac = (fhi << 15) | (q.u.lo >> 49), rem = q.u.lo & ((1ull << 49) - 1), m;
    if (e == 0x7fff) {
        m = (1ull << 63) | frac;
        if ((fhi | q.u.lo) && !frac) m |= 1ull << 62;  // a NaN whose payload did not fit
    } else {
        m = (e ? (1ull << 63) : 0) | frac;
        const uint64_t half = 1ull << 48;
        if (rem > half || (rem == half && (m & 1))) {
            m++;
            if (m == 0) { m = 1ull << 63; e++; }
            else if (!e && (m >> 63)) e = 1;
        }
        if (e >= 0x7fff) { e = 0x7fff; m = 1ull << 63; }  // overflow -> inf
    }
    *se = (uint16_t)((sign << 15) | e);
    return m;
}
INL uint64_t __ps4_f128_to_f80_lo(f128 x) { uint16_t se; return f128_to_f80(x, &se); }
INL uint16_t __ps4_f128_to_f80_hi(f128 x) { uint16_t se; f128_to_f80(x, &se); return se; }

// FPU exceptions (MXCSR): not modelled, the exception flags stay zero.
INL int __remill_fpu_exception_test(int mask) { (void)mask; return 0; }
INL int __remill_fpu_exception_test_and_clear(int read_mask, int clear_mask) { (void)read_mask; (void)clear_mask; return 0; }
INL int __remill_fpu_exception_clear(int mask) { (void)mask; return 0; }

// Rounding: the semantics pass x86 <fenv.h> constants (0 / 0x400 down / 0x800 up /
// 0xc00 toward zero). The host FPU gets the same mode: the lifted arithmetic runs on it.
int fesetround(int);
int fegetround(void);
INL int __remill_fpu_get_rounding(void) {
    switch (fegetround()) {
    case 0x800000: return 0x400;
    case 0x400000: return 0x800;
    case 0xc00000: return 0xc00;
    default: return 0;
    }
}
INL int __remill_fpu_set_rounding(int mode) {
    switch (mode & 0xc00) {
    case 0x400: return fesetround(0x800000);
    case 0x800: return fesetround(0x400000);
    case 0xc00: return fesetround(0xc00000);
    default: return fesetround(0);
    }
}

__attribute__((noreturn)) void ps4rt_escape(int kind, uint64_t pc);
#define IO(N, T) \
    INL T __remill_read_io_port_##N(Memory *m, uint64_t port) { (void)m; ps4rt_escape(5, port); } \
    INL Memory *__remill_write_io_port_##N(Memory *m, uint64_t port, T v) { (void)m; (void)v; ps4rt_escape(5, port); }
IO(8, uint8_t) IO(16, uint16_t) IO(32, uint32_t)

INL Memory *__remill_function_return(State *s, uint64_t pc, Memory *m) { (void)s; (void)pc; return m; }
// The dispatcher (ps4rt.c). Not inlined, but it gets State: that is fine, State lives in memory
// at trace boundaries anyway.
Memory *ps4rt_dispatch_call(State *s, uint64_t pc, Memory *m);
Memory *ps4rt_dispatch_jump(State *s, uint64_t pc, Memory *m);
Memory *ps4rt_dispatch_missing(State *s, uint64_t pc, Memory *m);
Memory *ps4rt_hypercall(State *s, Memory *m, uint32_t id);
__attribute__((noreturn)) void ps4rt_escape(int kind, uint64_t pc);
INL Memory *__remill_function_call(State *s, uint64_t pc, Memory *m) { return ps4rt_dispatch_call(s, pc, m); }
INL Memory *__remill_jump(State *s, uint64_t pc, Memory *m) { return ps4rt_dispatch_jump(s, pc, m); }
INL Memory *__remill_missing_block(State *s, uint64_t pc, Memory *m) { return ps4rt_dispatch_missing(s, pc, m); }
INL Memory *__remill_error(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; ps4rt_escape(4, pc); }
INL Memory *__remill_sync_hyper_call(State *s, Memory *m, uint32_t id) { return ps4rt_hypercall(s, m, id); }
INL Memory *__remill_async_hyper_call(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; ps4rt_escape(6, pc); }
