#!/usr/bin/env python3
"""GSR2 reset/edge and Uncharted fallback correctness checks.

A successful APK build alone cannot reproduce flickering on the GPU; these
checks target specific undefined Vulkan/GLSL operations uncovered by a
2026-11-09 Uncharted 1 log (GSR2 640x360 -> 1280x720).
"""
from pathlib import Path
import math

root=Path(__file__).resolve().parents[1]
shader=(root/"gpu/shadps4/video_core/host_shaders/gsr2_upscale.comp").read_text()
cpp=(root/"gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp").read_text()
header=(root/"gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.h").read_text()

# GSR histories were discarded with oldLayout = Undefined on reset. The shader
# must not sample them in reset frames, or Inf/NaN can propagate through mix().
assert 'const bool' not in shader[shader.index('vec4 History = vec4(0.0);'):shader.index('if (params.reset == 0u)')]
assert 'vec4 History = vec4(0.0);' in shader
assert 'vec3 HistoryColor = vec3(0.0);' in shader
start=shader.index('if (params.reset == 0u) {')
end=shader.index('}',start)
history_block=shader[start:end]
assert 'textureLod(PrevHistoryOutput, PrevUV, 0.0)' in history_block
assert 'SampleHistoryCatmullRom(PrevUV)' in history_block
assert shader.index('vec4 History = vec4(0.0);') < start
assert 'vec4 History = textureLod(PrevHistoryOutput' not in shader

# GLSL texelFetch outside an image is undefined; clamp edges even at UV 1.0.
assert 'ivec2 InputPos = clamp(ivec2(Jitteruv * InputInfoViewportSize)' in shader
assert 'min(InputPosBtmRight,maxInputPos)' in shader
assert 'texelFetch(YCoCgColor, InputPosBtmRight, 0)' not in shader

# Lanczos/box weights can approach 0 on a valid scene edge.
assert '1.0 / max(rectboxweight, 1.0e-6)' in shader
assert 'abs(totalWeight) > 1.0e-6' in shader
assert 'DecodeColor(topleft.y)' in shader
assert 'max(totalWeight,0.0)' in shader

# The fallback must show exactly the patched scene extent, not undefined
# padding inside a 1920x1080 guest allocation. UI/cutscenes remain full-size.
assert 'bool scene_fallback = false' in header
assert 'RunUiOnly(source, camera_motion.Depth(), true)' in cpp
assert 'if (scene_fallback && scaled_session && camera_motion.VelocityMode())' in cpp
assert 'source_width = visible[0];' in cpp
assert 'source_height = visible[1];' in cpp
assert 'RunUiOnly(color, depth);' in cpp
assert 'if (seen_frames % 256 == 0)' in cpp

for w,h in [(640,360),(960,540),(1280,720)]:
    for x,y in [(-1,-1),(0,0),(w-1,h-1),(w,h),(w+1,h+1)]:
        p=(max(0,min(x,w-1)),max(0,min(y,h-1)))
        neighbour=(min(p[0]+1,w-1),min(p[1]+1,h-1))
        assert 0<=p[0]<w and 0<=p[1]<h
        assert 0<=neighbour[0]<w and 0<=neighbour[1]<h

# Old (undefined) history can contain NaN. The reset branch must start
# finite; otherwise even mix(old, current, 1.0) can propagate NaN.
history=float('nan')
current=(.1,.3,.7)
assert math.isnan(0.0*history)
reset_history=(0.0,0.0,0.0)
mixed=tuple(0.0*a+1.0*b for a,b in zip(reset_history,current))
assert all(math.isfinite(z) for z in mixed)
print('PASS: GSR2 undefined-history reset, edge texel fetch, safe Lanczos weights and 640x360 fallback crop')
