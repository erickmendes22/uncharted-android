// SPDX-License-Identifier: GPL-2.0-or-later

// g2-arm64: the presented frame drawn into a pre-rotated swapchain image (Android pre-rotation:
// the swapchain takes the display's transform), so the compositor shows it as it is instead of
// rotating it on the GPU. `uv` covers the frame's rectangle in the swapchain image.

#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D frame_image;

layout (push_constant) uniform params {
    uint quarter_turns; // clockwise rotation of the frame (the surface's pre-transform)
} pc;

void main() {
    vec2 src = uv;
    if (pc.quarter_turns == 1u) {
        src = vec2(uv.y, 1.0 - uv.x);
    } else if (pc.quarter_turns == 2u) {
        src = vec2(1.0 - uv.x, 1.0 - uv.y);
    } else if (pc.quarter_turns == 3u) {
        src = vec2(1.0 - uv.y, uv.x);
    }
    color = vec4(texture(frame_image, src).rgb, 1.0);
}
