// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace BbDriverCache {
// VkPipelineCacheHeaderVersionOne, checked before any bytes reach the driver.
inline bool Matches(std::span<const char> data, std::uint32_t vendor, std::uint32_t device,
                    std::span<const std::uint8_t, 16> uuid) {
    constexpr std::size_t size = 32;
    if (data.size() < size) return false;
    std::array<std::uint32_t, 4> h{};
    std::memcpy(h.data(), data.data(), sizeof(h));
    return h[0] >= size && h[0] <= data.size() && h[1] == 1 &&
           h[2] == vendor && h[3] == device &&
           std::memcmp(data.data() + 16, uuid.data(), uuid.size()) == 0;
}
}
