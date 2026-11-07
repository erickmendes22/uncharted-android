#!/usr/bin/env python3
"""Guard the working native PS controller while removing duplicate menu buttons
and reverting Uncharted 2's unintended 1280x720-in-1920x1080 movie resize.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def source(p):return (root/p).read_text()
view=source("android/scripts/g2/game_app/TouchControllerOverlay.java")
menu=source("gpu/shim/bbport_overlay_android.cpp")
launcher=source("android/scripts/g2/game_app/Launcher.java")
video=source("gpu/shadps4/core/libraries/videodec/videodec2_impl.cpp")
copy=source("gpu/shadps4/core/libraries/videodec/video_utils.cpp")
pad=source("android/scripts/g2/game_app/PadBridge.java")

# The original working GoW3/Bloodborne touch path remains present.
assert "getPointerId(at)" in view and "SparseArray<String> controls" in view
assert "pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)" in view
assert "renameTo(state)" in pad
# Both duplicated menu affordances must be absent in Java native mode.
assert 'c.drawText("MENU"' not in view
assert 'if (x>550 && x<730 && y>0 && y<69)' not in view
assert 'GameActivity.nativeToggleMenu()' in view
assert "menuSwipePointer=id;" in view
assert "x<=40f && hit(x,y)==null" in view and "x-menuSwipeStartX>=140f" in view
assert "Math.abs(y-menuSwipeStartY)<=65f" in view
assert "if (id==menuSwipePointer) menuSwipePointer=-1" in view
assert "if (const char* java_touch = std::getenv(\"BB_JAVA_TOUCH\");" in menu
assert "java_touch && java_touch[0] == '1') return;" in menu
assert "java_touch && java_touch[0] == '1' && !open) return TouchAction::None;" in menu

# U2 alone must restore the natural full-frame video regardless of 360p
# scene rendering or the 1280x720 output of FSR. The guest movie frame
# metadata is still 1920x1080 and its chroma offset is based on 1088 rows.
assert 'env[i] = g[0].equals("u2")' in launcher
assert '"-BB_VDEC_CONTENT\\n-BB_VDEC_EXPLICIT_CONTENT\\n-BB_VDEC_SIZE\\n"' in launcher
assert 'if (g[0].equals("u2")) {' in launcher
assert 'env[i] += "BB_VDEC_DIAG=1\\n"' in launcher
assert 'env[i] += "-BB_VDEC_DIAG\\n-BB_U2_MOVIE_FSR_BYPASS\\n"' in launcher
assert "SampleDecodedMovie(*frame);" in video
assert "n != 1 && n % 300 != 0" in video
# Non-tightly packed FFmpeg planes cannot use bulk memcpy. This was a real
# NV12 row-stride bug and also preserves the fast path for tight 1080p.
assert "src.linesize[0] != src.width" in copy
assert "src.linesize[1] != src.width" in copy
print("PASS: native PS pad preserved; left-edge swipe only; two menu buttons hidden; U2 full-frame movies and NV12 stride guarded")
