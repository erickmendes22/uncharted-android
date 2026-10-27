package dev.unchartedg2.game;

/** Optional RedVorteX mechanisms on the published Daedalus 0.2.5 runtime. */
final class ArmCompatibility {
    static final String[] OWNED={"BB_GPU_MERGE","BB_SIDE_DOWNLOAD","BB_UMA_READBACK","BB_FLUSH_DRAWS","BB_JOB_SPIN"};
    static boolean owned(String key){for(String k:OWNED)if(k.equals(key))return true;return false;}
    static String key(String line){return line.startsWith("-")?line.substring(1):line.split("=",2)[0];}
    static java.util.List<String> cleanPerGame(java.util.List<String> lines){
        java.util.List<String> clean=new java.util.ArrayList<>();
        for(String line:lines)if(!owned(key(line)))clean.add(line);
        return clean;
    }
    static String environment(String game,boolean fastReads,boolean smallBatches,boolean idleSleep) {
        boolean sequel="u2".equals(game)||"u3".equals(game);
        // These options never choose a readback precision or override the side queue.
        return "BB_UMA_READBACK="+(sequel&&fastReads?"1":"0")+"\n"
            +"BB_FLUSH_DRAWS="+(sequel&&smallBatches?"100":"0")+"\n"
            +"BB_JOB_SPIN="+(idleSleep?"256,20":"0")+"\n";
    }
}
