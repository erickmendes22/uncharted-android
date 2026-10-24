// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan::HostPasses {

/// g2-arm64: draws the presented frame into a swapchain image rotated by the surface's
/// pre-transform (Android pre-rotation), with black around the frame's rectangle.
class PresentRotatePass {
public:
    void Create(vk::Device device, vk::Format swapchain_format);

    [[nodiscard]] bool Ready() const {
        return static_cast<bool>(pipeline);
    }

    /// `input` in ShaderReadOnlyOptimal, `target` in ColorAttachmentOptimal; `rect` is where the
    /// frame lands in the target (already rotated); `quarter_turns` clockwise.
    void Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::ImageView target,
                vk::Extent2D target_size, vk::Rect2D rect, u32 quarter_turns);

private:
    vk::UniquePipeline pipeline{};
    vk::UniquePipelineLayout pipeline_layout{};
    vk::UniqueDescriptorSetLayout desc_set_layout{};
    vk::UniqueSampler sampler{};
};

} // namespace Vulkan::HostPasses
