// The Remill intrinsics runtime for the pilot (aarch64). Compiled to LLVM bitcode and linked with
// the lifted code before opt -O3, so everything gets inlined. Memory model: guest address == host address
// (on the G2 the guest ranges are reserved 1:1, see notes/findings.md).
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

// External linkage (not C99 inline, otherwise the definitions do not get into the bitcode); inlining in opt.
#define INL __attribute__((always_inline))
typedef struct Memory Memory;
typedef struct State State;

// Leaving lifted code at an unexpected point (indirect jmp, error): back to the runner.
extern __thread jmp_buf pilot_escape;
extern __thread uint64_t pilot_escape_pc;
extern __thread int pilot_escape_kind;
extern __thread uint8_t *pilot_guest_stack_top;

#ifndef PILOT_TSO
// Plain loads/stores: fast, but ARM's memory order is weaker than x86's (TSO).
#define RD(N, T) INL T __remill_read_memory_##N(Memory *m, uint64_t a) { (void)m; return *(T *)(uintptr_t)a; }
#define WR(N, T) INL Memory *__remill_write_memory_##N(Memory *m, uint64_t a, T v) { *(T *)(uintptr_t)a = v; return m; }
RD(8, uint8_t) RD(16, uint16_t) RD(32, uint32_t) RD(64, uint64_t) RD(f32, float) RD(f64, double)
WR(8, uint8_t) WR(16, uint16_t) WR(32, uint32_t) WR(64, uint64_t) WR(f32, float) WR(f64, double)
#else
// TSO emulation: every load is an acquire (LDAPR with RCpc), every store a release (STLR).
// Gives all x86 orderings except store->load (x86 does not guarantee that either without mfence/lock).
#define RD(N, T, U) INL T __remill_read_memory_##N(Memory *m, uint64_t a) { \
    (void)m; U u = __atomic_load_n((U *)(uintptr_t)a, __ATOMIC_ACQUIRE); T t; __builtin_memcpy(&t, &u, sizeof t); return t; }
#define WR(N, T, U) INL Memory *__remill_write_memory_##N(Memory *m, uint64_t a, T v) { \
    U u; __builtin_memcpy(&u, &v, sizeof u); __atomic_store_n((U *)(uintptr_t)a, u, __ATOMIC_RELEASE); return m; }
RD(8, uint8_t, uint8_t) RD(16, uint16_t, uint16_t) RD(32, uint32_t, uint32_t) RD(64, uint64_t, uint64_t)
RD(f32, float, uint32_t) RD(f64, double, uint64_t)
WR(8, uint8_t, uint8_t) WR(16, uint16_t, uint16_t) WR(32, uint32_t, uint32_t) WR(64, uint64_t, uint64_t)
WR(f32, float, uint32_t) WR(f64, double, uint64_t)
#endif

#define CMPXCHG(N, T)                                                                          \
    INL Memory *__remill_compare_exchange_memory_##N(Memory *m, uint64_t a, T *expected, T desired) { \
        __atomic_compare_exchange_n((T *)(uintptr_t)a, expected, desired, false, __ATOMIC_SEQ_CST, \
                                    __ATOMIC_SEQ_CST);                                         \
        return m;                                                                              \
    }
CMPXCHG(8, uint8_t) CMPXCHG(16, uint16_t) CMPXCHG(32, uint32_t) CMPXCHG(64, uint64_t)

#define FETCH(OP, N, T)                                                                        \
    INL Memory *__remill_fetch_and_##OP##_##N(Memory *m, uint64_t a, T *v) {                   \
        *v = __atomic_fetch_##OP((T *)(uintptr_t)a, *v, __ATOMIC_SEQ_CST);                     \
        return m;                                                                              \
    }
#define FETCH_ALL(OP) FETCH(OP, 8, uint8_t) FETCH(OP, 16, uint16_t) FETCH(OP, 32, uint32_t) FETCH(OP, 64, uint64_t)
FETCH_ALL(add) FETCH_ALL(sub) FETCH_ALL(or) FETCH_ALL(and) FETCH_ALL(xor)

// A lock prefix without its own intrinsic (xadd, lock add, ...): Remill wraps plain
// read+write in begin/end. The pilot is single-threaded -> only a barrier. Not acceptable in the game (see findings).
#ifdef PILOT_TSO
// In TSO mode no barriers are needed around lock RMW: the RMWs themselves are LDADDAL/CASAL (acquire+release,
// RCsc), and plain accesses are already LDAPR/STLR. Only store->load through mfence remains.
INL Memory *__remill_atomic_begin(Memory *m) { return m; }
INL Memory *__remill_atomic_end(Memory *m) { return m; }
#else
INL Memory *__remill_atomic_begin(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_atomic_end(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
#endif
INL Memory *__remill_barrier_load_load(Memory *m) { __atomic_thread_fence(__ATOMIC_ACQUIRE); return m; }
INL Memory *__remill_barrier_load_store(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_barrier_store_load(Memory *m) { __atomic_thread_fence(__ATOMIC_SEQ_CST); return m; }
INL Memory *__remill_barrier_store_store(Memory *m) { __atomic_thread_fence(__ATOMIC_RELEASE); return m; }

INL uint8_t __remill_undefined_8(void) { return 0; }
INL uint16_t __remill_undefined_16(void) { return 0; }
INL uint32_t __remill_undefined_32(void) { return 0; }
INL uint64_t __remill_undefined_64(void) { return 0; }
INL float __remill_undefined_f32(void) { return 0; }
INL double __remill_undefined_f64(void) { return 0; }

// Markers for analysis: they just pass the value through.
INL bool __remill_flag_computation_zero(bool r, ...) { return r; }
INL bool __remill_flag_computation_sign(bool r, ...) { return r; }
INL bool __remill_flag_computation_overflow(bool r, ...) { return r; }
INL bool __remill_flag_computation_carry(bool r, ...) { return r; }
#define CMP(N) INL bool __remill_compare_##N(bool r) { return r; }
CMP(eq) CMP(neq) CMP(ult) CMP(ule) CMP(ugt) CMP(uge) CMP(slt) CMP(sle) CMP(sgt) CMP(sge)

INL uint64_t __remill_symbolic_STACK(void) { return (uint64_t)(uintptr_t)pilot_guest_stack_top; }
INL uint64_t __remill_symbolic_FSBASE(void) { return 0; }
INL uint64_t __remill_symbolic_GSBASE(void) { return 0; }

// A normal exit from a leaf function: ret -> the wrapper reads RAX.
INL Memory *__remill_function_return(State *s, uint64_t pc, Memory *m) { (void)s; (void)pc; return m; }

// Exits are inlined and do not get State: otherwise State "escapes" and opt cannot drop
// a single flag store (measured: t_bits 41 -> ? us on the X4).
__attribute__((noinline, noreturn)) static void escape(int kind, uint64_t pc) {
    pilot_escape_pc = pc;
    pilot_escape_kind = kind;
    longjmp(pilot_escape, 1);
}
INL Memory *__remill_jump(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; escape(1, pc); }
INL Memory *__remill_function_call(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; escape(2, pc); }
INL Memory *__remill_missing_block(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; escape(3, pc); }
INL Memory *__remill_error(State *s, uint64_t pc, Memory *m) { (void)s; (void)m; escape(4, pc); }
INL Memory *__remill_sync_hyper_call(State *s, Memory *m, uint32_t call) { (void)s; (void)m; escape(5, call); }
