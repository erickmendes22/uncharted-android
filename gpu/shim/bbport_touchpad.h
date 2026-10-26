// SPDX-License-Identifier: GPL-2.0-or-later
// g2-arm64: an on-screen gamepad for phones (Android). Fingers on the screen become a DualShock:
// a floating left stick on the left half, the camera by dragging on the right half (the right
// stick), the face buttons, the shoulders and triggers, the D-pad, L3/R3, Options and the touch
// pad button as translucent controls drawn by ImGui over the game. The state goes to the pad
// sampler through runtime_touch_pad (src/runtime_pad.c). Off, Auto (shown while no physical
// gamepad is connected) or On: BbSettings::touch_pad, bbport.ini "touch_pad", BB_TOUCH_PAD.
#pragma once

#include <SDL3/SDL_events.h>
#include "bbport_touch_layout.h"

namespace BbOverlay {

/// Whether the touch gamepad is drawn and samples fingers now (the setting and, in Auto, no
/// physical gamepad). The menu being open suspends it (TouchPadReset).
bool TouchPadActive();
/// A finger event while the menu is closed. Fingers that start in the menu's edge zone (the
/// last 3% of the width) are not taken. Returns true when the touch pad took the finger.
bool TouchPadEvent(const SDL_Event& event);
/// Every frame while active, inside an ImGui frame: draws the controls at the display size
/// `width` x `height` (logical pixels) and hands the pad state to the runtime.
void TouchPadFrame(float width, float height);
/// Releases every finger and sends a neutral state (the menu opened, the setting turned off).
void TouchPadReset();
/// Screen rectangle occupied by the corner FPS HUD (zero releases the reservation).
void TouchPadReserveHud(float left,float top,float right,float bottom);
BbTouchLayout::Rect TouchPadMenuRect(float width,float height);

} // namespace BbOverlay
