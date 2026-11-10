package dev.unchartedg2.game;

/** Shared, side-effect-free ID mapping for game-card launches and SDL boot. */
final class SelectedGameArgs {
    private SelectedGameArgs() {}
    static boolean valid(String id) {
        return "u1".equals(id)||"u2".equals(id)||"u3".equals(id);
    }
    static boolean isSelected(String candidate,String requested) {
        return valid(requested)&&requested.equals(candidate);
    }
    static String filename(String id) {
        if(!valid(id))throw new IllegalArgumentException("Invalid Uncharted game ID: "+id);
        return "args-"+id+".txt";
    }
}
