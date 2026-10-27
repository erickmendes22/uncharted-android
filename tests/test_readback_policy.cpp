#include "bbport_readback_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace BbReadback;
    assert(!CanWaitProducer(0,2,false));
    assert(!CanWaitProducer(4,4,false));
    assert(!CanWaitProducer(5,4,false));
    assert(!CanWaitProducer(3,4,true));
    assert(CanWaitProducer(3,4,false));
    assert(DrawFlushLimit(nullptr,true)==100);
    assert(DrawFlushLimit(nullptr,false)==0);
    assert(DrawFlushLimit("",true)==100);
    assert(DrawFlushLimit("0",true)==0);
    assert(DrawFlushLimit("250",false)==250);
    assert(DrawFlushLimit("999999",true)==10000);
    assert(DrawFlushLimit("-1",true)==0);
    assert(DrawFlushLimit("100oops",true)==0);
    puts("Readback: completed/current/future producer ticks and batching limits PASS");
}
