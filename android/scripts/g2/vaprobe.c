// Address space and CPU feature probe for the G2 (and any aarch64 Linux/Android).
// Build (WSL): aarch64-linux-gnu-gcc -O2 -static vaprobe.c -o vaprobe
// As a library (probe_app): -DVAPROBE_LIB, call vaprobe_report(FILE *).
// Reserves only PROT_NONE + MAP_NORESERVE: does not touch physical memory.
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static int try_reserve(uint64_t addr, uint64_t size, int *err) {
    void *p = mmap((void *)addr, size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) { *err = errno; return 0; }
    if ((uint64_t)p != addr) { munmap(p, size); *err = EEXIST; return 0; }  // old kernels: hint only
    munmap(p, size);
    *err = 0;
    return 1;
}

static void range(FILE *out, const char *what, uint64_t lo, uint64_t hi) {
    int err;
    int ok = try_reserve(lo, hi - lo, &err);
    fprintf(out, "reserve %-34s 0x%011llx..0x%011llx (%6.1f GiB): %s\n", what, (unsigned long long)lo,
           (unsigned long long)hi, (double)(hi - lo) / (1ull << 30), ok ? "OK" : strerror(err));
}

struct bit { int hwcap2; int bit; const char *name; };
static const struct bit bits[] = {
    {0, 8, "atomics (LSE)"}, {0, 25, "uscat (LSE2)"}, {0, 15, "lrcpc"}, {0, 26, "ilrcpc"},
    {1, 46, "lrcpc3"}, {1, 47, "lse128"}, {0, 27, "flagm"}, {1, 7, "flagm2"}, {1, 20, "afp"},
    {0, 22, "sve"}, {1, 1, "sve2"}, {1, 23, "sme"}, {0, 20, "asimddp"}, {1, 13, "i8mm"},
    {1, 14, "bf16"}, {1, 8, "frint"}, {1, 34, "cssc"}, {1, 43, "mops"}, {1, 18, "mte"},
};

int vaprobe_report(FILE *out) {
    fprintf(out, "page size: %ld\n", sysconf(_SC_PAGESIZE));
    unsigned long hw = getauxval(AT_HWCAP), hw2 = getauxval(AT_HWCAP2);
    fprintf(out, "AT_HWCAP=0x%lx AT_HWCAP2=0x%lx\n", hw, hw2);
    for (size_t i = 0; i < sizeof bits / sizeof bits[0]; i++)
        fprintf(out, "  %-14s %s\n", bits[i].name,
               ((bits[i].hwcap2 ? hw2 : hw) >> bits[i].bit) & 1 ? "yes" : "no");

    // VA width: the highest 1 MiB piece below 2^b that can be mapped.
    int va_bits = 0;
    for (int b = 36; b <= 52; b++) {
        int err;
        uint64_t addr = (1ull << b) - (2ull << 20);
        int ok = try_reserve(addr, 1ull << 20, &err);
        fprintf(out, "probe below 2^%d (0x%llx): %s\n", b, (unsigned long long)addr, ok ? "OK" : strerror(err));
        if (ok) va_bits = b;
    }
    fprintf(out, "user VA bits (max mappable): %d\n", va_bits);

    // Guest ranges (notes/findings.md: the reference used 0x10_0000_0000..0x15_0380_0000).
    range(out, "guest used (eboot run 1)", 0x1000000000ull, 0x1600000000ull);
    range(out, "LOW_MIN..0x16 (image+guest)", 0x0800000000ull, 0x1600000000ull);
    range(out, "LOW_MIN..0x20 (with headroom)", 0x0800000000ull, 0x2000000000ull);
    range(out, "port USER_MIN..USER_MAX", 0x1000000000ull, 0xFC00000000ull);
    range(out, "image @0x8_0000_0000 (64 MiB)", 0x0800000000ull, 0x0804000000ull);

    // The process map: where its own mappings are.
    FILE *f = fopen("/proc/self/maps", "r");
    if (f) {
        char line[512];
        fputs("--- /proc/self/maps\n", out);
        while (fgets(line, sizeof line, f)) fputs(line, out);
        fclose(f);
    }
    return 0;
}

#ifndef VAPROBE_LIB
int main(void) { return vaprobe_report(stdout); }
#endif
