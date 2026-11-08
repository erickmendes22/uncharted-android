#!/usr/bin/env python3
"""Confirm the approved GoW3 Uncharted action pad is OPTIONAL.

Never compile different input mappings: classic and Uncharted touch styles
must share the same pointer-ID-to-OrbisPad bridge and preserve external pads.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def read(p): return (root/p).read_text()
touch=read("android/scripts/g2/game_app/TouchControllerOverlay.java")
launcher=read("android/scripts/g2/game_app/Launcher.java")
game=read("android/scripts/g2/game_app/GameActivity.java")
menu=read("gpu/shim/bbport_overlay_android.cpp")
assert 'CLASSIC_STYLE="classic"' in touch
assert 'UNCHARTED_STYLE="uncharted"' in touch
assert 'getString("overlay_style"' in touch
assert 'putString("overlay_style",style)' in touch
assert 'void toggleStyle()' in touch and 'void syncStyle()' in touch
assert 'private static final float[][] DEFAULT=' in touch
assert 'private static final float[][] ACTION_DEFAULT=' in touch
assert 'unchartedStyle?ACTION_DEFAULT:DEFAULT' in touch
assert 'positionPrefix()' in touch
assert 'overlay_uncharted_' in touch and '"overlay_"' in touch
assert 'sizeKey()' in touch and 'opacityKey()' in touch
assert 'paint.setColor(actionFill(' in touch and 'paint.setColor(name.equals(dragged)?0xffe8dfc9:classicFill' in touch
assert 'drawActionGlyph(c,name,p.x,p.y,r*.67f)' in touch
for button,label in [('l1','L1'),('l2','L2'),('r1','R1'),('r2','R2'),
                     ('triangle','△'),('square','□'),('circle','○'),('cross','×')]:
    assert 'actionButton(c,"'+button+'",48,"'+label+'")' in touch,(button,label)
assert 'actionStick(c,"leftstick")' in touch
assert 'actionStick(c,"rightstick")' in touch
assert 'actionClickStick(c,"l3")' in touch
assert 'actionClickStick(c,"r3")' in touch
assert 'actionDpad(c)' in touch
assert 'L3 + R3' not in touch and 'camera' not in touch[touch.index('private void actionStick('):touch.index('private void actionClickStick(')]
assert 'circle(c,"triangle",39,"△"' in touch # original PS layout preserved
assert 'rectangle(c,"l2",108,"L2")' in touch
assert 'pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)' in touch
assert 'GameActivity.nativeToggleMenu()' in touch
assert 'touchStyles.setOnCheckedChangeListener' in launcher
assert 'TouchControllerOverlay.CLASSIC_STYLE' in launcher
assert 'TouchControllerOverlay.UNCHARTED_STYLE' in launcher
assert 'Editar posicao e tamanho dos botoes' in launcher
assert 'androidPadView.toggleStyle()' in game
assert 'androidPadView.syncStyle()' in game
assert 'BB_TOUCH_STYLE_REQUEST' in game and 'BB_TOUCH_STYLE_STATE' in game
assert 'BB_TOUCH_STYLE_REQUEST' in menu and 'BB_TOUCH_STYLE_STATE' in menu
assert 'Switch to Classic PlayStation' in menu
assert 'Switch to Uncharted action icons' in menu
assert 'BB_TOUCH_EDIT_REQUEST' in menu and 'BB_JAVA_TOUCH' in menu
print("PASS: two persistent independent layouts, real action glyphs, camera stick unchanged, in-game/launcher selector, physical pad bridge intact")
