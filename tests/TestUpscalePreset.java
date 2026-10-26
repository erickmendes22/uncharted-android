package dev.unchartedg2.game;
public final class TestUpscalePreset {
 public static void main(String[] args) {
  String[] expected={"832x468","1280x720","960x540","832x468","640x360"};
  for(int mode=0;mode<5;mode++) {
   if(!UpscalePreset.input(mode,"832x468").equals(expected[mode]))throw new AssertionError("input "+mode);
   if(!UpscalePreset.OUTPUT.equals("1280x720"))throw new AssertionError("output");
  }
  if(!UpscalePreset.label(3).equals("FSR Balanced"))throw new AssertionError("balanced label");
  if(!UpscalePreset.label(4).equals("FSR Performance"))throw new AssertionError("performance label");
  if(!UpscalePreset.label(5).equals("FSR Performance"))throw new AssertionError("legacy preset migration");
  System.out.println("FSR presets: PASS (Native 720, Quality 540, Balanced 468, Performance 360 -> 720p)");
 }
}
