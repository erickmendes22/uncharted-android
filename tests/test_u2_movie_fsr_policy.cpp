// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_u2_movie_fsr_policy.h"
#include <cassert>
using BbU2MovieFsr::ShouldUseGuestDisplay;
int main() {
    // Only Uncharted 2 prerecorded movies while FSR is active use the original
    // guest video surface. Regular gameplay, TAA/disabled, and U1/U3 unchanged.
    assert( ShouldUseGuestDisplay(true, true, true));
    assert(!ShouldUseGuestDisplay(true, true, false));
    assert(!ShouldUseGuestDisplay(true, false, true));
    assert(!ShouldUseGuestDisplay(false, true, true));
    assert(!ShouldUseGuestDisplay(false, false, false));
}
