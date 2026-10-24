// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/renderer_vulkan/host_passes/present_rotate_pass.h"

#include "common/assert.h"
#include "video_core/host_shaders/fs_tri_vert.h"
#include "video_core/host_shaders/present_rotate_frag.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan::HostPasses {

void PresentRotatePass::Create(vk::Device device, vk::Format swapchain_format) {
    const vk::DescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };
    desc_set_layout = Check<"create present rotate descriptor set layout">(
        device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = 1,
            .pBindings = &binding,
        }));
    const vk::PushConstantRange push_constants{
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(u32),
    };
    pipeline_layout = Check<"create present rotate pipeline layout">(
        device.createPipelineLayoutUnique({
            .setLayoutCount = 1,
            .pSetLayouts = &*desc_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constants,
        }));

    const auto vs_module = CompileSPV(FS_TRI_VERT, device);
    const auto fs_module = CompileSPV(PRESENT_ROTATE_FRAG, device);
    ASSERT(vs_module && fs_module);
    const std::array stages{
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vs_module,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fs_module,
            .pName = "main",
        },
    };
    const vk::PipelineRenderingCreateInfo rendering_ci{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain_format,
    };
    const vk::PipelineVertexInputStateCreateInfo vertex_input{};
    const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
    };
    const vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const vk::PipelineRasterizationStateCreateInfo raster{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .lineWidth = 1.0f,
    };
    const vk::PipelineMultisampleStateCreateInfo multisample{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
    };
    const vk::PipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineColorBlendStateCreateInfo blend{
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    const std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamic{
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
    pipeline = Check<"create present rotate pipeline">(device.createGraphicsPipelineUnique(
        {}, vk::GraphicsPipelineCreateInfo{
                .pNext = &rendering_ci,
                .stageCount = static_cast<u32>(stages.size()),
                .pStages = stages.data(),
                .pVertexInputState = &vertex_input,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &raster,
                .pMultisampleState = &multisample,
                .pColorBlendState = &blend,
                .pDynamicState = &dynamic,
                .layout = *pipeline_layout,
            }));
    device.destroyShaderModule(vs_module);
    device.destroyShaderModule(fs_module);

    sampler = Check<"create present rotate sampler">(device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    }));
}

void PresentRotatePass::Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::ImageView target,
                               vk::Extent2D target_size, vk::Rect2D rect, u32 quarter_turns) {
    const vk::RenderingAttachmentInfo attachment{
        .imageView = target,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}},
    };
    const vk::DescriptorImageInfo image_info{
        .sampler = *sampler,
        .imageView = input,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    const vk::WriteDescriptorSet write{
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &image_info,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, target_size},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    });
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    cmdbuf.setViewport(0, vk::Viewport{
                              .x = static_cast<float>(rect.offset.x),
                              .y = static_cast<float>(rect.offset.y),
                              .width = static_cast<float>(rect.extent.width),
                              .height = static_cast<float>(rect.extent.height),
                              .minDepth = 0.0f,
                              .maxDepth = 1.0f,
                          });
    cmdbuf.setScissor(0, rect);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, write);
    cmdbuf.pushConstants(*pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(u32),
                         &quarter_turns);
    cmdbuf.draw(3, 1, 0, 0);
    cmdbuf.endRendering();
}

} // namespace Vulkan::HostPasses
