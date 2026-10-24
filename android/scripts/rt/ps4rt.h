// ps4rt: the runtime of lifted (Remill -> aarch64) PS4 code. The host side.
#pragma once
#include <setjmp.h>
#include <stdint.h>

typedef struct State State;
typedef struct Memory Memory;
typedef Memory *(*ps4_trace_fn)(State *, uint64_t, Memory *);
struct ps4_trace { uint64_t addr; ps4_trace_fn fn; };

// Trace tables are registered by the pieces' constructors (ps4lift -> ps4rt_register_traces).
void ps4rt_register_traces(const struct ps4_trace *table, uint64_t count);
ps4_trace_fn ps4rt_lookup(uint64_t guest_addr);
uint64_t ps4rt_trace_count(void);

// Leaving lifted code at an unexpected point.
enum ps4rt_escape_kind {
    PS4RT_OK = 0, PS4RT_UNRESOLVED_JUMP, PS4RT_UNRESOLVED_CALL, PS4RT_MISSING_BLOCK, PS4RT_ERROR,
    PS4RT_HYPERCALL, PS4RT_ASYNC_HYPERCALL,
};
struct ps4rt_result {
    int escape;          // ps4rt_escape_kind
    uint64_t escape_pc;  // the address/number it left at
    uint64_t rax, rdx;
    double xmm0;         // the low 64 bits of XMM0 as a double
    float xmm0f;
};

// Calling a guest function with the SysV ABI: up to 6 integer arguments and up to 4 doubles (XMM0..3).
// Guest stack and State per thread. fs_base is the FS value (the guest's TLS), may be 0.
struct ps4rt_result ps4rt_call(uint64_t guest_addr, const uint64_t *iargs, int niargs,
                               const double *fargs, int nfargs, uint64_t fs_base);
// The same with float arguments (the low 32 bits of XMM).
struct ps4rt_result ps4rt_call_f32(uint64_t guest_addr, const float *fargs, int nfargs);

const char *ps4rt_escape_name(int kind);

// Guest -> host bridge: the address the guest code gets in its GOT/table and calls (call/jmp)
// maps to a host function (the platform's usual convention). Arguments: RDI..R9 + 2 stack
// slots -> integer registers, XMM0..7 -> floating point; result: RAX/RDX and XMM0/XMM1. Suits
// C API functions without structures by value and without va_list.
void ps4rt_register_host(uint64_t guest_visible_addr, void *fn);
