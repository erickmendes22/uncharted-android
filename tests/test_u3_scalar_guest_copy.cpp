// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_scalar_guest_copy_policy.h"
#include <cassert>
#include <cstdint>
int main() {
    using BbScalarGuestCopy::NeedsOrderedReadback;
    assert(NeedsOrderedReadback(true, 1, true, true, 4));
    assert(NeedsOrderedReadback(true, 1, true, true, 1024));
    assert(NeedsOrderedReadback(true, 1, true, true, 4096));
    assert(!NeedsOrderedReadback(false, 1, true, true, 1024));
    assert(!NeedsOrderedReadback(true, 64, true, true, 1024)); // preserve U2 path
    assert(!NeedsOrderedReadback(true, 1, false, true, 1024));
    assert(!NeedsOrderedReadback(true, 1, true, false, 1024));
    assert(!NeedsOrderedReadback(true, 1, true, true, 0));
    assert(!NeedsOrderedReadback(true, 1, true, true, 4097));
    assert(!NeedsOrderedReadback(true, 1, true, true, 8192));
    assert(!NeedsOrderedReadback(true, 1, true, true, UINT64_MAX));
    return 0;
}
