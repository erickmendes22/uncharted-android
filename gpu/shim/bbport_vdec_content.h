// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdio>
#include <cstdint>

// Uncharted's resolution patches may change the scene size while the
// PS4 VideoOut framebuffer stays at 1920x1080. When the launcher explicitly
// requests movie content dimensions, those dimensions must win over the
// reported VideoOut framebuffer size. The NV12 frame layout itself remains
// the decoder's original width and pitch; only the picture content is scaled.
namespace BbVdecContent {
struct Size { int width=0, height=0; };
inline Size Parse(const char* env) {
    int w=0,h=0;
    if (!env || std::sscanf(env, "%dx%d", &w, &h)!=2 ||
        w < 64 || h < 64 || w > 7680 || h > 4320 ||
        (w & 1) || (h & 1)) return {};
    return {w,h};
}
inline Size Choose(Size requested, Size frame) {
    if (requested.width <= 0 || requested.height <= 0 ||
        requested.width > frame.width || requested.height > frame.height) return {};
    return requested;
}
}
