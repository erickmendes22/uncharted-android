#!/usr/bin/env python3
"""0.3.0 Android UX integration: real per-game profiles and restart-safe patches."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
def src(path): return (root/path).read_text()
launcher=src("android/scripts/g2/game_app/Launcher.java")
activity=src("android/scripts/g2/game_app/GameActivity.java")
menu=src("gpu/shim/bbport_overlay_android.cpp")
hud=src("gpu/shim/bbport_overlay.cpp")
touch=src("android/scripts/g2/game_app/TouchControllerOverlay.java")
driver=src("android/scripts/fetch_gen8_driver.sh")
flow=src(".github/workflows/android.yml")

assert src("android/VERSION").strip()=="0.3.0"
assert 'String version="0.3.0"' in launcher
assert "VERSION_CODE=$((100000 + VERSION_CODE))" not in flow
assert "versionName='0.3.0'" in flow and "versionCode='100300'" in flow
assert "Uncharted-Daedalus-0.3.0.apk" in flow
assert 'gameCard(gallery,g[0])' in launcher
assert 'gameCardAction(actions,"Play",true' in launcher
assert 'gameCardAction(actions,"Configure",false' not in launcher
assert 'gallery.setOrientation(portraitLayout()?LinearLayout.VERTICAL:LinearLayout.HORIZONTAL)' in launcher
assert 'categoryScroll.setHorizontalScrollBarEnabled(false)' in launcher
assert 'container.setOnClickListener(v->chooseGame(game,0))' in launcher
assert "private void saveGameProfile(String game)" in launcher
assert "private void loadGameProfile(String game)" in launcher
assert 'if(key.startsWith("patch_")) return true;' in launcher
assert 'prefs.edit().putString("selected_game",g[0]).commit();' in launcher
assert 'String[] scene=UpscalePreset.input(upscale,chosenResolution()).split("x");' in launcher
assert '"patches-state-"+g[0]+".txt"' in launcher
assert '"bbport-"+game+".ini"' in launcher
assert 'Os.setenv("BB_CONFIG", settings.getAbsolutePath(), true)' in activity
assert 'Os.setenv("BB_GAME_ID",game,true)' in activity
assert 'Os.setenv("BB_GAME_CONFIG_REQUEST",d+"/game-config-request.txt",true)' in activity
assert 'getBoolean("game_"+game+"_gpu_small_readbacks"' in activity
assert 'autoPlayConfigRequest' in launcher and 'applyGameRequest()' in launcher
assert 'apply_game_request' in activity and 'apply_game_request' in launcher
assert 'File d=getExternalFilesDir(null)' in activity
assert 'final String[] categories={"Video","Performance","Controls","Drivers","Games / Files","System"}' in launcher

assert 'constexpr float PanelShare = 0.53f' in menu
assert 'ImGui::SetNextWindowPos(viewport->WorkPos)' in menu
assert 'Video","Performance","Patches","HUD","Controls","System' in menu
assert 'const char* providers[]={"Off","FSR 3.1","GSR 2"}' in menu
assert 'UpscalerTaa' not in menu
assert 'void LoadDraft()' in menu and 'bool RestartWithDraft(bool apply_draft)' in menu
assert 'runtime_restart(); // Restart helper opens Launcher' in menu
assert '"BB_PATCH_STATE"' in menu
assert 'MobilePatchCount' in menu
assert 'BB_GAME_ID' in menu
assert 'std::fprintf(file,"upscale=%d' in menu
assert 'if (!open && event.tfinger.x <= EdgeZone)' in menu
assert 'down_outside = open && event.tfinger.x > panel_left_norm.load()' in menu
assert 'x<=Math.min(getWidth()*.10f' in touch
assert 'getWidth()*.065f' in touch
assert '"GSR 2 "' in hud
assert '"TAA",normal});' not in hud
assert 'Turnip_Gen8_V39_Patched.zip' in driver and 'Turnip_v27.0.0-R2.zip' in driver
assert 'bd457b30f195cf93a334b5302c221b1b5cb59c79c485935decf95d560a38c8a2' in driver
assert '8d30be022582d0c03b1918c019540491d00439e14e9e5e1359fe0bb19004f781' in driver
assert '"gpu_small_readbacks",false' in launcher
assert '"BB_COPY64_EOP_READBACK","u3".equals(game)?"1":"0"' in activity
assert '"BB_EFFECT_LOOP_CLAMP","1"' in activity
print("PASS: 0.3.0 selection, responsive immersive UI, patches and HUD")
