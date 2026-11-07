#!/usr/bin/env python3
"""Uncharted Android native View controller integration checks.
Inspired by working GoW3/Bloodborne TouchControllerOverlay + PadBridge;
the real APK build compiles the Java classes and JNI implementation.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def src(path):return (root/path).read_text()
java=src("android/scripts/g2/game_app/TouchControllerOverlay.java")
bridge=src("android/scripts/g2/game_app/PadBridge.java")
activity=src("android/scripts/g2/game_app/GameActivity.java")
runtime=src("src/runtime_pad.c")
old=src("gpu/shim/bbport_touchpad.cpp")
assert "extends View" in java and "MotionEvent.ACTION_POINTER_DOWN" in java
assert "MotionEvent.ACTION_POINTER_UP" in java and "getPointerId(" in java
assert "SparseArray<String> controls" in java and "positions.get(e.getPointerId(i))" in java
assert "layoutX(MotionEvent e,int index)" in java and "layoutY(MotionEvent e,int index)" in java
assert "GameActivity.nativeToggleMenu()" in java
assert "pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)" in java
assert "XInput" not in java
assert "renameTo(state)" in bridge and "FileOutputStream(temp)" in bridge
assert 'line.append("l2 ")' in bridge and 'line.append("r2 ")' in bridge
assert 'if (leftActive)' in bridge and 'if (rightActive)' in bridge
assert "Os.setenv(\"BB_PAD_FILE\"" in activity and "Os.setenv(\"BB_JAVA_TOUCH\"" in activity
assert "addContentView(androidPadView" in activity
assert "nativeOverlayCapturing()" in activity
assert "bbgpu_overlay_captures_input()" in runtime
assert "SDL_PushEvent(&e)" in runtime
assert 'getenv("BB_JAVA_TOUCH")' in old
# Avoid double injection: SDL's old virtual pad is disabled, but the physical
# SDL gamepad still samples normally, merged after any BB_PAD_FILE injection.
assert "read_inject();" in runtime
assert "if (injected.stick[i]>=0)" in runtime
assert "sample_host(d);" in runtime
assert "merge_touch_pad(d,axes)" in runtime
print("PASS: native Android View with pointer IDs, atomic pad file, no duplicate SDL touch, preserved physical gamepads and menu")
