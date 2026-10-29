/* SPDX-License-Identifier: GPL-2.0-or-later
 * Targeted U1 provenance capture. Never changes game data, scheduling or readback policy.
 * Included by probe.c; bounded history, no background polling or per-frame log output. */
#ifndef U1_LIGHTPROBE_DIAG_H
#define U1_LIGHTPROBE_DIAG_H
#include <stdatomic.h>
#include <fcntl.h>

#if defined(__linux__)
enum { U1_JOBS = 256, U1_WRITES = 4096, U1_HEADER_WORDS = 6 };
typedef struct {
    _Atomic uint64_t serial, job, input, global, words[U1_HEADER_WORDS], readable;
} U1JobRecord;
typedef struct { _Atomic uint64_t serial, address, size, first; } U1WriteRecord;
static U1JobRecord u1_jobs[U1_JOBS];
static U1WriteRecord u1_writes[U1_WRITES];
static _Atomic uint64_t u1_job_head, u1_write_head;
static _Atomic int u1_diag_enabled, u1_bad_reports;
static uint64_t u1_probe_fault_pc;
static int u1_probe_memfd=-1;

static int u1_read(uint64_t address, void *out, size_t size) {
    if (!address || size > UINT64_MAX-address) return 0;
    struct iovec local={out,size}, remote={(void *)(uintptr_t)address,size};
    if(process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)size)return 1;
    return u1_probe_memfd>=0 && address<=INT64_MAX && pread(u1_probe_memfd,out,size,(off_t)address)==(ssize_t)size;
}
static int u1_pointer(uint64_t p) {
    return p>=UINT64_C(0x1000000000) && p<UINT64_C(0xfc00000000) && !(p&7);
}
/* This is called only for successful backing-view writes. It does not invoke GPU work,
 * acquire runtime locks, read protected guest memory, or print. All fields are atomic so
 * a fatal-report reader can reject an overwritten/incomplete record without a data race. */
void runtime_u1_note_backing(uintptr_t address, const void *data, uint64_t size) {
    if (!atomic_load_explicit(&u1_diag_enabled,memory_order_relaxed) || !size) return;
    uint64_t serial=atomic_fetch_add_explicit(&u1_write_head,1,memory_order_relaxed)+1;
    U1WriteRecord *r=&u1_writes[(serial-1)%U1_WRITES];
    atomic_store_explicit(&r->serial,0,memory_order_release);
    uint64_t first=0; memcpy(&first,data,size<8?(size_t)size:8);
    atomic_store_explicit(&r->address,address,memory_order_relaxed);
    atomic_store_explicit(&r->size,size,memory_order_relaxed);
    atomic_store_explicit(&r->first,first,memory_order_relaxed);
    atomic_store_explicit(&r->serial,serial,memory_order_release);
}
static void __attribute__((ms_abi)) u1_probe_construct(uint64_t job,uint64_t global,uint64_t input,uint64_t unused) {
    (void)unused;
    uint64_t words[U1_HEADER_WORDS]={0};
    int ok=u1_read(input,words,sizeof(words));
    uint64_t serial=atomic_fetch_add_explicit(&u1_job_head,1,memory_order_relaxed)+1;
    U1JobRecord *r=&u1_jobs[(serial-1)%U1_JOBS];
    atomic_store_explicit(&r->serial,0,memory_order_release);
    atomic_store_explicit(&r->job,job,memory_order_relaxed);
    atomic_store_explicit(&r->input,input,memory_order_relaxed);
    atomic_store_explicit(&r->global,global,memory_order_relaxed);
    for(int k=0;k<U1_HEADER_WORDS;++k)atomic_store_explicit(&r->words[k],words[k],memory_order_relaxed);
    atomic_store_explicit(&r->readable,ok,memory_order_relaxed);
    atomic_store_explicit(&r->serial,serial,memory_order_release);
}
static int u1_overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m) {
    return n && m && n<=UINT64_MAX-a && m<=UINT64_MAX-b && a<b+m && b<a+n;
}
static void u1_probe_dump(uint64_t job,const char *reason) {
    uint64_t object[8]={0}, input=0;
    const int object_ok=u1_read(job,object,sizeof(object));
    if(object_ok)input=object[1];
    fprintf(stderr,"U1 light probes: %s job=%#llx object-readable=%d input=%#llx copied-mask=%#llx\n",
            reason,(unsigned long long)job,object_ok,(unsigned long long)input,(unsigned long long)object[2]);
    uint64_t n=atomic_load_explicit(&u1_job_head,memory_order_acquire), saved_input=0;
    for(uint64_t serial=n;serial && n-serial<U1_JOBS;--serial){
        U1JobRecord *r=&u1_jobs[(serial-1)%U1_JOBS];
        if(atomic_load_explicit(&r->serial,memory_order_acquire)!=serial || atomic_load_explicit(&r->job,memory_order_relaxed)!=job)continue;
        uint64_t words[U1_HEADER_WORDS], src=atomic_load_explicit(&r->input,memory_order_relaxed);
        uint64_t ok=atomic_load_explicit(&r->readable,memory_order_relaxed);
        for(int k=0;k<U1_HEADER_WORDS;++k)words[k]=atomic_load_explicit(&r->words[k],memory_order_relaxed);
        if(atomic_load_explicit(&r->serial,memory_order_acquire)!=serial)continue;
        saved_input=src;
        fprintf(stderr,"U1 light probes: constructor #%llu input=%#llx readable=%llu mask=%#llx table=%#llx flags=%#llx params=%#llx\n",
            (unsigned long long)serial,(unsigned long long)src,(unsigned long long)ok,
            (unsigned long long)words[0],(unsigned long long)words[1],(unsigned long long)words[2],(unsigned long long)words[5]);
        break;
    }
    uint64_t current[U1_HEADER_WORDS]={0};
    if(u1_read(input,current,sizeof(current)))fprintf(stderr,
        "U1 light probes: current input mask=%#llx table=%#llx flags=%#llx params=%#llx\n",
        (unsigned long long)current[0],(unsigned long long)current[1],(unsigned long long)current[2],(unsigned long long)current[5]);
    n=atomic_load_explicit(&u1_write_head,memory_order_acquire); int shown=0;
    for(uint64_t serial=n;serial && n-serial<U1_WRITES && shown<24;--serial){
        U1WriteRecord *r=&u1_writes[(serial-1)%U1_WRITES];
        if(atomic_load_explicit(&r->serial,memory_order_acquire)!=serial)continue;
        uint64_t a=atomic_load_explicit(&r->address,memory_order_relaxed),sz=atomic_load_explicit(&r->size,memory_order_relaxed),first=atomic_load_explicit(&r->first,memory_order_relaxed);
        if(atomic_load_explicit(&r->serial,memory_order_acquire)!=serial)continue;
        if(!u1_overlap(a,sz,job,sizeof(object)) && !u1_overlap(a,sz,input,sizeof(current)) &&
           !u1_overlap(a,sz,saved_input,sizeof(current)))continue;
        fprintf(stderr,"U1 light probes: backing write #%llu at=%#llx bytes=%llu first=%#llx\n",
                (unsigned long long)serial,(unsigned long long)a,(unsigned long long)sz,(unsigned long long)first); ++shown;
    }
    fprintf(stderr,"U1 light probes: backing history=%llu (last %d), matching=%d; absence does not exclude older/guest CPU writes\n",
            (unsigned long long)n,U1_WRITES,shown);
    fflush(stderr);
}
static void __attribute__((ms_abi)) u1_probe_enter(uint64_t job,uint64_t unused1,uint64_t unused2,uint64_t unused3) {
    (void)unused1;(void)unused2;(void)unused3;
    uint64_t object[4], mask=0;
    if(!u1_read(job,object,sizeof(object)) || !u1_read(object[1],&mask,8) || !u1_pointer(mask)) {
        if(atomic_fetch_add_explicit(&u1_bad_reports,1,memory_order_relaxed)<2)u1_probe_dump(job,"invalid input before entry");
    }
}
static void u1_probe_fault(uint64_t pc,uint64_t job) {
    if(atomic_load_explicit(&u1_diag_enabled,memory_order_relaxed) && pc==u1_probe_fault_pc)
        u1_probe_dump(job,"fatal mask load");
}
#else
static void u1_probe_fault(uint64_t pc,uint64_t job){(void)pc;(void)job;}
#endif
#endif
