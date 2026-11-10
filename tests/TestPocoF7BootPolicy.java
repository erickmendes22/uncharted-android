package dev.unchartedg2.game;

public final class TestPocoF7BootPolicy {
    private static void ok(boolean yes,String label) {
        if(!yes)throw new AssertionError(label);
    }
    public static void main(String[] args) {
        ok(PocoF7BootPolicy.conservative("POCO F7","onyx","SM8735","u2"),"Poco F7 U2");
        ok(PocoF7BootPolicy.conservative("POCO F7","onyx","SM8735","u3"),"Poco F7 U3");
        ok(!PocoF7BootPolicy.conservative("POCO F7","onyx","SM8735","u1"),"U1 untouched");
        ok(!PocoF7BootPolicy.conservative("REDMAGIC 11 Pro","NX809J","SM8850","u2"),"other phones untouched");
        ok(!PocoF7BootPolicy.conservative("POCO F7 Pro","zorn","SM8750","u2"),"F7 Pro untouched");
        ok(!PocoF7BootPolicy.conservative("POCO F7 Ultra","miro","SM8750","u3"),"F7 Ultra untouched");
        ok(!PocoF7BootPolicy.environment(true).contains("BB_DRIVER_CACHE=0"),"preserve verified cache");
        ok(PocoF7BootPolicy.environment(true).contains("BB_BACKGROUND_WARMUP=0"),"no warm-up");
        ok(PocoF7BootPolicy.environment(true).contains("BB_UMA_READBACK=0"),"no UMA");
        ok(PocoF7BootPolicy.environment(false).isEmpty(),"no non-target override");
        System.out.println("PASS: POCO F7 U2/U3 targeted startup fallback");
    }
}
