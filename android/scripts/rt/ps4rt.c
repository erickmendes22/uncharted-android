// ps4rt: trace dispatcher, hypercalls, calling guest functions from the host.
#define _GNU_SOURCE
#include "ps4rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- State: accessors from ps4lift --state_module (aarch64 layout by construction) ---
extern const uint64_t ps4_state_size;
#define REG(N) extern uint64_t *ps4_reg_##N(State *);
REG(RAX) REG(RBX) REG(RCX) REG(RDX) REG(RSI) REG(RDI) REG(RBP) REG(RSP) REG(R8) REG(R9)
REG(R10) REG(R11) REG(R12) REG(R13) REG(R14) REG(R15) REG(RIP) REG(FSBASE) REG(GSBASE)
#undef REG
extern uint8_t *ps4_reg_XMM0(State *), *ps4_reg_XMM1(State *), *ps4_reg_XMM2(State *), *ps4_reg_XMM3(State *);
extern uint8_t *ps4_reg_XMM4(State *), *ps4_reg_XMM5(State *), *ps4_reg_XMM6(State *), *ps4_reg_XMM7(State *);
extern uint8_t *ps4_reg_DF(State *);

// --- trace table: open addressing, the key is the guest address ---
static uint64_t *keys;
static ps4_trace_fn *vals;
static size_t cap, used;

static size_t slot(uint64_t a) { return (size_t)((a * 0x9E3779B97F4A7C15ull) >> 20) & (cap - 1); }

static void insert(uint64_t a, ps4_trace_fn f) {
    size_t i = slot(a);
    while (keys[i] && keys[i] != a) i = (i + 1) & (cap - 1);
    if (!keys[i]) used++;
    keys[i] = a;
    vals[i] = f;
}

void ps4rt_register_traces(const struct ps4_trace *t, uint64_t n) {
    if ((used + n) * 2 > cap) {
        size_t ncap = cap ? cap : 1 << 16;
        while ((used + n) * 2 > ncap) ncap <<= 1;
        uint64_t *ok = keys;
        ps4_trace_fn *ov = vals;
        size_t ocap = cap;
        keys = calloc(ncap, sizeof *keys);
        vals = calloc(ncap, sizeof *vals);
        cap = ncap;
        used = 0;
        for (size_t i = 0; i < ocap; i++) if (ok[i]) insert(ok[i], ov[i]);
        free(ok);
        free(ov);
    }
    for (uint64_t i = 0; i < n; i++) insert(t[i].addr, t[i].fn);
}

ps4_trace_fn ps4rt_lookup(uint64_t a) {
    if (!cap) return NULL;
    for (size_t i = slot(a); keys[i]; i = (i + 1) & (cap - 1))
        if (keys[i] == a) return vals[i];
    return NULL;
}

uint64_t ps4rt_trace_count(void) { return used; }

// --- exits ---
static __thread jmp_buf *escape_buf;
static __thread uint64_t escape_pc;
static __thread int escape_kind;

__attribute__((noreturn)) void ps4rt_escape(int kind, uint64_t pc) {
    escape_kind = kind;
    escape_pc = pc;
    if (!escape_buf) {
        fprintf(stderr, "ps4rt: %s at 0x%llx with no handler\n", ps4rt_escape_name(kind), (unsigned long long)pc);
        abort();
    }
    longjmp(*escape_buf, 1);
}

const char *ps4rt_escape_name(int kind) {
    static const char *const n[] = {"ok", "unresolved jump", "unresolved call", "missing block",
                                    "error", "hypercall", "async hypercall"};
    return kind >= 0 && kind < 7 ? n[kind] : "?";
}

// --- guest -> host bridge ---
enum { HOST_CAP = 4096 };
static uint64_t host_keys[HOST_CAP];
static void *host_fns[HOST_CAP];

void ps4rt_register_host(uint64_t addr, void *fn) {
    size_t i = (size_t)((addr * 0x9E3779B97F4A7C15ull) >> 52) & (HOST_CAP - 1);
    while (host_keys[i] && host_keys[i] != addr) i = (i + 1) & (HOST_CAP - 1);
    host_keys[i] = addr;
    host_fns[i] = fn;
}

static void *host_lookup(uint64_t addr) {
    size_t i = (size_t)((addr * 0x9E3779B97F4A7C15ull) >> 52) & (HOST_CAP - 1);
    for (; host_keys[i]; i = (i + 1) & (HOST_CAP - 1))
        if (host_keys[i] == addr) return host_fns[i];
    return NULL;
}

// Calls a host function with arguments from State; then acts as the guest function's `ret`: pops the
// return address (the guest came here through call [GOT] or a tail jmp [GOT]).
static Memory *call_host(State *s, void *fn, Memory *m) {
    uint64_t sp = *ps4_reg_RSP(s);
    uint64_t ia[8] = {*ps4_reg_RDI(s), *ps4_reg_RSI(s), *ps4_reg_RDX(s), *ps4_reg_RCX(s),
                      *ps4_reg_R8(s), *ps4_reg_R9(s), *(uint64_t *)(uintptr_t)(sp + 8),
                      *(uint64_t *)(uintptr_t)(sp + 16)};
    double fa[8];
    uint8_t *const xmm[8] = {ps4_reg_XMM0(s), ps4_reg_XMM1(s), ps4_reg_XMM2(s), ps4_reg_XMM3(s),
                             ps4_reg_XMM4(s), ps4_reg_XMM5(s), ps4_reg_XMM6(s), ps4_reg_XMM7(s)};
    for (int i = 0; i < 8; i++) memcpy(&fa[i], xmm[i], 8);
    uint64_t r0, r1;
    double f0, f1;
#if defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = ia[0], x1 __asm__("x1") = ia[1], x2 __asm__("x2") = ia[2],
                      x3 __asm__("x3") = ia[3], x4 __asm__("x4") = ia[4], x5 __asm__("x5") = ia[5],
                      x6 __asm__("x6") = ia[6], x7 __asm__("x7") = ia[7];
    register double d0 __asm__("d0") = fa[0], d1 __asm__("d1") = fa[1], d2 __asm__("d2") = fa[2],
                    d3 __asm__("d3") = fa[3], d4 __asm__("d4") = fa[4], d5 __asm__("d5") = fa[5],
                    d6 __asm__("d6") = fa[6], d7 __asm__("d7") = fa[7];
    __asm__ volatile("blr %[f]"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5), "+r"(x6), "+r"(x7),
                       "+w"(d0), "+w"(d1), "+w"(d2), "+w"(d3), "+w"(d4), "+w"(d5), "+w"(d6), "+w"(d7)
                     : [f] "r"(fn)
                     : "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x30",
                       "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26",
                       "v27", "v28", "v29", "v30", "v31", "memory", "cc");
    r0 = x0; r1 = x1; f0 = d0; f1 = d1;
#else
    // x86-64 host (the debug variant): SysV -> SysV with an ordinary call; RAX:RDX through a struct
    // of two u64, the floating point result is not captured.
    typedef struct { uint64_t a, b; } pair;
    pair p = ((pair(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, double, double,
                       double, double, double, double, double, double))fn)(
        ia[0], ia[1], ia[2], ia[3], ia[4], ia[5], fa[0], fa[1], fa[2], fa[3], fa[4], fa[5], fa[6], fa[7]);
    r0 = p.a; r1 = p.b; f0 = 0; f1 = 0;
#endif
    *ps4_reg_RAX(s) = r0;
    *ps4_reg_RDX(s) = r1;
    memcpy(ps4_reg_XMM0(s), &f0, 8);
    memcpy(ps4_reg_XMM1(s), &f1, 8);
    uint64_t ret = *(uint64_t *)(uintptr_t)sp;
    *ps4_reg_RSP(s) = sp + 8;
    *ps4_reg_RIP(s) = ret;
    return m;
}

// --- dispatcher ---
Memory *ps4rt_dispatch_call(State *s, uint64_t pc, Memory *m) {
    ps4_trace_fn f = ps4rt_lookup(pc);
    if (f) return f(s, pc, m);
    void *h = host_lookup(pc);
    if (h) return call_host(s, h, m);
    ps4rt_escape(PS4RT_UNRESOLVED_CALL, pc);
}

// Jumps are tail calls (musttail: the signature of a trace), so chains of jmp do not grow
// the host stack. A tail jmp into a host function (PLT) is the bridge with an emulated ret.
Memory *ps4rt_dispatch_jump(State *s, uint64_t pc, Memory *m) {
    ps4_trace_fn f = ps4rt_lookup(pc);
    if (!f) {
        void *h = host_lookup(pc);
        if (h) return call_host(s, h, m);
        ps4rt_escape(PS4RT_UNRESOLVED_JUMP, pc);
    }
    __attribute__((musttail)) return f(s, pc, m);
}

Memory *ps4rt_dispatch_missing(State *s, uint64_t pc, Memory *m) {
    ps4_trace_fn f = ps4rt_lookup(pc);
    if (!f) ps4rt_escape(PS4RT_MISSING_BLOCK, pc);
    __attribute__((musttail)) return f(s, pc, m);
}

// --- hypercalls (Remill SyncHyperCall) ---
enum { kX86CPUID = 0x102, kX86ReadTSC = 0x103, kX86ReadTSCP = 0x104 };

Memory *ps4rt_hypercall(State *s, Memory *m, uint32_t id) {
    switch (id) {
    case kX86ReadTSC:
    case kX86ReadTSCP: {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        // PS4: a 1.6 GHz TSC (Jaguar); the game cares about monotonicity and the scale from sceKernelGetTscFrequency.
        uint64_t tsc = (uint64_t)ts.tv_sec * 1600000000ull + (uint64_t)ts.tv_nsec * 8 / 5;
        *ps4_reg_RAX(s) = (uint32_t)tsc;
        *ps4_reg_RDX(s) = tsc >> 32;
        if (id == kX86ReadTSCP) *ps4_reg_RCX(s) = 0;
        return m;
    }
    case kX86CPUID: {
        // Minimal: vendor AuthenticAMD, no extended leaves. The port itself does not rely on CPUID.
        uint32_t leaf = (uint32_t)*ps4_reg_RAX(s);
        uint32_t a = 0, b = 0, c = 0, d = 0;
        if (leaf == 0) { a = 0xd; b = 0x68747541; d = 0x69746e65; c = 0x444d4163; }
        *ps4_reg_RAX(s) = a; *ps4_reg_RBX(s) = b; *ps4_reg_RCX(s) = c; *ps4_reg_RDX(s) = d;
        return m;
    }
    case 0x101:  // kAMD64EmulateInstruction: no semantics — report the instruction address
        ps4rt_escape(PS4RT_HYPERCALL, *ps4_reg_RIP(s));
    default:
        ps4rt_escape(PS4RT_HYPERCALL, id);
    }
}

// --- calling a guest function ---
enum { GUEST_STACK = 1 << 20 };
static __thread State *tls_state;
static __thread uint8_t *tls_stack;
static const uint64_t STOP_ADDR = 0x0000dead0000beefull;  // a fake return address

static struct ps4rt_result do_call(uint64_t addr, const uint64_t *iargs, int ni,
                                   const void *fargs, int nf, int fsize, uint64_t fs_base) {
    struct ps4rt_result r = {0};
    if (!tls_state) {
        tls_state = aligned_alloc(64, (ps4_state_size + 63) & ~63ull);
        tls_stack = aligned_alloc(64, GUEST_STACK);
    }
    State *s = tls_state;
    memset(s, 0, ps4_state_size);
    uint64_t *const gpr_args[6] = {ps4_reg_RDI(s), ps4_reg_RSI(s), ps4_reg_RDX(s), ps4_reg_RCX(s),
                                   ps4_reg_R8(s), ps4_reg_R9(s)};
    for (int i = 0; i < ni && i < 6; i++) *gpr_args[i] = iargs[i];
    uint8_t *const xmm[4] = {ps4_reg_XMM0(s), ps4_reg_XMM1(s), ps4_reg_XMM2(s), ps4_reg_XMM3(s)};
    for (int i = 0; i < nf && i < 4; i++) memcpy(xmm[i], (const uint8_t *)fargs + i * fsize, fsize);
    *ps4_reg_RAX(s) = (uint64_t)nf;  // SysV varargs: AL = the number of vector registers
    // Stack: SysV alignment (RSP+8 a multiple of 16 at entry), the return address is STOP_ADDR.
    uint64_t sp = ((uint64_t)(uintptr_t)(tls_stack + GUEST_STACK - 256)) & ~15ull;
    sp -= 8;
    *(uint64_t *)(uintptr_t)sp = STOP_ADDR;
    *ps4_reg_RSP(s) = sp;
    *ps4_reg_RIP(s) = addr;
    *ps4_reg_FSBASE(s) = fs_base;

    jmp_buf jb, *prev = escape_buf;
    escape_buf = &jb;
    if (setjmp(jb)) {
        r.escape = escape_kind;
        r.escape_pc = escape_pc;
    } else {
        ps4_trace_fn f = ps4rt_lookup(addr);
        if (!f) ps4rt_escape(PS4RT_UNRESOLVED_CALL, addr);
        f(s, addr, NULL);
    }
    escape_buf = prev;
    r.rax = *ps4_reg_RAX(s);
    r.rdx = *ps4_reg_RDX(s);
    memcpy(&r.xmm0, ps4_reg_XMM0(s), 8);
    memcpy(&r.xmm0f, ps4_reg_XMM0(s), 4);
    return r;
}

struct ps4rt_result ps4rt_call(uint64_t addr, const uint64_t *iargs, int ni, const double *fargs,
                               int nf, uint64_t fs_base) {
    return do_call(addr, iargs, ni, fargs, nf, 8, fs_base);
}

struct ps4rt_result ps4rt_call_f32(uint64_t addr, const float *fargs, int nf) {
    return do_call(addr, NULL, 0, fargs, nf, 4, 0);
}
