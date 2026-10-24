// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: small read-only guest buffers (constants) copied by the GPU command thread (stage A of
// the draw pipeline, vk_draw_pipe.h) into this ring, so the draw recording thread only binds
// them. Positions are monotonic byte counts; a region is reused once the submission that
// recorded its last draw has completed on the GPU. Stage B stamps each packet it records with
// the submission tick (Stamp), stage A retires stamps whose tick the GPU has passed.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <optional>
#include <utility>

#include "common/assert.h"
#include "common/types.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

class ConstantRing {
public:
    static constexpr u64 Capacity = 32ull << 20;

    ConstantRing(const Instance& instance, Scheduler& scheduler_)
        : scheduler{scheduler_}, buffer{instance, 0, Capacity, RingMemoryType(),
                                        "bbport constant ring"} {
        ASSERT(!buffer.mapped_data.empty());
        std::printf("Constant ring: %s memory%s\n",
                    RingMemoryType() == VideoCore::MemoryType::HostCached ? "cached" : "stream",
                    buffer.is_coherent ? ", coherent" : ", flushed");
    }

    // g2-arm64: BB_RING_CACHED=1 — cached host memory (on Adreno with Turnip cached and
    // IO-coherent) instead of write-combined: stage A writes several hundred MB/s here.
    static VideoCore::MemoryType RingMemoryType() {
        const char* env = std::getenv("BB_RING_CACHED");
        return env && env[0] == '1' ? VideoCore::MemoryType::HostCached
                                    : VideoCore::MemoryType::Stream;
    }

    [[nodiscard]] vk::Buffer Handle() const noexcept {
        return buffer.Handle();
    }

    [[nodiscard]] vk::DeviceAddress DeviceAddress() const noexcept {
        return buffer.BufferDeviceAddress();
    }

    /// Stage A: `size` bytes at `alignment`; returns the ring offset, or nullopt when the ring is
    /// still in use by the GPU (the caller leaves the binding to stage B).
    std::optional<u64> Allocate(u64 size, u64 alignment) {
        u64 at = (position + alignment - 1) & ~(alignment - 1);
        if (at % Capacity + size > Capacity) {
            at += Capacity - at % Capacity; // wrap to the ring start
        }
        const auto in_use = [&] { return at + size > Capacity && at + size - Capacity > Kept(); };
        if (in_use()) {
            Retire();
            if (in_use()) {
                return std::nullopt;
            }
        }
        position = at + size;
        last_at = at;
        return at % Capacity;
    }

    /// Stage A, g2-arm64 (BB_STREAM_DEDUPE): a copy made in the current epoch may be bound again
    /// by later packets of that epoch. An epoch ends after EpochBytes of allocations; its bytes
    /// are kept until every packet produced in it has completed (its end position retired).
    static constexpr u64 EpochBytes = 8ull << 20;
    void EnableEpochs() {
        epochs = true;
        epoch_start = position;
    }
    [[nodiscard]] u64 EpochStart() const noexcept {
        return epoch_start;
    }
    /// The position of the last allocation (compared with EpochStart).
    [[nodiscard]] u64 LastAllocation() const noexcept {
        return last_at;
    }
    /// Stage A, before a packet's allocations.
    void NextEpochIfFull() {
        if (epochs && position - epoch_start >= EpochBytes) {
            live_epochs.push_back({epoch_start, position});
            epoch_start = position;
        }
    }

    [[nodiscard]] u8* Data(u64 offset) noexcept {
        return buffer.mapped_data.data() + offset;
    }

    /// Stage A, after writing: makes the bytes visible to the GPU.
    void Flush(u64 offset, u64 size) {
        buffer.Flush(offset, size);
    }

    /// Stage A: the position the next packet's allocations end at.
    [[nodiscard]] u64 Position() const noexcept {
        return position;
    }

    /// Stage B, after recording a packet whose allocations end at `end`. Stamps of one
    /// submission tick are merged: only the last end per tick is published.
    void Stamp(u64 end) {
        const u64 tick = scheduler.CurrentTick();
        if (end == pending_end) {
            // g2-arm64: a later packet that allocated nothing may bind copies made before it
            // (deduplicated): the end retires with the latest packet that has it.
            pending_tick = tick;
            return;
        }
        if (tick != pending_tick && pending_end != last_published) {
            Publish(pending_end, pending_tick);
        }
        pending_end = end;
        pending_tick = tick;
    }

private:
    void Publish(u64 end, u64 tick) {
        const u64 at = stamps_head.load(std::memory_order_relaxed);
        if (at - stamps_tail.load(std::memory_order_acquire) >= stamps.size()) {
            return; // full: this region retires with a later stamp
        }
        stamps[at % stamps.size()] = {end, tick};
        stamps_head.store(at + 1, std::memory_order_release);
        last_published = end;
    }

    /// Stage A: the position before which the ring may be overwritten.
    u64 Kept() {
        if (!epochs) {
            return retired;
        }
        while (!live_epochs.empty() && live_epochs.front().second <= retired) {
            live_epochs.pop_front();
        }
        u64 kept = std::min(retired, epoch_start);
        if (!live_epochs.empty()) {
            kept = std::min(kept, live_epochs.front().first);
        }
        return kept;
    }

    void Retire() {
        auto* semaphore = scheduler.GetWorkSemaphore();
        semaphore->Refresh();
        u64 at = stamps_tail.load(std::memory_order_relaxed);
        const u64 head = stamps_head.load(std::memory_order_acquire);
        while (at < head && semaphore->IsFree(stamps[at % stamps.size()].tick)) {
            retired = stamps[at % stamps.size()].end;
            ++at;
        }
        stamps_tail.store(at, std::memory_order_release);
    }

    struct StampEntry {
        u64 end;
        u64 tick;
    };
    Scheduler& scheduler;
    VideoCore::Buffer buffer;
    u64 position = 0;     ///< stage A
    u64 retired = 0;      ///< stage A: the GPU is done with everything before this
    u64 last_at = 0;      ///< stage A
    bool epochs = false;  ///< stage A
    u64 epoch_start = 0;  ///< stage A
    std::deque<std::pair<u64, u64>> live_epochs; ///< stage A: closed epochs (start, end)
    u64 pending_end = 0, pending_tick = 0, last_published = 0; ///< stage B
    std::array<StampEntry, 1024> stamps{};
    alignas(64) std::atomic<u64> stamps_head{0}; ///< stage B writes
    alignas(64) std::atomic<u64> stamps_tail{0}; ///< stage A reads
};

} // namespace Vulkan
