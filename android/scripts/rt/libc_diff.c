// Differential test of real PS4 code: the game's libc.prx functions.
//   x86-64 (WSL, native): a call by address in the mapped image.
//   aarch64 (-DLIFTED):    a call into lifted code through ps4rt_call (G2 or qemu).
// The libc.prx image is mapped at BASE with the same RELATIVE relocations; the output compares
// line by line (pointers as offsets from the input buffer, floats as %a). Every test runs in a fork().
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libc_funcs.h"
#ifdef LIFTED
#include "ps4rt.h"
#endif

#define BASE 0x900000000ull
typedef uint64_t u64;

static u64 fn(const char *name) {
    for (size_t i = 0; i < sizeof libc_funcs / sizeof libc_funcs[0]; i++)
        if (!strcmp(libc_funcs[i].name, name)) return BASE + libc_funcs[i].vaddr;
    return 0;
}

// --- imports (libkernel etc.) implemented by the harness ---
#include "libc_known_nids.h"
static __thread int h_errno_val;
static uint64_t h_stack_guard = 0x5ca1ab1e0ddba11ull;
static int *h_error(void) { return &h_errno_val; }
static void h_stack_chk_fail(void) { printf("STACK SMASHED\n"); exit(5); }
static int h_mutex_nop(void *m) { (void)m; return 0; }
static uint64_t h_tsc_freq(void) { return 1600000000ull; }
static void *h_malloc(uint64_t n) { return malloc(n); }
static void h_free(void *p) { free(p); }
static const struct { const char *nid; void *fn; int data; } known_imports[] = {
    {NID_error, (void *)h_error, 0},
    {NID_stack_chk_fail, (void *)h_stack_chk_fail, 0},
    {NID_stack_chk_guard, &h_stack_guard, 1},
    {NID_pthread_mutex_lock, (void *)h_mutex_nop, 0},
    {NID_pthread_mutex_unlock, (void *)h_mutex_nop, 0},
    {NID_scePthreadMutexLock, (void *)h_mutex_nop, 0},
    {NID_scePthreadMutexUnlock, (void *)h_mutex_nop, 0},
    {NID_sceKernelGetTscFrequency, (void *)h_tsc_freq, 0},
    {NID_malloc, (void *)h_malloc, 0},
    {NID_free, (void *)h_free, 0},
};
static char unknown_nids[256][16];
static int n_unknown;
#ifndef LIFTED
static void unknown_import(void) { printf("UNKNOWN IMPORT called\n"); exit(4); }
#endif
// An unknown import in lifted code: a fake address; leaving through it prints the NID.
#define FAKE_IMPORT_BASE 0x7f0000000000ull

static void bind_imports(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char nid_[64];
    unsigned long long off, defined;
    long long addend;
    int kind, bound = 0, own = 0;
    while (fscanf(f, "%llx %d %63s %llx %lld", &off, &kind, nid_, &defined, &addend) == 5) {
        u64 *slot = (u64 *)(uintptr_t)(BASE + off);
        if (defined) { *slot = BASE + defined + (u64)addend; own++; continue; }
        void *fn = NULL;
        for (size_t i = 0; i < sizeof known_imports / sizeof known_imports[0]; i++)
            if (!strcmp(known_imports[i].nid, nid_)) {
                fn = known_imports[i].fn;
#ifdef LIFTED
                if (!known_imports[i].data) ps4rt_register_host((u64)(uintptr_t)fn, fn);
#endif
            }
        if (fn) { *slot = (u64)(uintptr_t)fn + (u64)addend; bound++; continue; }
        int idx = -1;
        for (int i = 0; i < n_unknown; i++) if (!strcmp(unknown_nids[i], nid_)) idx = i;
        if (idx < 0 && n_unknown < 256) { idx = n_unknown++; snprintf(unknown_nids[idx], 16, "%s", nid_); }
#ifdef LIFTED
        *slot = FAKE_IMPORT_BASE + (u64)idx * 16;
#else
        *slot = (u64)(uintptr_t)unknown_import;
#endif
    }
    fclose(f);
    fprintf(stderr, "imports: %d own-symbol relocs, %d bound, %d unknown NIDs\n", own, bound, n_unknown);
}

static void map_image(const char *image, const char *relocs) {
    int fd = open(image, O_RDONLY);
    if (fd < 0) { perror(image); exit(1); }
    off_t size = lseek(fd, 0, SEEK_END);
    void *p = mmap((void *)BASE, (size_t)size + 0x10000, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != (void *)BASE) { perror("mmap image"); exit(1); }
    if (pread(fd, p, (size_t)size, 0) != size) { perror("read image"); exit(1); }
    close(fd);
    FILE *r = fopen(relocs, "rb");
    if (!r) { perror(relocs); exit(1); }
    u64 rel[2];
    while (fread(rel, 8, 2, r) == 2) *(u64 *)(uintptr_t)(BASE + rel[0]) = BASE + rel[1];
    fclose(r);
}

// --- calls: native or through ps4rt ---
static char tls_block[0x4000] __attribute__((aligned(64)));
#ifdef LIFTED
static void report_escape(struct ps4rt_result r) {
    if (r.escape_pc >= FAKE_IMPORT_BASE && r.escape_pc < FAKE_IMPORT_BASE + 256 * 16)
        printf("ESCAPE import %s\n", unknown_nids[(r.escape_pc - FAKE_IMPORT_BASE) / 16]);
    else
        printf("ESCAPE %s at 0x%llx\n", ps4rt_escape_name(r.escape), (unsigned long long)r.escape_pc);
    exit(3);
}
static u64 call_i(u64 f, int n, u64 a, u64 b, u64 c, u64 d) {
    u64 args[4] = {a, b, c, d};
    struct ps4rt_result r = ps4rt_call(f, args, n, NULL, 0, (u64)(uintptr_t)(tls_block + 0x2000));
    if (r.escape) report_escape(r);
    return r.rax;
}
static double call_d(u64 f, int n, double a, double b) {
    double args[2] = {a, b};
    struct ps4rt_result r = ps4rt_call(f, NULL, 0, args, n, (u64)(uintptr_t)(tls_block + 0x2000));
    if (r.escape) report_escape(r);
    return r.xmm0;
}
static float call_f(u64 f, int n, float a, float b) {
    float args[2] = {a, b};
    struct ps4rt_result r = ps4rt_call_f32(f, args, n);
    if (r.escape) report_escape(r);
    return r.xmm0f;
}
#else
static u64 call_i(u64 f, int n, u64 a, u64 b, u64 c, u64 d) {
    (void)n;
    return ((u64(*)(u64, u64, u64, u64))(uintptr_t)f)(a, b, c, d);
}
static double call_d(u64 f, int n, double a, double b) {
    (void)n;
    return ((double (*)(double, double))(uintptr_t)f)(a, b);
}
static float call_f(u64 f, int n, float a, float b) {
    (void)n;
    return ((float (*)(float, float))(uintptr_t)f)(a, b);
}
#endif

// --- data ---
static char buf[4096] __attribute__((aligned(64)));
static char buf2[4096] __attribute__((aligned(64)));
static const char *const strs[] = {"", "a", "hello", "hello world", "Uncharted: Drake's Fortune",
                                   "0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEF",
                                   "  -12345xyz", "0x7fff", "3.14159e2", "\xff\x80\x01"};
#define NSTR (sizeof strs / sizeof strs[0])
static char *S(int i) { strcpy(buf + 64 * i, strs[i]); return buf + 64 * i; }
static long off(u64 p) { return p ? (long)(p - (u64)(uintptr_t)buf) : -1; }
static void dump(const char *p, int n) { for (int i = 0; i < n; i++) printf("%02x", (unsigned char)p[i]); }

static void run(const char *name) {
    u64 f = fn(name);
    if (!f) { printf("%s: not exported\n", name); return; }
    if (!strcmp(name, "strlen")) for (unsigned i = 0; i < NSTR; i++) printf("strlen(%u)=%llu\n", i, (unsigned long long)call_i(f, 1, (u64)(uintptr_t)S(i), 0, 0, 0));
    else if (!strcmp(name, "strnlen")) for (unsigned i = 0; i < NSTR; i++) printf("strnlen(%u,5)=%llu\n", i, (unsigned long long)call_i(f, 2, (u64)(uintptr_t)S(i), 5, 0, 0));
    else if (!strcmp(name, "strcmp") || !strcmp(name, "strncmp") || !strcmp(name, "memcmp")) {
        for (unsigned i = 0; i < NSTR; i++)
            for (unsigned j = 0; j < NSTR; j++) {
                int r = (int)call_i(f, 3, (u64)(uintptr_t)S(i), (u64)(uintptr_t)S(j), 4, 0);
                printf("%s(%u,%u)=%d\n", name, i, j, r < 0 ? -1 : r > 0);
            }
    } else if (!strcmp(name, "memcpy") || !strcmp(name, "memmove")) {
        for (unsigned n = 0; n < 300; n += 37) {
            for (int k = 0; k < 512; k++) buf[k] = (char)(k * 7 + 1);
            memset(buf2, 0, 512);
            u64 r = call_i(f, 3, (u64)(uintptr_t)(buf2 + 3), (u64)(uintptr_t)(buf + 5), n, 0);
            printf("%s(n=%u)=%ld ", name, n, (long)(r - (u64)(uintptr_t)buf2)); dump(buf2, 320); printf("\n");
        }
        if (!strcmp(name, "memmove")) {
            for (int k = 0; k < 512; k++) buf[k] = (char)k;
            call_i(f, 3, (u64)(uintptr_t)(buf + 10), (u64)(uintptr_t)buf, 200, 0);
            printf("memmove(overlap) "); dump(buf, 220); printf("\n");
        }
    } else if (!strcmp(name, "memset")) {
        for (unsigned n = 0; n < 300; n += 41) {
            memset(buf, 0x11, 512);
            call_i(f, 3, (u64)(uintptr_t)(buf + 1), 0xab, n, 0);
            printf("memset(n=%u) ", n); dump(buf, 310); printf("\n");
        }
    } else if (!strcmp(name, "strchr") || !strcmp(name, "strrchr")) {
        for (unsigned i = 0; i < NSTR; i++) printf("%s(%u)=%ld %ld\n", name, i, off(call_i(f, 2, (u64)(uintptr_t)S(i), 'l', 0, 0)), off(call_i(f, 2, (u64)(uintptr_t)S(i), 0, 0, 0)));
    } else if (!strcmp(name, "strstr")) {
        for (unsigned i = 0; i < NSTR; i++) for (unsigned j = 0; j < NSTR; j++) printf("strstr(%u,%u)=%ld\n", i, j, off(call_i(f, 2, (u64)(uintptr_t)S(i), (u64)(uintptr_t)S(j), 0, 0)));
    } else if (!strcmp(name, "strcpy") || !strcmp(name, "strcat") || !strcmp(name, "strncpy")) {
        for (unsigned i = 0; i < NSTR; i++) {
            memset(buf2, 'Z', 128); strcpy(buf2, "pre:");
            call_i(f, 3, (u64)(uintptr_t)buf2, (u64)(uintptr_t)S(i), 6, 0);
            printf("%s(%u) ", name, i); dump(buf2, 80); printf("\n");
        }
    } else if (!strcmp(name, "abs") || !strcmp(name, "labs") || !strcmp(name, "llabs") || !strcmp(name, "toupper") || !strcmp(name, "tolower") || !strcmp(name, "isdigit")) {
        const long long v[] = {0, 1, -1, 'a', 'Z', '5', 0x7fffffff, -0x7fffffffll - 1, 0x123456789ll, -98765};
        for (unsigned i = 0; i < 10; i++) printf("%s(%lld)=%lld\n", name, v[i], (long long)call_i(f, 1, (u64)v[i], 0, 0, 0));
    } else if (!strcmp(name, "atoi")) {
        for (unsigned i = 0; i < NSTR; i++) printf("atoi(%u)=%d\n", i, (int)call_i(f, 1, (u64)(uintptr_t)S(i), 0, 0, 0));
    } else if (!strcmp(name, "strtol") || !strcmp(name, "strtoul")) {
        for (unsigned i = 0; i < NSTR; i++) for (int base = 0; base <= 16; base += 8) {
            u64 end = 0;
            long long r = (long long)call_i(f, 3, (u64)(uintptr_t)S(i), (u64)(uintptr_t)&end, (u64)base, 0);
            printf("%s(%u,%d)=%lld end=%ld\n", name, i, base, r, off(end));
        }
    } else if (!strcmp(name, "strtod") || !strcmp(name, "strtof")) {
        for (unsigned i = 0; i < NSTR; i++) {
            u64 end = 0;
            u64 args_end = (u64)(uintptr_t)&end;
#ifdef LIFTED
            u64 ia[2] = {(u64)(uintptr_t)S(i), args_end};
            struct ps4rt_result r = ps4rt_call(f, ia, 2, NULL, 0, (u64)(uintptr_t)(tls_block + 0x2000));
            if (r.escape) report_escape(r);
            if (!strcmp(name, "strtod")) printf("strtod(%u)=%a end=%ld\n", i, r.xmm0, off(end));
            else printf("strtof(%u)=%a end=%ld\n", i, r.xmm0f, off(end));
#else
            if (!strcmp(name, "strtod")) printf("strtod(%u)=%a end=%ld\n", i, ((double (*)(const char *, u64))(uintptr_t)f)(S(i), args_end), off(end));
            else printf("strtof(%u)=%a end=%ld\n", i, ((float (*)(const char *, u64))(uintptr_t)f)(S(i), args_end), off(end));
#endif
        }
    } else if (!strcmp(name, "snprintf") || !strcmp(name, "sprintf")) {
        memset(buf2, 0, 256);
        strcpy(buf + 1024, "[%08d|%%|x]");  // exactly one argument: 1234
        long r = (long)call_i(f, 4, (u64)(uintptr_t)buf2, 200, (u64)(uintptr_t)(buf + 1024), 1234);
        printf("%s=%ld ", name, r); dump(buf2, 40); printf("\n");
    } else if (!strcmp(name, "scalbn") || !strcmp(name, "ldexp") || !strcmp(name, "frexp") || !strcmp(name, "modf") || !strcmp(name, "lrint")) {
        // Mixed arguments: double + int (or a pointer).
        const double v[] = {0.0, -0.0, 1.0, -1.5, 3.141592653589793, 1e-300, 1e300, 0x1.7e43c8800759cp+996, 5e-324, 2.5};
        const int e[] = {0, 1, -1, 54, -54, 1000, -1074, 2000, -2000, 960};
        for (int i = 0; i < 10; i++)
            for (int j = 0; j < 10; j += 3) {
                double r;
                u64 aux = 0;
                u64 ia[1] = {!strcmp(name, "scalbn") || !strcmp(name, "ldexp") ? (u64)(int64_t)e[j] : (u64)(uintptr_t)&aux};
                int ni = strcmp(name, "lrint") ? 1 : 0;
#ifdef LIFTED
                struct ps4rt_result rr = ps4rt_call(f, ia, ni, &v[i], 1, (u64)(uintptr_t)(tls_block + 0x2000));
                if (rr.escape) report_escape(rr);
                r = !strcmp(name, "lrint") ? (double)(int64_t)rr.rax : !strcmp(name, "frexp") || !strcmp(name, "modf") ? rr.xmm0 : rr.xmm0;
#else
                if (!strcmp(name, "lrint")) r = (double)((long (*)(double))(uintptr_t)f)(v[i]);
                else if (!strcmp(name, "frexp") || !strcmp(name, "modf")) r = ((double (*)(double, u64))(uintptr_t)f)(v[i], ia[0]);
                else r = ((double (*)(double, int))(uintptr_t)f)(v[i], e[j]);
#endif
                if (strcmp(name, "scalbn") && strcmp(name, "ldexp") && j) continue;
                printf("%s(%a,%d)=%a aux=%llx\n", name, v[i], e[j], r, (unsigned long long)aux);
            }
    } else if (name[strlen(name) - 1] == 'f' && strcmp(name, "fmod")) {
        const float v[] = {0.0f, -0.0f, 0.5f, 1.0f, -1.5f, 3.14159f, 100.0f, 1e-30f, 1e30f, 2.5f};
        int two = !strcmp(name, "atan2f") || !strcmp(name, "powf");
        for (int i = 0; i < 10; i++) printf("%s(%a)=%a\n", name, v[i], call_f(f, 1 + two, v[i], v[(i + 3) % 10]));
    } else {
        const double v[] = {0.0, -0.0, 0.5, 1.0, -1.5, 3.141592653589793, 100.0, 1e-300, 1e300, 2.5, -7.25, 1e10};
        int two = !strcmp(name, "atan2") || !strcmp(name, "pow") || !strcmp(name, "fmod");
        for (int i = 0; i < 12; i++) printf("%s(%a)=%a\n", name, v[i], call_d(f, 1 + two, v[i], v[(i + 5) % 12]));
    }
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s libc.image.bin libc_relocs.bin libc_imports.txt [func...]\n", argv[0]); return 2; }
    map_image(argv[1], argv[2]);
    bind_imports(argv[3]);
#ifdef LIFTED
    fprintf(stderr, "ps4rt: %llu traces registered\n", (unsigned long long)ps4rt_trace_count());
#endif
    int first = 4, n = argc - 4;
    for (size_t i = 0; i < (n ? (size_t)n : sizeof libc_funcs / sizeof libc_funcs[0]); i++) {
        const char *name = n ? argv[first + i] : libc_funcs[i].name;
        fflush(stdout);
        pid_t pid = fork();
        if (pid == 0) { run(name); fflush(stdout); _exit(0); }
        int st = 0;
        waitpid(pid, &st, 0);
        if (WIFSIGNALED(st)) printf("%s: CRASH signal %d\n", name, WTERMSIG(st));
        else if (WEXITSTATUS(st)) printf("%s: exit %d\n", name, WEXITSTATUS(st));
    }
    return 0;
}
