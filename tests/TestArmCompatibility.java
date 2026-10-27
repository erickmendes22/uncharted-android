package dev.unchartedg2.game;
public final class TestArmCompatibility {
    private static void check(boolean value){if(!value)throw new AssertionError();}
    public static void main(String[] args){
        for(String game:new String[]{"u1","u2","u3"})for(int mask=0;mask<8;mask++){
            boolean fast=(mask&1)!=0,batches=(mask&2)!=0,idle=(mask&4)!=0;
            String env=ArmCompatibility.environment(game,fast,batches,idle);
            check(env.contains("BB_JOB_SPIN="+(idle?"256,20":"0")+"\n"));
            if(game.equals("u1")){
                check(env.contains("-BB_GPU_MERGE\n")&&env.contains("-BB_UMA_READBACK\n"));
                check(env.contains("BB_FLUSH_DRAWS=0\n"));
            }else{
                check(env.contains("BB_GPU_MERGE=0\nBB_SIDE_DOWNLOAD=0\n"));
                check(env.contains("BB_UMA_READBACK="+(fast?"1":"0")+"\n"));
                check(env.contains(batches?"-BB_FLUSH_DRAWS\n":"BB_FLUSH_DRAWS=0\n"));
            }
        }
        System.out.println("Compatibility: all switch combinations and U1/U2/U3 isolation PASS");
    }
}
