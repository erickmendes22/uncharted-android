package dev.unchartedg2.game;

import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;
import java.util.Set;

/** Port of the working GoW3/Bloodborne Android pad-file bridge.
 * Native OrbisPad already accepts BB_PAD_FILE; never synthesize SDL touches.
 * This bridge only publishes touch input, leaving physical SDL3 controllers alone.
 */
final class PadBridge {
    private final File state;
    private final Set<String> virtualButtons = new LinkedHashSet<>();
    private String previous = null;
    PadBridge(File state) { this.state = state; reset(); }

    void virtual(Set<String> buttons, int lx, int ly, int rx, int ry,
                 boolean leftActive, boolean rightActive, int l2, int r2) {
        virtualButtons.clear();
        virtualButtons.addAll(buttons);
        StringBuilder line = new StringBuilder();
        for (String b : virtualButtons) {
            if (b.equals("l2") || b.equals("r2")) continue;
            line.append(b).append(' ');
        }
        line.append("lx=").append(leftActive ? clamp(lx) : 128)
            .append(" ly=").append(leftActive ? clamp(ly) : 128)
            .append(" rx=").append(rightActive ? clamp(rx) : 128)
            .append(" ry=").append(rightActive ? clamp(ry) : 128)
            .append(" l2=").append(clamp(l2))
            .append(" r2=").append(clamp(r2)).append('\n');
        publish(line.toString());
    }
    private static int clamp(int x) { return Math.min(255, Math.max(0, x)); }
    void reset() {
        virtualButtons.clear();
        virtual(virtualButtons,128,128,128,128,false,false,0,0);
    }
    private void publish(String text) {
        if (text.equals(previous)) return;
        File temp = new File(state.getAbsolutePath() + ".tmp");
        try {
            state.getParentFile().mkdirs();
            try (FileOutputStream out = new FileOutputStream(temp)) {
                out.write(text.getBytes(StandardCharsets.US_ASCII));
            }
            if (!temp.renameTo(state)) throw new java.io.IOException("pad-state rename failed");
            previous = text;
        } catch (java.io.IOException ex) {
            Log.e("Uncharted PadBridge", "Cannot publish virtual PS4 pad state", ex);
        }
    }
}
