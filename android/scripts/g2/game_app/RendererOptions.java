package dev.unchartedg2.game;

/** Only mechanisms missing from the published renderer. Applied at every process start. */
final class RendererOptions {
    static final String[] KEYS={"BB_DRIVER_CACHE","BB_BACKGROUND_WARMUP","BB_PREP_COMPILE",
        "BB_PREP_COMPUTE","BB_FULL_LIGHT_TABLE","BB_SCALAR_EXEC_FIX","BB_SATURATE_NAN_FS",
        "BB_READBACK_SMALL_BUFFERS"};
    static boolean owned(String key){for(String k:KEYS)if(k.equals(key))return true;return false;}
    static java.util.List<String> cleanPerGame(java.util.List<String> lines){
        java.util.List<String> out=new java.util.ArrayList<>();
        for(String line:lines)if(!owned(ArmCompatibility.key(line)))out.add(line);
        return out;
    }
    // Fixed release policy matching the user's tested configuration. Reapplied after old
    // preferences and environment files on every launch, including collection loadexec.
    static String environment(String game) {
        return "BB_DRIVER_CACHE=1\nBB_BACKGROUND_WARMUP=1\n"
            +"BB_PREP_COMPILE=1\nBB_PREP_COMPUTE=1\n"
            +"BB_FULL_LIGHT_TABLE=1\nBB_SCALAR_EXEC_FIX=0\n"
            +"BB_SATURATE_NAN_FS=1\nBB_READBACK_SMALL_BUFFERS=0\n";
    }
}
