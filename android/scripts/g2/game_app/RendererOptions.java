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
    static String environment(String game, boolean cache, boolean warmup, boolean compile,
                              boolean lights, boolean scalar, boolean nan, boolean readbacks) {
        return "BB_DRIVER_CACHE="+(cache?1:0)+"\n"
            +"BB_BACKGROUND_WARMUP="+(warmup?1:0)+"\n"
            +"BB_PREP_COMPILE="+(compile?1:0)+"\nBB_PREP_COMPUTE="+(compile?1:0)+"\n"
            +"BB_FULL_LIGHT_TABLE="+(lights?1:0)+"\n"
            +"BB_SCALAR_EXEC_FIX="+(scalar?1:0)+"\n"
            +"BB_SATURATE_NAN_FS="+(nan?1:0)+"\n"
            +"BB_READBACK_SMALL_BUFFERS="+(readbacks&&"u3".equals(game)?1:0)+"\n";
    }
}
