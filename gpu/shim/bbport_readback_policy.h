// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstdlib>

namespace BbReadback {
// A pending recording has no submitted timeline signal yet. Waiting for that
// tick on its owner thread would deadlock; let the existing full-readback path
// flush it instead. Completed pages need no wait.
constexpr bool CanWaitProducer(std::uint64_t producer,std::uint64_t current,bool complete) {
    return producer && !complete && producer<current;
}
inline unsigned DrawFlushLimit(const char* value,bool mapped) {
    if(!value||!*value)return mapped?100u:0u;
    char* end=nullptr;
    const unsigned long count=std::strtoul(value,&end,10);
    if(value[0]=='-'||end==value||*end)return 0;
    return count>10000?10000u:static_cast<unsigned>(count);
}
}
