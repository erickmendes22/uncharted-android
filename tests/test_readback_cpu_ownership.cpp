// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_readback_merge.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
using namespace BbReadbackMerge;
int main(){
    std::array<unsigned char,32> cpu{},old{},gpu;gpu.fill(0xaa);
    std::vector<Range> owned{{8,16}};
    // Reproduce the former save/GPU-write/restore path losing a later CPU store.
    auto saved=old;
    old[8]=0x77;std::memcpy(old.data(),gpu.data(),32);
    std::memcpy(old.data()+8,saved.data()+8,8);assert(old[8]==0);
    // The same CPU store after classification survives every GPU write in the new path.
    bool first=true;
    ForEachGpuWrite(0,32,owned,[&](auto address,auto size){
        if(first){cpu[8]=0x77;first=false;}
        std::memcpy(cpu.data()+address,gpu.data()+address,size);
    });
    assert(cpu[8]==0x77 && cpu[0]==0xaa && cpu[31]==0xaa);
    for(int i=9;i<16;++i)assert(cpu[i]==0);
    std::mt19937 rng(17);
    for(int test=0;test<5000;++test){
        std::array<bool,128> excluded{},written{};
        std::vector<Range> ranges;
        for(unsigned n=rng()%30;n;--n){
            unsigned a=rng()%160,b=rng()%160;if(a>b)std::swap(a,b);
            ranges.emplace_back(a,b);
            for(unsigned i=a;i<b && i<128;++i)excluded[i]=true;
        }
        Normalize(ranges);
        unsigned address=rng()%100,size=rng()%(129-address);
        ForEachGpuWrite(address,size,ranges,[&](auto a,auto n){
            assert(a>=address && n && a+n<=address+size);
            for(auto i=a;i<a+n;++i){assert(!written[i] && !excluded[i]);written[i]=true;}
        });
        for(unsigned i=0;i<128;++i)assert(written[i]==(i>=address && i<address+size && !excluded[i]));
    }
    bool called=false;ForEachGpuWrite(UINT64_MAX-4,8,{},[&](auto,auto){called=true;});assert(!called);
    puts("PASS: old merge deterministically loses a concurrent CPU store; new readback preserves live CPU-owned bytes and writes all GPU-owned ranges");
}
