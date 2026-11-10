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
  if(!UpscalePreset.label(5).equals("GSR 2 Native AA"))throw new AssertionError("GSR native");
  if(!UpscalePreset.label(8).equals("GSR 2 Performance"))throw new AssertionError("GSR performance");
  for(int mode=5;mode<=8;mode++) {
   if(!UpscalePreset.input(mode,"1920x1080").equals(expected[mode-4]))throw new AssertionError("GSR input "+mode);
   if(!UpscalePreset.provider(mode).equals("gsr2"))throw new AssertionError("GSR provider "+mode);
   if(UpscalePreset.preset(mode)!=mode-4)throw new AssertionError("GSR preset "+mode);
  }
  if(!UpscalePreset.provider(4).equals("fsr3"))throw new AssertionError("FSR provider");
  for(int providerChoice=0;providerChoice<=2;providerChoice++)
   for(int quality=1;quality<=4;quality++){
    int combined=UpscalePreset.compose(providerChoice,quality);
    if(combined!=(providerChoice==0?0:providerChoice==1?quality:quality+4))
      throw new AssertionError("compose "+providerChoice+" "+quality);
    if(UpscalePreset.providerChoice(combined)!=providerChoice)
      throw new AssertionError("provider selection "+combined);
    if(providerChoice!=0 && UpscalePreset.qualityChoice(combined)!=quality)
      throw new AssertionError("quality selection "+combined);
   }
  if(UpscalePreset.compose(0,3)!=0)throw new AssertionError("Off must disable upscale");
  if(UpscalePreset.compose(2,4)!=8)throw new AssertionError("GSR performance");
  for(int mode=0;mode<=8;++mode)for(boolean saved:new boolean[]{false,true}) {
   boolean effective=UpscalePreset.disableNativeAa(mode,saved);
   if(effective!=(mode>0 || saved))throw new AssertionError("native AA conflict mode="+mode);
  }
  if(UpscalePreset.disableNativeAa(0,false))throw new AssertionError("native AA must return to user's saved enabled choice");
  for(int mode=1;mode<=8;mode++)if(!UpscalePreset.disableMotionBlur(mode,true))throw new AssertionError("user motion blur patch ignored");
   System.out.println("FSR presets: PASS (Native 720, Quality 540, Balanced 468, Performance 360 -> 720p)");
 }
}
