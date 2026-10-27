package dev.unchartedg2.game;
import java.nio.file.*;
import java.io.*;
import java.util.*;
public final class TestGraphicsProfile {
 public static void main(String[] args)throws Exception{
  File root=Files.createTempDirectory("uncharted-graphics-test").toFile();
  File ini=new File(root,"bbport.ini");
  String original="upscaler=fsr3\npreset=3\noutput_res=3840x2160\ndebug_view=2\njitter=0\nreactive=1\nframe_gen=lsfg\nframe_gen_multiplier=4\nhud_scale=2.25\ntouch_pad_look=3.0\nfullscreen=1\n";
  GraphicsProfile.write(ini,original);
  Map<String,Object> prefs=new HashMap<>();prefs.put("resolution","640x360");prefs.put("patch_Disable fog",true);prefs.put("driver","pkg:my-turnip");prefs.put("language","pt");
  File save=new File(root,"save.dat");Files.write(save.toPath(),new byte[]{1,2,3});
  File backup=GraphicsProfile.backup(root,prefs);
  GraphicsProfile.resetIni(ini);
  Map<String,String> clean=GraphicsProfile.read(ini);
  if(!"off".equals(clean.get("upscaler"))||!"off".equals(clean.get("frame_gen"))||!"1280x720".equals(clean.get("output_res"))||!"0".equals(clean.get("debug_view")))throw new AssertionError(clean);
  if(clean.containsKey("reactive")||clean.containsKey("jitter"))throw new AssertionError("hidden overrides survived");
  if(!"2.25".equals(clean.get("hud_scale"))||!"3.0".equals(clean.get("touch_pad_look"))||!"1".equals(clean.get("fullscreen")))throw new AssertionError("unrelated settings changed");
  if(!original.equals(Files.readString(new File(backup,"bbport.ini").toPath())))throw new AssertionError("backup differs");
  if(!Arrays.equals(Files.readAllBytes(save.toPath()),new byte[]{1,2,3}))throw new AssertionError("save changed");
  if(GraphicsProfile.graphicsPreference("driver")||GraphicsProfile.graphicsPreference("language")||!GraphicsProfile.graphicsPreference("patch_Disable fog"))throw new AssertionError("preference scope");
  String overrides=GraphicsProfile.cleanOverrides("BB_RENDER_RES=640x360\nBB_OUTPUT_RES=3840x2160\n-BB_UPSCALER\nBB_FRAME_GEN=lsfg\nBB_FSR_SHARPNESS=2\nBB_VK_DRIVER=custom.so\nFEX_MULTIBLOCK=1");
  if(!overrides.equals("BB_VK_DRIVER=custom.so\nFEX_MULTIBLOCK=1"))throw new AssertionError(overrides);
  String snapshot=GraphicsProfile.snapshot(root,prefs,Map.of("BB_RENDER_RES","640x360"),"test-device","u1");
  if(!snapshot.contains("Launcher patch_Disable fog=true")||!snapshot.contains("Environment BB_RENDER_RES=640x360")||!snapshot.contains("INI upscaler=off"))throw new AssertionError(snapshot);
  System.out.println("Graphics recovery: backup, hidden-state reset, settings preservation and startup snapshot PASS");
 }
}
