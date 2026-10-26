#define _GNU_SOURCE
#include "runtime.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    const unsigned char code[]={0x90,0x48,0x8b,0x11,0x22,0x90,0x48,0x8b,0x33,0x22};
    size_t at=999,n=0;
    assert(runtime_find_guest_fix(code,sizeof(code),"48 8b 11 22",&at,&n)==1 && at==1 && n==4);
    assert(runtime_find_guest_fix(code,sizeof(code),"48 8b ?? 22",&at,&n)==2);
    assert(runtime_find_guest_fix(code,3,"48 8b 11 22",&at,&n)==0);
    assert(runtime_find_guest_fix(code,sizeof(code),"?? ??",&at,&n)==0);
    assert(runtime_find_guest_fix(code,sizeof(code),"48 8",&at,&n)==0);
    assert(runtime_find_guest_fix(code,sizeof(code),"48 zz",&at,&n)==0);
    assert(runtime_find_guest_fix(code,sizeof(code),"ff ff",&at,&n)==0);
    size_t count=0; const RuntimeGuestFix *fixes=runtime_guest_fixes(&count);
    assert(count==6);
    for (size_t i=0;i<count;++i) {
        runtime_find_guest_fix(code,0,fixes[i].pattern,&at,&n);
        assert(n>0 && fixes[i].offset<=n && fixes[i].moved<=n-fixes[i].offset);
    }
    puts("Guest fix pattern safety checks passed");
}
