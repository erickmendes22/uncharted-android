// SPDX-License-Identifier: GPL-2.0-or-later
// Linux fork: guest memory a WriteData has been chained to write (RunWriteData, BB_CHAIN_WRITEDATA)
// but not yet written. The draw recording thread goes on past such a write; GPU work recorded
// meanwhile that reads the same memory waits for it first (Scheduler::WaitDeferredSignals), as
// a real command processor's later packets would see the write.
#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "common/types.h"

namespace VideoCore::ChainedWrites {

struct Entry {
    VAddr begin;
    VAddr end;
    std::shared_ptr<std::atomic<bool>> landed;
};

struct State {
    std::mutex mutex;
    std::vector<Entry> entries;
    std::atomic<u32> pending{0};
};

inline State& Get() {
    static State state;
    return state;
}

/// The GPU command thread and the draw recording thread may wait for a chained write (they are
/// not on its way: the Vulkan recording and copy threads are).
inline thread_local bool may_wait = false;
inline void AllowWait() {
    may_wait = true;
}

/// Notes a write of [address, address + size); the returned flag is set once it is in memory.
inline std::shared_ptr<std::atomic<bool>> Add(VAddr address, u64 size) {
    auto landed = std::make_shared<std::atomic<bool>>(false);
    auto& s = Get();
    std::scoped_lock lk{s.mutex};
    s.entries.push_back({address, address + size, landed});
    s.pending.fetch_add(1, std::memory_order_release);
    return landed;
}

/// Called by the write itself once done.
inline void Landed(const std::shared_ptr<std::atomic<bool>>& landed) {
    landed->store(true, std::memory_order_release);
    Get().pending.fetch_sub(1, std::memory_order_release);
}

/// Whether a chained write into [address, address + size) has not landed yet.
inline bool Overlaps(VAddr address, u64 size) {
    auto& s = Get();
    if (s.pending.load(std::memory_order_acquire) == 0) {
        return false;
    }
    std::scoped_lock lk{s.mutex};
    std::erase_if(s.entries, [](const Entry& e) { return e.landed->load(std::memory_order_acquire); });
    const VAddr end = address + size;
    return std::any_of(s.entries.begin(), s.entries.end(),
                       [&](const Entry& e) { return address < e.end && e.begin < end; });
}

/// Whether the caller must wait (WaitDeferredSignals) before reading [address, address + size).
inline bool MustWait(VAddr address, u64 size) {
    return may_wait && Overlaps(address, size);
}

} // namespace VideoCore::ChainedWrites
