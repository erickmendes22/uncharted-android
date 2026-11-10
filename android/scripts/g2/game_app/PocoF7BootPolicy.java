package dev.unchartedg2.game;

/**
 * Conservative U2/U3 startup for Snapdragon 8s Gen 4 / Adreno 825 on POCO F7.
 * These optional GPU readback/batching and background compilation paths are
 * more aggressive than U1's defaults. Do not change U1 or other devices.
 *
 * A successful boot must be validated on-device before re-enabling the
 * optimizations: a U1 log does NOT prove the U2/U3 native crash mechanism.
 */
final class PocoF7BootPolicy {
    private PocoF7BootPolicy(){}
    static boolean isPocoF7(String model, String device, String soc) {
        return "onyx".equalsIgnoreCase(device==null?"":device.trim()) ||
            ("POCO F7".equalsIgnoreCase(model==null?"":model.trim()) &&
             ("SM8735".equalsIgnoreCase(soc==null?"":soc.trim()) ||
              soc==null || soc.trim().isEmpty()));
    }
    static boolean conservative(String model, String device, String soc, String game) {
        return ("u2".equals(game)||"u3".equals(game)) && isPocoF7(model,device,soc);
    }
    static String environment(boolean conservative) {
        if(!conservative)return "";
        // No speculative "GPU bug fixed" claim: disable these optional fast
        // paths while gathering reproducible U2/U3 startup logs.
        return "BB_UMA_READBACK=0\n" +
            "BB_FLUSH_DRAWS=0\n" +
            "BB_BACKGROUND_WARMUP=0\n" +
            "BB_PREP_COMPILE=0\n" +
            "BB_PREP_COMPUTE=0\n";
    }
}
