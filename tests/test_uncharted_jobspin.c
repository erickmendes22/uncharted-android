#define BB_JOB_SPIN_TEST 1
#include "../src/runtime_jobspin.c"
#include <assert.h>
static GuestPauseHook installed[16];
static uintptr_t sites[16];
static unsigned registered;
int guest_cpu_hook(uintptr_t address,size_t size,GuestPauseHook callback){
    assert(size==2 && registered<16);
    unsigned char *p=(void*)address;assert(p[0]==0xf3 && p[1]==0x90);
    installed[registered]=callback;sites[registered++]=address;
    p[0]=0x0f;p[1]=0x05;return 1;
}
static void example(unsigned char *code){
    memset(code,0x90,128);
    for(unsigned i=0;i<PATTERN;++i)code[i]=pattern[i]<256?(unsigned char)pattern[i]:0;
    code[9]=38;code[18]=45;
    int32_t limit=1000;memcpy(code+13,&limit,4);
    code[48]=code[64]=0xf3;code[49]=code[65]=0x90;
}
int main(int argc,char **argv){
    if(argc==2){
        FILE *input=fopen(argv[1],"rb");assert(input);
        assert(!fseek(input,0,SEEK_END));long size=ftell(input);assert(size>0);
        rewind(input);unsigned char *bytes=malloc((size_t)size);assert(bytes);
        assert(fread(bytes,1,(size_t)size,input)==(size_t)size);fclose(input);
        setenv("BB_JOB_SPIN","256,20",1);
        runtime_job_spin_install(bytes,0,(uint64_t)size);
        assert(registered==2);free(bytes);
        puts("Executable segment: both worker PAUSE sites validated PASS");return 0;
    }
    unsigned char code[128],original[128];example(code);memcpy(original,code,128);
    setenv("BB_JOB_SPIN","0",1);runtime_job_spin_install(code,0,128);
    assert(registered==0 && !memcmp(original,code,128));
    setenv("BB_JOB_SPIN","256,20",1);disabled=0;
    code[9]=127;runtime_job_spin_install(code,0,128);assert(registered==0);
    example(code);code[48]=0xcc;runtime_job_spin_install(code,0,128);assert(registered==0);
    example(code);runtime_job_spin_install(code,0,128);assert(registered==2);
    assert(sites[0]==(uintptr_t)(code+64) && sites[1]==(uintptr_t)(code+48));
    GuestContext regs={0},before;
    for(unsigned i=0;i<16;++i)regs.gpr[i]=0xabc000+i;
    regs.gpr[GUEST_R14]=12;regs.rip=sites[0];regs.mxcsr=0x1f80;regs.fcw=0x37f;before=regs;
    free_passes=0;sleep_us=0;installed[0](&regs);
    assert(regs.gpr[GUEST_R14]==1000 && regs.rip==before.rip+2);
    regs.gpr[GUEST_R14]=before.gpr[GUEST_R14];regs.rip=before.rip;
    assert(!memcmp(&regs,&before,sizeof(regs)));
    regs.rip=sites[1];before=regs;installed[1](&regs);before.rip+=2;
    assert(!memcmp(&regs,&before,sizeof(regs)));
    puts("Job spin: validated branches, malformed/disabled fallback and register preservation PASS");
    return 0;
}
