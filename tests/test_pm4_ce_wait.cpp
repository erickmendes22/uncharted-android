// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_ce_wait_policy.h"
#include <cassert>
#include <cstdint>
int main() {
    using BbCeWait::ShouldYield;
    // Regression: unsigned (de - ce) used to underflow and suspend forever.
    assert(!ShouldYield(0,1,1));
    assert(!ShouldYield(1,5,1));
    assert(ShouldYield(5,1,2));
    assert(ShouldYield(5,5,0));
    assert(!ShouldYield(5,5,1));
    assert(!ShouldYield(5,3,4));
    assert(!ShouldYield(UINT64_MAX-1,UINT64_MAX,1));
    assert(ShouldYield(UINT64_MAX,UINT64_MAX-1,1));
    return 0;
}
