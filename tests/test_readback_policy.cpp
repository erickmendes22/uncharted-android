#include "bbport_readback_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace BbReadback;
    assert(DrawFlushLimit(nullptr,true)==100);
    assert(DrawFlushLimit(nullptr,false)==0);
    assert(DrawFlushLimit("",true)==100);
    assert(DrawFlushLimit("0",true)==0);
    assert(DrawFlushLimit("250",false)==250);
    assert(DrawFlushLimit("999999",true)==10000);
    assert(DrawFlushLimit("-1",true)==0);
    assert(DrawFlushLimit("100oops",true)==0);
    puts("GPU batch limits: disabled/default/explicit/malformed inputs PASS");
}
