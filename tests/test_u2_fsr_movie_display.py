#!/usr/bin/env python3
"""Uncharted 2 movie/FSR regression: never hand stale FSR display to presenter."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def source(path): return (root / path).read_text()
u=source("gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp")
h=source("gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.h")
driver=source("gpu/shadps4/core/libraries/videoout/driver.cpp")
launcher=source("android/scripts/g2/game_app/Launcher.java")
menu=source("android/scripts/g2/game_app/TouchControllerOverlay.java")
assert "bool MovieRecentlyDecoded()" in driver
assert "MovieRecentlyDecoded()" in u
assert 'std::getenv("BB_U2_MOVIE_FSR_BYPASS")' in u
assert "BbSettings::UpscalerFsr3" in u
assert "BbSettings::IsFsr4(provider)" in u
assert 'env[i] += "BB_U2_MOVIE_FSR_BYPASS=1\\n"' in launcher
assert 'env[i] += "-BB_VDEC_DIAG\\n-BB_U2_MOVIE_FSR_BYPASS\\n"' in launcher
# No movie should redirect into the previous scene's FSR display image.
assert "display_redirect = ui_phase && !movie_bypass && !changed;" in u
assert "return !MovieFsrBypass() && enabled && !failed" in u
assert "if (MovieFsrBypass()) {" in u
assert "ui_phase = display_redirect = false;" in u
assert "if (it != displays.end()) it->second.valid = false;" in u
assert "if (!display_redirect || image != ui_color || MovieFsrBypass())" in u
assert "return display_redirect && image == ui_color && !MovieFsrBypass();" in h
assert "if (!ui_phase || depth != ui_depth || MovieFsrBypass())" in u
# Preserve good Android pad and left-edge-only menu.
assert "pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)" in menu
assert "x<=40f && hit(x,y)==null" in menu
assert 'c.drawText("MENU"' not in menu
# Full-size movie fix from previous turn stays in place.
assert '"-BB_VDEC_CONTENT\\n-BB_VDEC_EXPLICIT_CONTENT\\n-BB_VDEC_SIZE\\n"' in launcher
print("PASS: FSR host-display bypass is U2+movie-only; stale host image invalidated; native pad/menu and full-size video kept")
