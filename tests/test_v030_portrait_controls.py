#!/usr/bin/env python3
"""Regression checks for portrait navigation and Uncharted on-screen controls."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
launcher=(root/"android/scripts/g2/game_app/Launcher.java").read_text()
touch=(root/"android/scripts/g2/game_app/TouchControllerOverlay.java").read_text()
glyphs=(root/"android/scripts/g2/game_app/UnchartedActionGlyphs.java").read_text()
native=(root/"gpu/shim/bbport_overlay_android.cpp").read_text()
strings=json.loads((root/"android/localization/strings.json").read_text())

assert 'gallery.setOrientation(portraitLayout()?LinearLayout.VERTICAL:LinearLayout.HORIZONTAL);' in launcher
assert 'new LinearLayout.LayoutParams(-1,dp(182))' in launcher
assert 'new LinearLayout.LayoutParams(0,dp(292),1f)' in launcher
assert 'gameCardAction(actions,"Play",true' in launcher
assert 'gameCardAction(actions,"Configure",false' in launcher
assert 'item.setSingleLine(true)' in launcher
assert 'item.setEllipsize(android.text.TextUtils.TruncateAt.END)' in launcher
assert 'android.widget.HorizontalScrollView categoryScroll=' in launcher
assert 'tabLayout.width=dp(portraitLayout()?132:150)' in launcher
assert 'tabLayout.weight=0f' in launcher
assert 'home.addView(hero,new LinearLayout.LayoutParams(-1,dp(portraitLayout()?126:210)))' in launcher

assert 'private static final int ACTION_EDGE=0xff8d9da2' in touch
assert 'private static final int ACTION_GLYPH=0xffd0d8d6' in touch
assert '0xa04d6573' in touch
assert '0x77a14231' not in touch
assert '0xffcfaa70' not in touch
assert '0xfff2d59a' not in touch
assert '0xfff5d7a1' not in touch
assert 'int red=Math.round(226-46*y),green=Math.round(234-48*y)' in glyphs
assert 'actionDpad(c)' in touch
assert 'actionStick(c,"rightstick")' in touch
assert 'actionStick(c,"leftstick")' in touch

keys=("Android touch layout","Current: Uncharted action icons",
      "Current: Classic PlayStation","Switch to Classic PlayStation",
      "Switch to Uncharted action icons",
      "Edit touch layout (move, resize, icons)")
for key in keys:
    assert key in strings and strings[key]["pt"],key
assert 'Current: Uncharted (GoW3 action icons)' not in native
assert 'Current: Uncharted action icons' in native
print("PASS: responsive game cards, scrollable settings tabs, desaturated Uncharted controls and native locale keys")
