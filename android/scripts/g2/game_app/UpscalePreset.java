package dev.unchartedg2.game;
/** Applied at startup; known safe resolution recipes shared by all three games. */
final class UpscalePreset {
    static final String OUTPUT="1280x720";
    static final String[] SIZES={null,"1280x720","960x540","832x468","640x360"};
    static final String[] NAMES={"Off","FSR Native AA","FSR Quality","FSR Balanced","FSR Performance"};
    static int normalize(int mode){return Math.max(0,Math.min(4,mode));}
    static String input(int mode,String manual){return mode==0?manual:SIZES[normalize(mode)];}
    static String label(int mode){return NAMES[normalize(mode)];}
}
