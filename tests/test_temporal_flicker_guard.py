#!/usr/bin/env python3
"""Review-stage regression gates for source-format and current-frame temporal fallback.

These are source-contract checks. They are NOT Vulkan-on-device validation:
framebuffer flickering still requires log and video A/B from the phone.
"""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/
   "gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp").read_text()
assert 'UnchartedLdrViewFormat(vk::Format format)' in s
assert 'case vk::Format::eR8G8B8A8Srgb:' in s
assert 'return vk::Format::eR8G8B8A8Unorm;' in s
assert 'case vk::Format::eB8G8R8A8Srgb:' in s
assert 'return vk::Format::eB8G8R8A8Unorm;' in s
assert 'BB_UNCHARTED_SRGB_VIEW' in s # opt-out in case of driver regressions
assert 'provider != BbSettings::UpscalerFsr3' in s
assert 'provider != BbSettings::UpscalerGsr2' in s
assert 'requested == BbSettings::UpscalerTaa' not in s[s.index('void TemporalUpscaler::BeforeDisplay('):s.index('void TemporalUpscaler::OnColorTarget(')]
assert 'CachedView(color, UnchartedLdrViewFormat(color.info.pixel_format),' in s
assert 'info.color = Describe(color_image, UnchartedLdrViewFormat(color.info.pixel_format),' in s
assert 'EnsureUiResources(ow, oh, UnchartedLdrViewFormat(color.info.pixel_format),' in s
assert 'EnsureUiResources(target_width, target_height,' in s
start=s.index('void TemporalUpscaler::BeforeDisplay(')
end=s.index('void TemporalUpscaler::OnColorTarget(',start)
before=s[start:end]
assert 'temporal_selected' in before
assert 'camera_motion.Ready()' in before
assert 'MovieFsrBypass()' in before
assert 'if (ready) RunScaled();' in before
assert 'if (!ui_phase)' in before
assert 'RunUiOnly(source, camera_motion.Depth(), true);' in before
assert 'scene_fallback && scaled_session && camera_motion.VelocityMode()' in s
assert 'reset = true;' in before
assert 'UpscalerGsr2' in before and 'UpscalerFsr3' in before
print('PASS: LDR view normalization and conservative current-frame fallback wired for FSR3/GSR2')
