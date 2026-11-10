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
assert 'new LinearLayout.LayoutParams(-1,dp(146))' in launcher
assert 'new LinearLayout.LayoutParams(0,dp(194),1f)' in launcher
assert 'gameCardAction(actions,"Play",true' in launcher
assert 'gameCardAction(actions,"Configure",false' not in launcher
assert 'item.setSingleLine(true)' in launcher
assert 'item.setEllipsize(android.text.TextUtils.TruncateAt.END)' in launcher
assert 'android.widget.HorizontalScrollView categoryScroll=' in launcher
assert 'tabLayout.width=portrait?dp(132):0' in launcher
assert 'tabLayout.weight=portrait?0f:1f' in launcher
assert 'The collection hero repeated the header' in launcher
assert 'nav.addView(item, new LinearLayout.LayoutParams(0, dp(portrait?42:38), 1))' in launcher
assert 'new LinearLayout.LayoutParams(-1,dp(72))' in launcher
assert 'poster.addView(name,new android.widget.FrameLayout.LayoutParams(' in launcher
assert 'selectedConfigSection=4;selectedPage=2;buildViews();refresh();' in launcher
assert 'container.setOnClickListener(v->chooseGame(game,0))' in launcher
assert 'enterImmersiveMode()' in launcher
assert 'View.SYSTEM_UI_FLAG_HIDE_NAVIGATION' in launcher
assert 'android.view.WindowInsets.Type.statusBars()' in launcher
assert 'BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE' in launcher
assert 'text(config,"Settings",portrait?17:15,true)' in launcher

assert 'private static final int ACTION_EDGE=0xffc0d1d9' in touch
assert 'private static final int ACTION_GLYPH=0xffedf4f4' in touch
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
