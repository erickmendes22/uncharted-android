package dev.unchartedg2.game;

public final class TestDpadResolver {
    private static void eq(int want,int actual,String name) {
        if(want!=actual) throw new AssertionError(name+": expected "+want+", got "+actual);
    }
    public static void main(String[] args) {
        eq(0,DpadResolver.resolve(0f,0f,1f),"neutral");
        eq(DpadResolver.UP,DpadResolver.resolve(0f,-10f,1f),"up small tap");
        eq(DpadResolver.DOWN,DpadResolver.resolve(0f,10f,1f),"down small tap");
        eq(DpadResolver.LEFT,DpadResolver.resolve(-11f,0f,1f),"left small tap");
        eq(DpadResolver.RIGHT,DpadResolver.resolve(11f,0f,1f),"right small tap");
        eq(DpadResolver.UP|DpadResolver.RIGHT,DpadResolver.resolve(25f,-25f,1f),"up-right diagonal");
        eq(DpadResolver.DOWN|DpadResolver.LEFT,DpadResolver.resolve(-25f,25f,1f),"down-left diagonal");
        eq(DpadResolver.RIGHT,DpadResolver.resolve(25f,-7f,1f),"dominant axis");
        eq(0,DpadResolver.resolve(4f,-4f,1f),"small central jitter");
        eq(0,DpadResolver.resolve(6f,-6f,1.3f),"scaled central jitter");
        eq(DpadResolver.RIGHT,DpadResolver.resolve(15f,0f,1.3f),"scaled right tap");
        eq(DpadResolver.RIGHT,DpadResolver.resolve(24f,0f,1f),"widescreen visual X delta");
        System.out.println("PASS: four D-pad directions, diagonals, widescreen and touch deadzone");
    }
}
