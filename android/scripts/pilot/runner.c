// The pilot's runner: the same inputs -> every t_* -> result and time.
// Variants: x86 native (the reference for results), aarch64 native (the reference for speed),
// aarch64 lifted by Remill (-DLIFTED: functions call_sub_<address>, the guest image mapped 1:1).
#define _GNU_SOURCE
#include <elf.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef u64 (*fn2)(u64, u64);

struct saxpy { float a; u32 n; float *x; float *y; };
struct dbl { const double *v; u64 n; };

#ifdef LIFTED
#include "lifted_table.h"  // struct { const char *name; fn2 f; } tests[]
__thread jmp_buf pilot_escape;
__thread uint64_t pilot_escape_pc;
__thread int pilot_escape_kind;
__thread uint8_t *pilot_guest_stack_top;
static const char *const escape_names[] = {"", "unresolved jump", "call", "missing block", "error", "hypercall"};

static void map_guest_image(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); exit(1); }
    Elf64_Ehdr eh;
    if (pread(fd, &eh, sizeof eh, 0) != sizeof eh) exit(1);
    for (int i = 0; i < eh.e_phnum; i++) {
        Elf64_Phdr ph;
        pread(fd, &ph, sizeof ph, (off_t)(eh.e_phoff + (u64)i * eh.e_phentsize));
        if (ph.p_type != PT_LOAD) continue;
        u64 page = (u64)sysconf(_SC_PAGESIZE);
        u64 lo = ph.p_vaddr & ~(page - 1), hi = (ph.p_vaddr + ph.p_memsz + page - 1) & ~(page - 1);
        void *p = mmap((void *)lo, hi - lo, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p != (void *)lo) { perror("guest mmap"); exit(1); }
        pread(fd, (void *)ph.p_vaddr, ph.p_filesz, (off_t)ph.p_offset);
        mprotect(p, hi - lo, PROT_READ);  // guest code is not executed: data/rodata only
    }
    close(fd);
}
#else
u64 t_carry(u64, u64), t_flags(u64, u64), t_bits(u64, u64), t_saxpy(u64, u64),
    t_simd_int(u64, u64), t_switch(u64, u64), t_atomic(u64, u64), t_double(u64, u64), t_mt(u64, u64), t_mp(u64, u64);
static const struct { const char *name; fn2 f; } tests[] = {
    {"t_carry", t_carry}, {"t_flags", t_flags}, {"t_bits", t_bits}, {"t_saxpy", t_saxpy},
    {"t_simd_int", t_simd_int}, {"t_switch", t_switch}, {"t_atomic", t_atomic}, {"t_double", t_double},
    {"t_mt", t_mt}, {"t_mp", t_mp}};
#endif

enum { N = 4096 };
static u64 data[N], cell;
static float fx[N], fy[N], fy0[N];
static double dv[N];

static u64 rng = 0x243F6A8885A308D3ull;
static u64 next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// The multithreaded test t_mt: T threads on shared memory, T*N expected in every counter.
static u64 shared[64] __attribute__((aligned(64)));
struct mt_arg { fn2 f; u64 n; int failed; u64 result; };
static void *mt_thread(void *p) {
    struct mt_arg *a = p;
#ifdef LIFTED
    uint8_t *stk = malloc(1 << 16);
    pilot_guest_stack_top = stk + (1 << 16) - 64;
    if (setjmp(pilot_escape)) { a->failed = 1; return NULL; }
#endif
    a->result = a->f((u64)(uintptr_t)shared, a->n);
    return NULL;
}

static void run_mt(fn2 f, int threads, u64 n) {
    memset(shared, 0, sizeof shared);
    pthread_t t[64];
    struct mt_arg a[64];
    double t0 = now();
    for (int i = 0; i < threads; i++) {
        a[i] = (struct mt_arg){f, n, 0, 0};
        pthread_create(&t[i], NULL, mt_thread, &a[i]);
    }
    for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
    double dt = now() - t0;
    u64 want = (u64)threads * n;
    int ok = shared[0] == want && shared[16] == want && shared[24] == want && shared[32] == 2 * want;
    printf("t_mt x%d    xadd %llu  xchg-lock %llu  cmpxchg %llu  lock-add %llu  (want %llu) %s %8.1f ms\n",
           threads, (unsigned long long)shared[0], (unsigned long long)shared[16],
           (unsigned long long)shared[24], (unsigned long long)shared[32] / 2, (unsigned long long)want,
           ok ? "OK" : "MISMATCH", dt * 1e3);
    for (int i = 0; i < threads; i++) if (a[i].failed) printf("  thread %d escaped\n", i);
}

// t_mp: 2 threads (writer/reader), the sum of order violations.
static void run_mp(fn2 f, u64 n) {
    memset(shared, 0, sizeof shared);
    pthread_t t[2];
    struct mt_arg a[2];
    for (int i = 0; i < 2; i++) {
        a[i] = (struct mt_arg){f, n, 0, 0};
        pthread_create(&t[i], NULL, mt_thread, &a[i]);
    }
    for (int i = 0; i < 2; i++) pthread_join(t[i], NULL);
    printf("t_mp        reordering observed: %llu of %llu reads %s\n",
           (unsigned long long)(a[0].result + a[1].result), (unsigned long long)(4 * n),
           a[0].result + a[1].result ? "(TSO violated)" : "(ok)");
}

int main(int argc, char **argv) {
    int reps = argc > 1 ? atoi(argv[1]) : 200;
#ifdef LIFTED
    map_guest_image(argc > 2 ? argv[2] : "guest.x86.elf");
    static uint8_t stack[1 << 20] __attribute__((aligned(64)));
    pilot_guest_stack_top = stack + sizeof stack - 64;
#endif
    for (int i = 0; i < N; i++) {
        data[i] = next();
        fx[i] = (float)(next() % 1000) / 37.0f;
        fy0[i] = (float)(next() % 1000) / 91.0f;
        dv[i] = ((double)(int64_t)next()) / 9.2e15;
    }
    for (size_t t = 0; t < sizeof tests / sizeof tests[0]; t++) {
        const char *name = tests[t].name;
        if (!strcmp(name, "t_mp")) {
            run_mp(tests[t].f, argc > 4 ? strtoull(argv[4], NULL, 0) : 200000);
            continue;
        }
        if (!strcmp(name, "t_mt")) {
            int threads = argc > 3 ? atoi(argv[3]) : 4;
            run_mt(tests[t].f, threads, argc > 4 ? strtoull(argv[4], NULL, 0) : 200000);
            continue;
        }
        struct saxpy s = {1.25f, N, fx, fy};
        struct dbl d = {dv, N};
        u64 a = (u64)(uintptr_t)data, b = N;
        if (!strcmp(name, "t_saxpy")) a = (u64)(uintptr_t)&s;
        if (!strcmp(name, "t_double")) a = (u64)(uintptr_t)&d;
        if (!strcmp(name, "t_atomic")) { a = (u64)(uintptr_t)&cell; b = 1000; }
#ifdef LIFTED
        if (setjmp(pilot_escape)) {
            printf("%-11s FAIL %s at 0x%llx\n", name, escape_names[pilot_escape_kind],
                   (unsigned long long)pilot_escape_pc);
            continue;
        }
#endif
        u64 r = 0;
        double best = 1e9;
        for (int k = 0; k < reps; k++) {
            memcpy(fy, fy0, sizeof fy);
            cell = 7;
            double t0 = now();
            r = tests[t].f(a, b);
            double dt = now() - t0;
            if (dt < best) best = dt;
        }
        printf("%-11s %016llx %9.1f us\n", name, (unsigned long long)r, best * 1e6);
    }
    return 0;
}
