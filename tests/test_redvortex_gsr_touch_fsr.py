#!/usr/bin/env python3
"""Daedalus base + RedVorteX GSR2 and flicker-safe FSR3.1 controller regressions."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def f(p):return (root/p).read_text()
launcher=f("android/scripts/g2/game_app/Launcher.java")
game=f("android/scripts/g2/game_app/GameActivity.java")
touch=f("android/scripts/g2/game_app/TouchControllerOverlay.java")
preset=f("android/scripts/g2/game_app/UpscalePreset.java")
raster=f("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp")
temporal=f("gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp")
header=f("gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.h")
settings=f("gpu/shim/bbport_settings.cpp")
cmake=f("gpu/CMakeLists.txt")
overlay=f("gpu/shim/bbport_overlay_android.cpp")
assert 'Async GPU label writes' not in launcher
assert 'BB_CHAIN_WRITEDATA' not in game
assert 'BB_CHAIN_WRITEDATA' not in f("gpu/shadps4/video_core/amdgpu/liverpool.cpp")
assert "patch_job_sleep(" not in f("src/probe.c")
assert 'BB_SKIP_COLLECTION_MENU' in game and 'patch_collection_skip(image_end)' in f('src/probe.c')
assert 'w.write("/app0/eboot.bin' not in game
assert 'Collection menu: not found, the menu stays' in f('src/probe.c')
assert "touchEditRequest.exists()" in game
assert "BB_TOUCH_EDIT_REQUEST" in overlay and "keep_open=false" in overlay
assert 'putFloat(sizeKey()' in touch
assert 'putString("overlay_style"' in touch
assert "actionLabel(" in touch and "drawActionGlyph(" in touch and "isEditing()" in touch
assert 'ACTION_DEFAULT' in touch and 'DEFAULT' in touch
assert 'TouchControllerOverlay.UNCHARTED_STYLE' in launcher
assert 'TouchControllerOverlay.CLASSIC_STYLE' in launcher
assert 'BB_TOUCH_STYLE_REQUEST' in game and 'BB_TOUCH_STYLE_STATE' in game
assert 'BB_TOUCH_STYLE_REQUEST' in overlay and 'BB_TOUCH_STYLE_STATE' in overlay
assert "✋" not in touch and "✊" not in touch and "⬡" not in touch
assert 'button(root,"Edit button position and size",v->editTouchLayout());' in launcher
assert "Editar posicao e tamanho dos botoes" not in launcher
assert 'option.setText(AppLanguage.text(choice[1]))' in launcher
assert "GSR 2 Native AA" in preset and 'return normalize(mode)>=5?"gsr2":"fsr3";' in preset
assert 'iniPut("upscaler",UpscalePreset.provider(upscale))' in launcher
assert "UpscalerGsr2" in header or "UseGsr2" in header
assert '"gsr2"' in settings and "UseGsr2()" in temporal and "RecordGsr2(" in temporal
assert 'gsr2_convert_comp' in cmake and 'gsr2_upscale_comp' in cmake
assert (root/"gpu/shadps4/video_core/host_shaders/gsr2_convert.comp").is_file()
assert (root/"gpu/shadps4/video_core/host_shaders/gsr2_upscale.comp").is_file()
assert "BbSettings::UpscalerGsr2" in overlay
assert "gsr_history" in temporal and "resources_gsr2" in header
assert "BB_U2_MOVIE_FSR_BYPASS" in temporal and "UpscalerGsr2" in temporal
assert "0xf614041eu" in raster and "0xb2a42acfu" in raster and "0x1a08df2du" in raster
assert "BB_JITTER_SMALL" in raster and "BB_JITTER_RECONSTRUCTION" in raster
assert "return env && env[0] == '1';" in raster
assert "disableNativeAa(" in launcher and "disableMotionBlur(" in launcher
print("PASS: original worker, U1 signature-skip, Canvas controls, GSR2 and last-visible FSR AA policy")
