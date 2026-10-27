/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Adapted from RedVorteX-TV/Uncharted-arm android-c303023 (runtime_jobspin.c).
 * Hooks are installed before any guest execution; diagnostics are opt-in here. */
/* Uncharted's job workers (NdJobWorkerThread) wait for work by spinning: each pass scans the job
 * queues, then `pause` and again. On the PS4 every worker owns a core; under FEXCore (ARM
 * hosts) six of them spinning keep the CPU busy and take the SoC's power budget from the GPU.
 *
 * The loop (Uncharted 1, found by its bytes, so other builds with the same code match too):
 *     lea  r14, [r8+1]           ; passes without work
 *     cmp  byte [rdi+0x59], 0    ; may the workers sleep?
 *     je   A                     ; no:  A: pause; jmp next pass
 *     cmp  r8, 1000
 *     jl   B                     ; yes, fewer than 1000 passes: B: pause; jmp next pass
 *     ...                        ; else: wait on the job semaphore (woken when work arrives)
 * The `pause` at B gets a hook that, after a spin, sets the pass count to 1000: the worker
 * then sleeps on the game's own semaphore right away instead of after 1000 passes. The `pause` at
 * A, where the game offers no wait, sleeps a little between passes once the spin is long.
 *
 * BB_JOB_SPIN=<passes>,<sleep us> (default 256,20); BB_JOB_SPIN=0 leaves the game's loop as is.
 * Only on hosts where the guest CPU can hook instructions (FEXCore); x86-64 hosts keep the loop. */
#define _GNU_SOURCE
#include "runtime.h"
#include "guest_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#if !defined(__aarch64__) && !defined(BB_JOB_SPIN_TEST)
void runtime_job_spin_install(unsigned char *image, uint64_t start, uint64_t size) {
    (void)image; (void)start; (void)size;
}
void runtime_job_spin_report(void) {}
#else
static unsigned free_passes = 256, sleep_us = 20;
static int64_t wait_passes = 1000;
static unsigned long long sleeps, early_waits;
static unsigned loops_found;
static int disabled;
static int statistics;

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}
/* Passes of this thread in the current wait: a pass later than 200 us after the previous one
 * means the worker ran a job in between, so a new wait starts. Every 10 s the counts go to the log. */
static unsigned streak(void) {
    static _Thread_local uint64_t last;
    static _Thread_local unsigned count;
    static uint64_t reported;
    const uint64_t now = now_ns();
    count = now - last > 200000 ? 1 : count + 1;
    last = now;
    uint64_t seen = __atomic_load_n(&reported, __ATOMIC_RELAXED);
    if (statistics && !seen) __atomic_compare_exchange_n(&reported, &seen, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    else if (statistics && now - seen > 10000000000ull &&
             __atomic_compare_exchange_n(&reported, &seen, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        const double seconds = (double)(now - seen) / 1e9;
        printf("Job spin: %.0f early waits/s, %.0f sleeps/s\n",
               (double)__atomic_exchange_n(&early_waits, 0, __ATOMIC_RELAXED) / seconds,
               (double)__atomic_exchange_n(&sleeps, 0, __ATOMIC_RELAXED) / seconds);
        fflush(stdout);
    }
    return count;
}
static void pause_may_sleep(GuestContext *regs) { /* B */
    regs->rip += 2;
    if (streak() > free_passes && (int64_t)regs->gpr[GUEST_R14] < wait_passes) {
        regs->gpr[GUEST_R14] = (uint64_t)wait_passes; /* next pass waits on the semaphore */
        __atomic_fetch_add(&early_waits, 1, __ATOMIC_RELAXED);
    }
}
static void pause_no_wait(GuestContext *regs) { /* A */
    regs->rip += 2;
    if (streak() > free_passes && sleep_us) {
        static _Thread_local int slack;
        if (!slack) { /* the default 50 us timer slack would double the short naps */
            prctl(PR_SET_TIMERSLACK, 1000UL, 0, 0, 0);
            slack = 1;
        }
        const struct timespec nap = {0, (long)sleep_us * 1000};
        nanosleep(&nap, NULL);
        __atomic_fetch_add(&sleeps, 1, __ATOMIC_RELAXED);
    }
}

/* The bytes from `lea r14, [r8+1]` to `jl B`; 0x100 marks the rel8 bytes. */
static const unsigned short pattern[] = {
    0x4d, 0x8d, 0x70, 0x01,                   /* lea r14, [r8+1] */
    0x80, 0x7f, 0x59, 0x00,                   /* cmp byte [rdi+0x59], 0 */
    0x74, 0x100,                              /* je A */
    0x49, 0x81, 0xf8, 0x100, 0x100, 0x100, 0x100, /* cmp r8, imm32 */
    0x7c, 0x100,                              /* jl B */
};
enum { PATTERN = sizeof(pattern) / sizeof(*pattern) };

static int matches(const unsigned char *p) {
    for (int i = 0; i < PATTERN; ++i)
        if (pattern[i] < 0x100 && p[i] != pattern[i]) return 0;
    return 1;
}
void runtime_job_spin_install(unsigned char *image, uint64_t start, uint64_t size) {
    statistics = getenv("BB_JOB_SPIN_STATS") != NULL;
    const char *spec = getenv("BB_JOB_SPIN");
    if (spec && spec[0] == '0' && spec[1] == 0) { disabled = 1; return; }
    if (spec && *spec) {
        char *end = NULL;
        free_passes = (unsigned)strtoul(spec, &end, 10);
        if (end && *end == ',') sleep_us = (unsigned)strtoul(end + 1, NULL, 10);
    }
    if (free_passes == 0 || free_passes > 100000) free_passes = 256;
    if (sleep_us > 1000) sleep_us = 20;
    unsigned hooked = 0, found = 0;
    for (uint64_t at = start; size >= PATTERN && at <= start + size - PATTERN; ++at) {
        unsigned char *p = image + at;
        if (!matches(p)) continue;
        ++found;
        ++loops_found;
        int64_t a_at=(int64_t)at+10+(signed char)p[9];
        int64_t b_at=(int64_t)at+PATTERN+(signed char)p[PATTERN-1];
        int32_t limit;
        memcpy(&limit,p+13,4);
        if(a_at<(int64_t)start || b_at<(int64_t)start ||
           (uint64_t)a_at>start+size-2 || (uint64_t)b_at>start+size-2 || limit<=0 ||
           image[a_at]!=0xf3 || image[a_at+1]!=0x90 || image[b_at]!=0xf3 || image[b_at+1]!=0x90){
            printf("Job spin: loop at %#llx not safely hookable; left unchanged\n",(unsigned long long)at);
            continue;
        }
        unsigned char *a=image+a_at,*b=image+b_at;
        wait_passes = limit;
        hooked += guest_cpu_hook((uintptr_t)b, 2, pause_may_sleep) ? 1 : 0;
        hooked += guest_cpu_hook((uintptr_t)a, 2, pause_no_wait) ? 1 : 0;
        printf("Job spin: worker loop at %#llx (waits after %d passes): pause at %#llx and %#llx hooked\n",
               (unsigned long long)at, limit, (unsigned long long)(a - image),
               (unsigned long long)(b - image));
    }
    if (found)
        printf("Job spin: %u loop(s), %u hook(s); spin %u passes, then sleep %u us or wait (BB_JOB_SPIN)\n",
               found, hooked, free_passes, sleep_us);
}
void runtime_job_spin_report(void) {
    if (disabled) printf("Job spin: off (BB_JOB_SPIN=0)\n");
    else if (!loops_found) printf("Job spin: no worker loop found in this executable; its workers spin as on the PS4\n");
}
#endif
