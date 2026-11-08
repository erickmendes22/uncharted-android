#!/usr/bin/env python3
"""Regression guard: the GoW3-style Android overlay must not steal SDL gamepads.
Also pin carefully selected upstream 0.3.0 / 0.3.14 hardening changes.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
def read(p): return (root/p).read_text()

activity=read("android/scripts/g2/game_app/GameActivity.java")
runtime=read("src/runtime_pad.c")
pad=read("android/scripts/g2/game_app/PadBridge.java")
touch=read("android/scripts/g2/game_app/TouchControllerOverlay.java")
presenter=read("gpu/shadps4/video_core/renderer_vulkan/vk_presenter.cpp")
shader=read("gpu/shadps4/video_core/host_shaders/fsr.comp")
swapchain=read("gpu/shadps4/video_core/renderer_vulkan/vk_swapchain.cpp")
sampler=read("gpu/shadps4/video_core/texture_cache/sampler.cpp")
rasterizer=read("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp")
launcher=read("android/scripts/g2/game_app/Launcher.java")

assert 'Os.setenv("BB_PAD_FILE", d + "/touch-pad-state.txt", true);' in activity
assert 'Os.setenv("BB_PAD_HOST", "1", true);' in activity
assert 'Os.setenv("BB_JAVA_TOUCH", "1", true);' in activity
assert 'bb_pad_scripted_only(file,host,java_touch)' in runtime
assert '#include "runtime_pad_host_policy.h"' in runtime
assert 'SDL_GetGamepads(&count)' in runtime and 'SDL_OpenGamepad(ids[0])' in runtime
assert 'SDL_GetGamepadButton(g,' in runtime and 'SDL_GetGamepadAxis(g,' in runtime
assert 'sample_host(d);' in runtime and 'read_inject();' in runtime
assert 'merge_touch_pad(d,axes)' in runtime
assert 'if (leftActive)' in pad and 'if (rightActive)' in pad
assert 'pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)' in touch
assert 'x<=40f && hit(x,y)==null' in touch
# Android 0.3.0 orientation on portrait-native phones.
assert 'void setOrientationBis(int width, int height, boolean resizable, String hint)' in activity
assert 'SCREEN_ORIENTATION_SENSOR_LANDSCAPE' in activity
# PC 0.3.14: correct compute/fragment presentation barrier and FSR edges.
assert 'vk::PipelineStageFlagBits2::eFragmentShader |' in presenter
assert 'vk::PipelineStageFlagBits2::eComputeShader' in presenter
assert 'FsrRcasLoadH' in shader and 'clamp(ASU2(p)' in shader
assert 'image_index = frame_index = 0;' in swapchain
assert '.maxLod = std::max(sampler.MinLod(), sampler.MaxLod())' in sampler
assert 'BB_DEPTH_TARGET_STATE' in rasterizer
assert 'vk::AccessFlagBits2::eDepthStencilAttachmentWrite' in rasterizer
# Preserve established U2 movie bypass.
assert 'BB_U2_MOVIE_FSR_BYPASS=1' in launcher
print("PASS: external SDL gamepad coexists with Java touch; selected upstream fixes and U2 FSR bypass retained")
