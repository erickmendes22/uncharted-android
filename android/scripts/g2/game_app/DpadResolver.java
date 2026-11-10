package dev.unchartedg2.game;

/** Pure D-pad direction resolver shared by all on-screen pad layouts.
 * Inputs are physical on-screen deltas in the overlay's 720-unit height space.
 * A small deadzone prevents accidental movement at the exact intersection;
 * adjacent directions can be held together for diagonals, never opposites.
 */
final class DpadResolver {
    static final int UP=1, RIGHT=2, DOWN=4, LEFT=8;
    private DpadResolver() {}

    static int resolve(float dx,float dy,float size) {
        final float deadzone=5f*Math.max(.5f,size);
        final float ax=Math.abs(dx), ay=Math.abs(dy);
        if (Math.max(ax,ay)<=deadzone) return 0;
        int result=0;
        // No 22-unit dead area: a normal tap on a cross arm now registers.
        // The 0.70 ratio preserves intentional diagonals near 45 degrees.
        if (ax>deadzone && ax>=.70f*ay) result|=dx<0?LEFT:RIGHT;
        if (ay>deadzone && ay>=.70f*ax) result|=dy<0?UP:DOWN;
        return result;
    }
}
