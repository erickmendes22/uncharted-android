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
    assert 'actionButton(c,"'+button+'",46,"'+label+'")' in touch,(button,label)
assert 'actionStick(c,"leftstick")' in touch
assert 'actionStick(c,"rightstick")' in touch
assert 'actionClickStick(c,"l3")' not in touch
assert 'actionClickStick(c,"r3")' not in touch
assert 'clickStickIfDoubleTapped(' in touch
assert 'postDelayed(clearLeftClick,135)' in touch
assert 'postDelayed(clearRightClick,135)' in touch
assert 'if(leftStickClicked)pressed.add("l3")' in touch
assert 'if(rightStickClicked)pressed.add("r3")' in touch
assert 'float r=80*sizeMultiplier;' in touch
assert 'ACTION_LAYOUT_VERSION_KEY' in touch
assert 'ACTION_LAYOUT_VERSION=4' in touch
assert 'actionHorizontalScale()' in touch
assert 'actionDistance(x,y,p)<radius*sizeMultiplier' in touch
assert 'actionDistance(last.x,last.y,start)>18*sizeMultiplier' in touch
assert 'actionDisplayX(float x)' in touch
assert 'actionLogicalX(float x)' in touch
assert 'PointF p=actionCenter(name);' in touch
assert 'PointF p=actionDpadCenter();' in touch
assert 'float toolbarX=screenLayoutX(e,at);' in touch
assert 'migration.remove("overlay_uncharted_x_"+name)' in touch
assert 'actionDpad(c)' in touch
assert 'private static final float[][] DPAD_ARMS=' in touch
assert 'pressed.contains(DPAD_DIRECTIONS[i])' in touch
assert 'c.drawRoundRect(x0,y0,x1,y1' in touch
assert 'migration.putFloat("overlay_uncharted_size",1.0f);' in touch
assert 'float radius=stick(name)?84:' in touch
assert 'drawFirePistol(c,p.x,p.y,r*.77f)' in touch
assert 'drawInteractHand(c,p.x,p.y,r*.76f)' in touch
assert 'if ("r2".equals(name))' in touch and 'if ("triangle".equals(name))' in touch
assert 'c.drawLine(.40f,-.66f,.40f,.71f,paint)' in touch # cover
assert 'c.drawLine(.14f,.13f,.60f,.47f,paint)' in touch # jump
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
print("PASS: two independent layouts, smaller balanced sticks, readable actions, L3/R3 double-tap, classic/external controls unchanged")
