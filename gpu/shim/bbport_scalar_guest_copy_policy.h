// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

// U3: the one-dword copy shader sometimes moves a GPU-produced animation constant
// into a CPU-written block that the guest's SRT walker reads. The GPU copy alone
// is not enough; guest memory must receive the completed source in command order.
// Scope the forced readback to small CPU-owned destinations, not every GPU copy.
namespace BbScalarGuestCopy {
constexpr bool NeedsOrderedReadback(bool enabled, unsigned group_size,
                                    bool gpu_written, bool cpu_destination,
                                    std::uint64_t bytes) noexcept {
    return enabled && group_size == 1 && gpu_written && cpu_destination &&
           bytes >= 4 && bytes <= 4096 && (bytes & 3) == 0;
}
} // namespace BbScalarGuestCopy
