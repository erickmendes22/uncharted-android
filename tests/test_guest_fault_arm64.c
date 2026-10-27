#include "guest_fault_arm64.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    uint64_t host[31];for(unsigned i=0;i<31;++i)host[i]=0x100000+i;
    GuestContext live={0};guest_fault_arm64_gprs(&live,host,0x123456);
    assert(live.rip==0x123456);
    assert(live.gpr[GUEST_RAX]==host[4] && live.gpr[GUEST_RSP]==host[8]);
    assert(live.gpr[GUEST_RBP]==host[9] && live.gpr[GUEST_R14]==host[19]);
    assert(live.gpr[GUEST_R15]==host[29] && live.gpr[GUEST_RCX]==host[7]);
    /* Reproduce the uploaded U2 log: synchronized RSP differs from live RSP. */
    GuestContext stale={0};stale.rip=0x75d3422398;stale.gpr[GUEST_RSP]=0x12011fbee8;
    GuestContext before=stale;
    host[8]=0x12011fbda0;host[9]=0x12011fbee0;host[4]=host[6]=host[19]=0;
    guest_fault_arm64_gprs(&live,host,0xbadcafe);
    assert(live.gpr[GUEST_RSP]!=stale.gpr[GUEST_RSP]);
    assert(live.gpr[GUEST_RAX]==0 && live.gpr[GUEST_RBX]==0 && live.gpr[GUEST_R14]==0);
    assert(!memcmp(&stale,&before,sizeof(stale)));
    puts("PASS: pinned FEX live register mapping, stale snapshot separation and no guest state mutation");
}
