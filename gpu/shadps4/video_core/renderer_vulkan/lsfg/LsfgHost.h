// SPDX-License-Identifier: GPL-3.0-or-later
//
// Uncharted ARM port: what the frame generation code ported from Eden by way of ARMSX2 asked of
// ARMSX2's GSDeviceVK — the device, waiting for it to go idle, and which of the features the
// LSFG shaders need were enabled at device creation. The port's FrameGenerator fills one and
// makes it current while frame generation exists.

#pragma once

#include <functional>

#include <vulkan/vulkan.h>

namespace Vulkan {

class LsfgHost {
public:
    VkDevice device = VK_NULL_HANDLE;
    /// Features enabled on the logical device (not merely supported): declaring the Vulkan
    /// memory model in a shader or relying on null descriptors without them is invalid usage.
    bool vulkan_memory_model = false;
    bool null_descriptor = false;
    bool shader_float16 = false;
    /// Waits for the device to go idle under the renderer's queue lock (other threads submit).
    std::function<void()> wait_idle;

    [[nodiscard]] VkDevice GetDevice() const {
        return device;
    }
    /// Teardown waits for everything submitted (the passes destroy their objects at once).
    void WaitForGPUIdle() const {
        if (wait_idle) {
            wait_idle();
        } else if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    /// The host of the frame generation that exists (ARMSX2's GSDeviceVK::GetInstance()), or null.
    static LsfgHost* GetInstance() {
        return current;
    }
    static void SetInstance(LsfgHost* host) {
        current = host;
    }

private:
    static inline LsfgHost* current = nullptr;
};

} // namespace Vulkan
