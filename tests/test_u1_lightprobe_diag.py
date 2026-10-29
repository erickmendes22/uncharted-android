"""Exercise actual loader thunks, constructor provenance and bounded backing history."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/probe.c').read_text()
start=s.index('static unsigned char *mobile_callback_thunk_args(')
end=s.index('static int64_t find_unique_pattern',start)
thunks=s[start:end]
harness=r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include "u1_lightprobe_diag.h"
static unsigned char *image;
#define GUEST_ABI_MS 1
static uintptr_t guest_host_function(const void *p,const char *name,int abi) {
    (void)name;(void)abi;return (uintptr_t)p;
}
static unsigned char *near_image_memory(size_t size) {
    void *p=mmap(NULL,size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(p!=MAP_FAILED);return p;
}
'''+thunks+r'''
int main(void){
    image=near_image_memory(4096);
    const unsigned char ctor[]={0x55,0x48,0x89,0xe5,0x48,0x89,0x37,0x48,0x89,0x57,0x08,0x5d,0xc3};
    /* Entry replays the genuine six-byte prologue, then copies the mask exactly as U1 does. */
    const unsigned char entry[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x48,0x8b,0x47,0x08,0x48,0x8b,0x08,0x48,0x89,0x4f,0x10,0x41,0x5f,0x5d,0xc3};
    memcpy(image,ctor,sizeof(ctor));memcpy(image+128,entry,sizeof(entry));
    assert(mobile_callback_thunk_args(image,7,(const void *)u1_probe_construct,1));
    assert(mobile_callback_thunk(image+128,6,(const void *)u1_probe_enter));
    uint64_t job[8]={0}, input[6]={UINT64_C(0x1110000000),0x1111111100,1,0,0,0x1122222200};
    uint64_t global=0x0610ba10;
    u1_probe_memfd=open("/proc/self/mem",O_RDONLY|O_CLOEXEC);
    atomic_store(&u1_diag_enabled,1);
    ((void(*)(void *,void *,void *))image)(job,&global,input);
    assert(job[0]==(uintptr_t)&global && job[1]==(uintptr_t)input);
    assert(atomic_load(&u1_jobs[0].input)==(uintptr_t)input);
    assert(atomic_load(&u1_jobs[0].words[0])==input[0]);
    ((void(*)(void *))(image+128))(job);
    assert(job[2]==input[0] && atomic_load(&u1_bad_reports)==0);
    uint64_t bad=UINT64_C(0xbfaaba3ec0a0c35e);
    memcpy(input,&bad,8);runtime_u1_note_backing((uintptr_t)input,&bad,8);
    ((void(*)(void *))(image+128))(job);
    assert(job[2]==bad && atomic_load(&u1_bad_reports)==1);
    u1_probe_fault_pc=0x8ed8da;
    u1_probe_fault(0x8ed8da,(uintptr_t)job);
    /* Invalid memory is reported without faulting, altering it or skipping gameplay. */
    assert(!u1_read(bad,&global,sizeof(global)));
    assert(!u1_overlap(UINT64_MAX-4,20,1,8));
    for(int i=0;i<U1_WRITES+1;++i)runtime_u1_note_backing(0x1100000000,&global,8);
    assert(atomic_load(&u1_write_head)==U1_WRITES+2);
    puts("PASS: original RDX preserved, exact prologues replayed, source-before-entry and backing provenance, bounded ring");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-O2','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 out=subprocess.run([str(p/'test')],capture_output=True,text=True)
 assert out.returncode==0,out.stderr
 assert 'constructor #1' in out.stderr and 'mask=0x1110000000' in out.stderr
 assert 'current input mask=0xbfaaba3ec0a0c35e' in out.stderr
 assert 'backing write #1' in out.stderr and 'first=0xbfaaba3ec0a0c35e' in out.stderr
 print(out.stdout.strip())
# No hooks in unrecognised games; no patch to a hardcoded address.
install=s[s.index('static void install_u1_probe_diagnostics'):s.index('extern _Thread_local int runtime_ndgi_open')]
assert 'if(ctor<0 || body<0 || entry<0)return;' in install
assert 'image+body+42' in install
