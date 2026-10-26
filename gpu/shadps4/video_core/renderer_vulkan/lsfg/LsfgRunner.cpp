// SPDX-License-Identifier: GPL-3.0-or-later
//
// Uncharted ARM port: see LsfgRunner.h. The call sequence per presented frame is ARMSX2's
// (GSLsfg.cpp): Process the frame, ask the pacer how many to generate, then GenerateInto each.

#include <algorithm>

#include "FrameGen.h"
#include "LsfgHost.h"
#include "LsfgRunner.h"
#include "LsfgVkCompat.h"

namespace Vulkan::Lsfg {

struct Runner::Impl {
    LsfgHost host;
    std::optional<Vulkan::Device> device;
    std::optional<MemoryAllocator> allocator;
    std::optional<FrameGen> frame_gen;
};

Runner::Runner(const DeviceInfo& info) : impl{std::make_unique<Impl>()} {
    impl->host.device = info.device;
    impl->host.vulkan_memory_model = info.vulkan_memory_model;
    impl->host.null_descriptor = info.null_descriptor;
    impl->host.shader_float16 = info.shader_float16;
    impl->host.wait_idle = info.wait_idle;
    LsfgHost::SetInstance(&impl->host);
    impl->device.emplace(&impl->host);
    impl->allocator.emplace(info.allocator);
    impl->frame_gen.emplace(*impl->allocator, &impl->host);
}

Runner::~Runner() {
    LsfgHost::SetInstance(&impl->host);
    impl->frame_gen.reset(); // waits for the device before its objects go
    LsfgHost::SetInstance(nullptr);
}

std::uint32_t Runner::Generate(VkCommandBuffer cmdbuf, const Settings& settings, VkImage frame,
                               VkExtent2D extent, VkFormat format, VkExtent2D render_extent,
                               std::span<const Target> outputs) {
    if (outputs.empty() || !frame) {
        return 0;
    }
    LsfgHost::SetInstance(&impl->host);
    auto& config = LsfgConfig();
    config.enabled = true;
    config.multiplier = std::clamp<std::uint32_t>(settings.multiplier, 2, 4);
    config.flow_scale = u8(std::clamp<std::uint32_t>(settings.flow_scale, 25, 100));
    config.fp16 = settings.fp16;
    config.dll_path = settings.dll_path;

    auto& frame_gen = *impl->frame_gen;
    const vk::CommandBuffer cmd{cmdbuf};
    // The storage view only has to exist (ARMSX2 passes one of its generation targets too).
    frame_gen.Process(*impl->device, cmd, frame, outputs[0].view, extent, format, render_extent);
    const size_t capacity = std::min(outputs.size(), VideoCore::FrameGen::MAX_GENERATIONS);
    const size_t count =
        std::min(frame_gen.WantedGenerations(capacity), frame_gen.GeneratedFrameCount());
    for (size_t i = 0; i < count; ++i) {
        frame_gen.GenerateInto(*impl->device, cmd, outputs[i].image, outputs[i].view, i);
    }
    return std::uint32_t(count);
}

const char* Runner::Problem() const {
    return impl->frame_gen->IsUnavailable() ? impl->frame_gen->UnavailableReason() : nullptr;
}

} // namespace Vulkan::Lsfg
