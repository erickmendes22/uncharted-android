// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace BbMeshCopy {
constexpr bool NeedsGuestResult(unsigned group_size,bool enabled,bool cpu_destination) {
    return enabled && group_size==64 && cpu_destination;
}
}
