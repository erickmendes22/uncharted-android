// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// A prerecorded movie in Uncharted 2 is drawn through the guest video output,
// not the FSR 3 scene reconstruction target. During Videodec2 playback the
// previous FSR display override may remain "valid" but contain a stale or
// cleared UI image. Do not re-use it in place of the movie's guest framebuffer.
//
// Keep this gate tied to BOTH the selected game and FSR: other games, modes
// and regular gameplay continue through the existing upscaler.
namespace BbU2MovieFsr {
constexpr bool ShouldUseGuestDisplay(bool is_uncharted2, bool fsr_selected,
                                     bool decoder_active) noexcept {
    return is_uncharted2 && fsr_selected && decoder_active;
}
}
