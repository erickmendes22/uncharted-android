package dev.unchartedg2.game;

/** The three Android compatibility controls used by RedVorteX android-c303023. */
final class ArmCompatibility {
    static String environment(String game,boolean fastReads,boolean smallBatches,boolean idleSleep) {
        boolean sequel="u2".equals(game)||"u3".equals(game);
        // U1 keeps its existing readback policy. Explicit unsets prevent a previous
        // game's environment from leaking through loadexec/restart.
        return (sequel ? "BB_GPU_MERGE=0\nBB_SIDE_DOWNLOAD=0\nBB_UMA_READBACK="+(fastReads?"1":"0")+"\n"
                        : "-BB_GPU_MERGE\n-BB_SIDE_DOWNLOAD\n-BB_UMA_READBACK\n")
            +(sequel && smallBatches ? "-BB_FLUSH_DRAWS\n" : "BB_FLUSH_DRAWS=0\n")
            +"BB_JOB_SPIN="+(idleSleep?"256,20":"0")+"\n";
    }
}
