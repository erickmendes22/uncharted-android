#!/usr/bin/env python3
"""Regression: transition from landscape launcher to SDL keeps fixed fullscreen bounds."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
g=(root/"android/scripts/g2/game_app/GameActivity.java").read_text()
l=(root/"android/scripts/g2/game_app/Launcher.java").read_text()
t=(root/"android/scripts/g2/game_app/TouchControllerOverlay.java").read_text()
for s in ["keepGameImmersive()", "w.setDecorFitsSystemWindows(false);",
          "void onWindowFocusChanged(boolean focused)",
          "void onConfigurationChanged(android.content.res.Configuration config)",
          "WindowInsets.Type.statusBars()", "WindowInsets.Type.navigationBars()"]:
    assert s in g,s
assert "protected void onCreate(Bundle savedInstanceState) {\n        keepGameImmersive();" in g
assert "super.onCreate(savedInstanceState);\n        keepGameImmersive();" in g
assert "keepGameImmersive();\n        super.onResume();\n        keepGameImmersive();" in g
assert "if(getRequestedOrientation()!=requested)setRequestedOrientation(requested);" in g
assert "getWindow().setDecorFitsSystemWindows(false)" in l
assert "view.setPadding(0,0,0,0);return insets;" in l
assert "enterImmersiveMode();\n        Intent i = new Intent(this, GameActivity.class);" in l
assert "ACTION_LAYOUT_VERSION=5" in t
assert "0xe006111b" in t
print("PASS immersive SDL initial framebuffer bounds and touch high-contrast migration")
