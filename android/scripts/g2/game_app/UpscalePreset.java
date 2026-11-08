package dev.unchartedg2.game;
/** Applied at startup; known safe resolution recipes shared by all three games. */
final class UpscalePreset {
    static final String OUTPUT="1280x720";
    static final String[] SIZES={null,"1280x720","960x540","832x468","640x360"};
    static final String[] NAMES={"Off","FSR Native AA","FSR Quality","FSR Balanced","FSR Performance",
        "GSR 2 Native AA","GSR 2 Quality","GSR 2 Balanced","GSR 2 Performance"};
    static int normalize(int mode){return Math.max(0,Math.min(8,mode));}
    static int preset(int mode){int m=normalize(mode);return m>=5?m-4:m;}
    static String provider(int mode){return normalize(mode)>=5?"gsr2":"fsr3";}
    static String input(int mode,String manual){return normalize(mode)==0?manual:SIZES[preset(mode)];}
    // FSR 3.1/GSR 2 require the original AA and motion/velocity passes.
    static boolean disableNativeAa(int mode,boolean userDisabled){return normalize(mode)>0 || userDisabled;}
    static boolean disableMotionBlur(int mode,boolean userDisabled){return userDisabled;}
    static String label(int mode){return NAMES[normalize(mode)];}
}
