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
assert 'VERSION_CODE=$((120000 + VERSION_CODE))' in src("android/scripts/g2/game_app/build.sh")
assert "versionName='0.3.0'" in flow and "versionCode='120300'" in flow
assert "Uncharted-Daedalus-0.3.0.apk" in flow
assert 'gameCard(gallery,g[0])' in launcher
assert 'gameCardAction(actions,"Play",true' in launcher
assert 'gamePlayButtons.add(gameCardAction(actions,"Play",true' in launcher
assert 'v->playGame(game)' in launcher
assert 'SelectedGameArgs.isSelected(g[0],selected)' in launcher
assert 'i.putExtra("launch_game",selected)' in launcher
assert 'SelectedGameArgs.filename(requested)' in activity
assert 'PocoF7BootPolicy.conservative(' in activity
assert 'PocoF7BootPolicy.environment(pocoSafe)' in activity
assert '"/uncharted-daedalus.log\\n"' in launcher
assert 'uncharted-daedalus-"+previousGame+"-previous.log' in activity
assert 'java.nio.file.Files.copy(attemptLog.toPath(),gameLog.toPath()' in activity
assert 'boot-attempt.txt' in activity
assert 'File f=new File(root,"uncharted-daedalus.log")' in src("android/scripts/g2/game_app/LogExport.java")
assert 'TestPocoF7BootPolicy' in flow
assert 'if (bootArgs.isEmpty() && !SelectedGameArgs.valid(requested))' in activity
assert 'img.setScaleType(portrait?android.widget.ImageView.ScaleType.CENTER_CROP:' in launcher
assert 'android.widget.ImageView.ScaleType.FIT_CENTER' in launcher
assert 'gamesLayout.height=0' in launcher
assert 'gamesLayout.weight=1f' in launcher
assert 'games.addView(gallery,new LinearLayout.LayoutParams(-1,0,1f));' in launcher
assert 'new LinearLayout.LayoutParams(-1,0,1f)' in launcher
assert 'shell.addView(home,new LinearLayout.LayoutParams(-1,0,1));' in launcher
assert 'home.setVisibility(index==0?View.VISIBLE:View.GONE);' in launcher
assert 'scroll.setVisibility(index==0?View.GONE:View.VISIBLE);' in launcher
assert 'if(n!=0)root.addView(pages[n]' in launcher
assert 'statusText.setVisibility(View.GONE)' in launcher
assert 'TextView subtitle = note(headingBar, "THE NATHAN DRAKE COLLECTION")' in launcher
assert 'new LinearLayout.LayoutParams(0,-1,1f)' in launcher
assert 'backdrop.setRenderEffect(android.graphics.RenderEffect.createBlurEffect(' in launcher
assert 'int coverHeight=' not in launcher
assert 'String[] labels={"Off · manual resolution"' not in launcher
assert 'final String[] providers={"Off","FSR 3.1","Snapdragon GSR 2"}' in launcher
assert 'final String[] qualityLabels={"Native AA","Quality","Balanced","Performance"}' in launcher
assert 'UpscalePreset.compose(provider,quality)' in launcher
assert '@drawable/uncharted_icon_approved' in src("android/scripts/g2/game_app/res/drawable/collection.xml")
assert 'drawable-nodpi/uncharted_icon_approved.webp' in src("android/scripts/g2/game_app/build.sh")
public_manifest=src("android/scripts/g2/game_app/AndroidManifest-public.xml")
assert 'android:icon="@mipmap/ic_launcher"' in public_manifest
assert '<activity-alias android:name="dev.unchartedg2.game.UnchartedDaedalusIcon2026"' in public_manifest
assert 'android:targetActivity="dev.unchartedg2.game.Launcher"' in public_manifest
assert public_manifest.count('android.intent.category.LAUNCHER')==1
assert "grep -q 'E: activity-alias' dist/manifest.txt" in flow
assert "grep -q 'UnchartedDaedalusIcon2026' dist/manifest.txt" in flow
assert 'android:roundIcon="@mipmap/ic_launcher_round"' in src("android/scripts/g2/game_app/AndroidManifest-public.xml")
assert 'android:icon="@mipmap/ic_launcher"' in src("android/scripts/g2/game_app/AndroidManifest.xml")
assert '<adaptive-icon' in src("android/scripts/g2/game_app/res/mipmap-anydpi-v26/ic_launcher.xml")
assert '<adaptive-icon' in src("android/scripts/g2/game_app/res/mipmap-anydpi-v26/ic_launcher_round.xml")
assert '@drawable/uncharted_icon_approved' in src("android/scripts/g2/game_app/res/drawable/uncharted_launcher_foreground.xml")
assert 'approved Uncharted app icon resource' in src("android/scripts/g2/game_app/build.sh")
assert '@drawable/uncharted_icon_approved' in src("android/scripts/g2/game_app/res/drawable/collection.xml")
assert 'for(Button play:gamePlayButtons)play.setEnabled(problem == null && !busy);' in launcher
assert 'gamePlayButtons.clear();' in launcher
assert 'primaryButton(games,"Play selected game"' not in launcher
assert 'note(games,"Select a game to play.")' not in launcher
# One installation location only: Config > Games / Files, never duplicated in Games.
assert 'LinearLayout pkgQuickActions=row(games);' not in launcher
assert 'button(pkgQuickActions,"Install game PKG"' not in launcher
assert 'button(pkgQuickActions,"Install update PKG"' not in launcher
assert 'LinearLayout pkgActions=row(folder);' in launcher
assert 'button(pkgActions,"Install game PKG",v->installPkg(false));' in launcher
assert 'button(pkgActions,"Install update PKG",v->installPkg(true));' in launcher
assert 'case SetupFlow.PKG:pkgInstaller.pick(false)' in launcher
assert 'case SetupFlow.UPDATE:pkgInstaller.pick(true)' in launcher
assert 'selectedConfigSection=4;selectedPage=2;buildViews();refresh();' not in launcher
assert launcher.count('button(pkgActions,"Install game PKG"')==1
assert launcher.count('button(pkgActions,"Install update PKG"')==1
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

assert 'constexpr float PanelShare = 0.49f' in menu
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
assert 'lines[0].push_back({temp,purple})' in hud
assert 'lines[0].push_back({temp,cyan})' in hud
assert 'lines.push_back({{upscaler,normal}})' in hud
assert '"TAA",normal});' not in hud
assert 'Turnip_Gen8_V39_Patched.zip' in driver and 'Turnip_v27.0.0-R2.zip' in driver
assert 'bd457b30f195cf93a334b5302c221b1b5cb59c79c485935decf95d560a38c8a2' in driver
assert '8d30be022582d0c03b1918c019540491d00439e14e9e5e1359fe0bb19004f781' in driver
assert '"gpu_small_readbacks",false' in launcher
assert '"BB_COPY64_EOP_READBACK","u3".equals(game)?"1":"0"' in activity
assert '"BB_EFFECT_LOOP_CLAMP","1"' in activity
print("PASS: 0.3.0 selection, responsive immersive UI, patches and HUD")
