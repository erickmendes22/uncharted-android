// SPDX-License-Identifier: GPL-3.0-or-later
//
// Uncharted ARM port: Lossless Scaling frame generation (LSFG) for the presenter's frames, on the
// frame generation passes Eden wrote (eden-emu PR #4263) as ARMSX2 ported them (FrameGen.h). The
// shaders come from the player's own Lossless.dll; nothing of Lossless Scaling is bundled.
//
// Only Vulkan handle types cross this header, so the renderer (built with VK_NO_PROTOTYPES and
// vulkan.hpp) and the ported passes (plain Vulkan C calls) never share a translation unit.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

#include <vulkan/vulkan_core.h>

typedef struct VmaAllocator_T* VmaAllocator;

namespace Vulkan::Lsfg {

struct DeviceInfo {
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = nullptr;
    /// Enabled on the device: the LSFG shaders need the first two.
    bool vulkan_memory_model = false;
    bool null_descriptor = false;
    bool shader_float16 = false;
    /// Waits for everything submitted, synchronized with the threads that submit.
    std::function<void()> wait_idle;
};

struct Settings {
    std::string dll_path; ///< the player's Lossless.dll
    std::uint32_t multiplier = 2; ///< frames shown per game frame (2 to 4)
    std::uint32_t flow_scale = 100; ///< motion resolution in percent, 100: automatic
    bool fp16 = false; ///< the half-precision shaders, where the device and the DLL have them
};

struct Target {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE; ///< storage view
};

class Runner {
public:
    explicit Runner(const DeviceInfo& info);
    ~Runner();
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    /// Feeds `frame` (the frame about to be shown, General layout, `extent`, `format`) and
    /// records into `cmdbuf` (outside a render pass) the frames that go before it, into
    /// `outputs` in order (General layout, storage). `render_extent`: the game's own size, for
    /// the motion resolution. Returns how many were generated: none for the first frames, and
    /// none while unavailable (Problem()).
    std::uint32_t Generate(VkCommandBuffer cmdbuf, const Settings& settings, VkImage frame,
                           VkExtent2D extent, VkFormat format, VkExtent2D render_extent,
                           std::span<const Target> outputs);

    /// Why LSFG cannot run (no DLL, unusable shaders, device features), or null.
    [[nodiscard]] const char* Problem() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Vulkan::Lsfg
