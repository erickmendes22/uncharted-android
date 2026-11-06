// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

// PS4 CE WaitOnDeCounterDiff: never subtract unsigned counters when the
// count engine is behind the draw engine. Such underflow can wait forever.
namespace BbCeWait {
constexpr bool ShouldYield(std::uint64_t ce_count, std::uint64_t de_count,
                           std::uint64_t difference) noexcept {
    return ce_count >= de_count && ce_count - de_count >= difference;
}
}
