// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace BbReadbackMerge {
using Range=std::pair<std::uint64_t,std::uint64_t>; // [begin,end), CPU-owned
inline void Normalize(std::vector<Range>& ranges) {
    std::sort(ranges.begin(),ranges.end());
    std::size_t n=0;
    for(const auto& r:ranges){
        if(r.second<=r.first)continue;
        if(n && r.first<=ranges[n-1].second)ranges[n-1].second=std::max(ranges[n-1].second,r.second);
        else ranges[n++]=r;
    }
    ranges.resize(n);
}
// CPU-owned bytes must never be overwritten with GPU data or with an earlier CPU snapshot.
// Preserving their live storage avoids losing CPU stores made after ownership classification.
// Input exclusions are normalized once for all download copies; callback receives address/size.
template<class Write>
void ForEachGpuWrite(std::uint64_t address,std::uint64_t size,std::span<const Range> cpu,Write&& write) {
    if(!size || size>std::numeric_limits<std::uint64_t>::max()-address)return;
    const auto end=address+size;
    auto cursor=address;
    auto it=std::lower_bound(cpu.begin(),cpu.end(),cursor,[](const Range& r,std::uint64_t a){return r.second<=a;});
    for(;it!=cpu.end() && it->first<end;++it){
        if(it->first>cursor)write(cursor,std::min(end,it->first)-cursor);
        cursor=std::max(cursor,std::min(end,it->second));
        if(cursor==end)return;
    }
    if(cursor<end)write(cursor,end-cursor);
}
}
