// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>

#include "video_core/renderer_vulkan/vk_gpu_profiler.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

void GpuProfiler::Init(const Instance& instance, Scheduler& scheduler) {
    const char* env = std::getenv("BB_GPU_PROFILE");
    if (!env || env[0] != '1' || instance_ptr) {
        return;
    }
    instance_ptr = new GpuProfiler(instance, scheduler);
}

GpuProfiler::GpuProfiler(const Instance& instance, Scheduler& scheduler_)
    : device{instance.GetDevice()}, scheduler{scheduler_} {
    const vk::QueryPoolCreateInfo info = {
        .queryType = vk::QueryType::eTimestamp,
        .queryCount = NumSlices * SliceQueries,
    };
    pool = Check<"create timestamp query pool">(device.createQueryPoolUnique(info));
    device.resetQueryPool(*pool, 0, NumSlices * SliceQueries);
    period_ns = instance.GetPhysicalDevice().getProperties().limits.timestampPeriod;
    for (auto& k : keys) {
        k.reserve(SliceQueries);
    }
    std::printf("GPU profile: on (timestamp period %.2f ns)\n", period_ns);
}

void GpuProfiler::WriteTimestamp(u64 key) {
    std::scoped_lock lock{mutex};
    // Outside render passes: radv_CmdWriteTimestamp2 crashed inside some. Marks sit where a
    // pass, dispatch or submission ends anyway.
    scheduler.EndRendering();
    current = key;
    // g2-arm64: the query and its label are taken when the command reaches the command buffer
    // (the Vulkan recording thread, in stream order). The GPU command thread and the draw
    // recording thread both mark segments: queries taken in call order came out of order, and
    // those intervals were dropped (about half of the GPU's time went uncounted).
    scheduler.Record([this, key](vk::CommandBuffer cmdbuf) { Stamp(cmdbuf, key); });
}

void GpuProfiler::Stamp(vk::CommandBuffer cmdbuf, u64 key) {
    std::scoped_lock lock{mutex};
    if (used[slice] + 1 >= SliceQueries) {
        return; // the frame's slice is full: the rest of the frame goes to the last label
    }
    const u32 query = slice * SliceQueries + used[slice]++;
    keys[slice].push_back(key);
    cmdbuf.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *pool, query);
}

void GpuProfiler::BeginFrame() {
    std::scoped_lock lock{mutex};
    scheduler.EndRendering();
    // g2-arm64: in stream order too; the open segment goes on in the next frame's slice.
    scheduler.Record([this, label = current](vk::CommandBuffer cmdbuf) { NextSlice(cmdbuf, label); });
}

void GpuProfiler::NextSlice(vk::CommandBuffer cmdbuf, u64 label) {
    std::scoped_lock lock{mutex};
    // Close the frame: one more timestamp without a label.
    if (used[slice] > 0 && used[slice] < SliceQueries) {
        const u32 query = slice * SliceQueries + used[slice]++;
        cmdbuf.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *pool, query);
        pending[slice] = true;
    }
    slice = (slice + 1) % NumSlices;
    // The oldest slice: its frame was submitted eight profiler frames ago.
    if (pending[slice]) {
        Collect(slice);
    }
    if (used[slice]) {
        device.resetQueryPool(*pool, slice * SliceQueries, used[slice]);
    }
    used[slice] = 0;
    keys[slice].clear();
    pending[slice] = false;
    if (label) {
        Stamp(cmdbuf, label);
    }
    Print();
}

void GpuProfiler::Collect(u32 which) {
    const u32 count = used[which];
    std::vector<u64> stamps(count);
    // Four frames on this is complete unless the GPU lags that far. g2-arm64: no wait (a
    // timestamp recorded out of slot order may never come): an incomplete frame is skipped.
    const auto result = device.getQueryPoolResults(
        *pool, which * SliceQueries, count, count * sizeof(u64), stamps.data(), sizeof(u64),
        vk::QueryResultFlagBits::e64);
    if (result != vk::Result::eSuccess) {
        ++skipped;
        return;
    }
    std::vector<Gap> gaps;
    for (u32 i = 0; i + 1 < count; ++i) {
        if (stamps[i + 1] < stamps[i]) {
            ++out_of_order;
            continue;
        }
        const double ms = double(stamps[i + 1] - stamps[i]) * period_ns / 1e6;
        auto& total = totals[keys[which][i]];
        total.ms += ms;
        ++total.segments;
        if (keys[which][i] == IdleKey) {
            const size_t bucket = std::ranges::upper_bound(IdleBounds, ms) - IdleBounds.begin();
            idle_lengths[bucket].ms += ms;
            ++idle_lengths[bucket].segments;
            gaps.push_back({ms, i > 0 ? keys[which][i - 1] : 0,
                            i + 2 < count ? keys[which][i + 1] : 0, u32(gaps.size())});
        }
    }
    last_gap_count = u32(gaps.size());
    std::ranges::sort(gaps, [](const Gap& a, const Gap& b) { return a.ms > b.ms; });
    gaps.resize(std::min<size_t>(gaps.size(), 6));
    last_gaps = std::move(gaps);
    ++frames;
}

void GpuProfiler::Print() {
    const auto now = std::chrono::steady_clock::now();
    if (now - window < std::chrono::seconds(5) || frames == 0) {
        return;
    }
    window = now;
    std::vector<std::pair<u64, Total>> sorted(totals.begin(), totals.end());
    std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
    double sum = 0;
    for (const auto& [key, total] : sorted) {
        sum += total.ms;
    }
    // g2-arm64: a profiler frame runs from one display pass to the next (U1 has more than one
    // per guest frame); ms per second divided by the frame rate is ms per guest frame.
    const double secs = std::chrono::duration<double>(now - window_start).count();
    window_start = now;
    std::printf("GPU profile: %.2f ms/frame over %llu frames (%.0f ms/s), %zu labels; frames not "
                "ready %llu, intervals out of order %llu\n",
                sum / frames, static_cast<unsigned long long>(frames), sum / secs, sorted.size(),
                static_cast<unsigned long long>(skipped),
                static_cast<unsigned long long>(out_of_order));
    skipped = out_of_order = 0;
    for (size_t i = 0; i < std::min<size_t>(sorted.size(), 30); ++i) {
        const auto& [key, total] = sorted[i];
        std::printf("  %6.3f ms/frame %5.1f/frame  %s\n", total.ms / frames,
                    double(total.segments) / frames, described[key].c_str());
    }
    std::printf("  idle gaps by length (ms: count/frame, ms/frame):");
    for (size_t b = 0; b < idle_lengths.size(); ++b) {
        if (b < IdleBounds.size()) {
            std::printf(" <%g: %.1f, %.2f;", IdleBounds[b], double(idle_lengths[b].segments) / frames,
                        idle_lengths[b].ms / frames);
        } else {
            std::printf(" >=%g: %.1f, %.2f\n", IdleBounds.back(),
                        double(idle_lengths[b].segments) / frames, idle_lengths[b].ms / frames);
        }
    }
    std::printf("  longest idle gaps of the last frame (%u gaps):\n", last_gap_count);
    const auto name = [this](u64 key) { return key ? described[key].c_str() : "(frame edge)"; };
    for (const auto& gap : last_gaps) {
        std::printf("    %6.3f ms (#%u) after: %s | before: %s\n", gap.ms, gap.index, name(gap.before),
                    name(gap.after));
    }
    idle_lengths = {};
    totals.clear();
    frames = 0;
}

} // namespace Vulkan
