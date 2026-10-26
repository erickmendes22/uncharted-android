#include <cassert>
#include <cstdio>
#include <chrono>
#include "bbport_gc_budget_policy.h"
#include "bbport_frame_pacing.h"
int main(){
 using namespace BbGcBudgetPolicy;
 int relaxed=0;
 auto d=Step(4096*MiB,4096*MiB,0,800*MiB,0,0,-1,relaxed);
 assert(d.bytes==3840*MiB && d.short_memory);
 d=Step(4096*MiB,4096*MiB,0,400*MiB,0,0,-1,relaxed);assert(d.bytes==3584*MiB);
 d=Step(1100*MiB,4096*MiB,0,400*MiB,0,0,-1,relaxed);assert(d.bytes==1024*MiB);
 d=Step(4096*MiB,4096*MiB,3500*MiB,400*MiB,0,0,-1,relaxed);assert(d.bytes==4096*MiB);
 d=Step(4096*MiB,4096*MiB,0,4096*MiB,4096*MiB,200*MiB,-1,relaxed);assert(d.bytes==3584*MiB);
 d=Step(4096*MiB,4096*MiB,0,4096*MiB,0,0,16,relaxed);assert(d.bytes==3584*MiB);
 relaxed=0;for(int i=0;i<4;++i){d=Step(2048*MiB,4096*MiB,0,4096*MiB,0,0,0,relaxed);assert(d.bytes==2048*MiB);}
 d=Step(2048*MiB,4096*MiB,0,4096*MiB,0,0,0,relaxed);assert(d.bytes==2176*MiB);
 d=Step(4000*MiB,4096*MiB,0,4096*MiB,0,0,0,relaxed);assert(d.bytes==4096*MiB && !d.short_memory);
 using namespace std::chrono;const steady_clock::time_point now{seconds(200)};const auto period=nanoseconds(33333333);
 auto next=BbFramePacing::Next(now+seconds(150),now,period);assert(next==now+period); // movie debt cannot survive
 next=BbFramePacing::Next(now-period*2,now,period);assert(next==now+period-period/16); // no very early frame after a late one
 assert(BbFramePacing::Next(now,now,nanoseconds(0))==now);
 std::puts("Mobile policies: PASS (RAM/swap/PSI pressure, usage floor, slow bounded recovery, post-movie pacing)");
}
