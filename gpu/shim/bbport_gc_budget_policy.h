// SPDX-License-Identifier: GPL-2.0-or-later
// Adaptive RAM policy adapted from ducvd89/Bloodborne-Android (776f6972).
#pragma once
#include <algorithm>
#include <cstdint>
namespace BbGcBudgetPolicy {
using u64=std::uint64_t;
constexpr u64 MiB=1ull<<20;
struct Decision{u64 bytes;bool short_memory;};
inline Decision Step(u64 current,u64 ceiling,u64 used,u64 available,u64 swap_total,u64 swap_free,double psi,int& relaxed_steps){
    const double swap_used=swap_total?100.0*double(swap_total-std::min(swap_free,swap_total))/double(swap_total):0;
    const bool critical=available<512*MiB || swap_used>80 || psi>=15;
    const bool tight=critical || available<1024*MiB || swap_used>50 || psi>=5;
    const bool relaxed=available>2048*MiB && swap_used<40 && (psi<0 || psi<1);
    u64 next=current;
    if(tight){
        relaxed_steps=0;
        const u64 floor=std::min(current,std::max(1024*MiB,used/4*5));
        const u64 step=(critical?512:256)*MiB;
        next=current>floor+step?current-step:floor;
    }else if(relaxed){
        if(++relaxed_steps>=5 && current<ceiling)next=std::min(ceiling,current+128*MiB);
    }else relaxed_steps=0;
    return {next,tight || next<ceiling};
}
}
