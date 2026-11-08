package dev.unchartedg2.game;
import java.util.*;
public final class TestArmCompatibility {
    private static void check(boolean ok){if(!ok)throw new AssertionError();}
    public static void main(String[] args){
        for(String game:new String[]{"u1","u2","u3"})for(int mask=0;mask<8;mask++){
            boolean fast=(mask&1)!=0,batch=(mask&2)!=0,idle=(mask&4)!=0;
            String env=ArmCompatibility.environment(game,fast,batch,idle);
            // No combination is permitted to select precise readback or disable
            // the original side queue. This catches the previous OFF regression.
            check(!env.contains("BB_GPU_MERGE")&&!env.contains("BB_SIDE_DOWNLOAD"));
            check(!env.contains("BB_SIDE_WAIT"));
            check(env.contains("BB_UMA_READBACK="+(!game.equals("u1")&&fast?"1":"0")+"\n"));
            check(env.contains("BB_FLUSH_DRAWS="+(!game.equals("u1")&&batch?"100":"0")+"\n"));
            check(env.contains("BB_JOB_SPIN="+(idle?"256,20":"0")+"\n"));
            boolean upstream=idle&&!game.equals("u2");
            check(env.contains("BB_JOB_SLEEP="+(upstream?"1":"0")+"\n"));
            check(env.contains("BB_JOB_SPINS="+(upstream?"2":"0")+"\n"));
            check(env.contains("BB_JOB_WAIT_US=1000\n"));
            check(env.contains("BB_JOB_RESUME_WAKE="+(upstream?"1":"0")+"\n"));
        }
        List<String> old=Arrays.asList("BB_GPU_MERGE=0","BB_SIDE_DOWNLOAD=0","BB_UMA_READBACK=1",
            "-BB_GPU_MERGE","-BB_FLUSH_DRAWS","BB_JOB_SPIN=256,20","BB_TINY_READBACK=1",
            "BB_RENDER_RES=640x360","-BB_OUTPUT_RES",
            "BB_JOB_SLEEP=1","BB_JOB_SPINS=123","-BB_JOB_RESUME_WAKE",
            "BB_JOB_WAIT_US=2000");
        List<String> clean=ArmCompatibility.cleanPerGame(old);
        check(clean.equals(Arrays.asList("BB_TINY_READBACK=1","BB_RENDER_RES=640x360","-BB_OUTPUT_RES")));
        String off=ArmCompatibility.environment("u3",false,false,false);
        check(off.equals("BB_UMA_READBACK=0\nBB_FLUSH_DRAWS=0\nBB_JOB_SPIN=0\n"
            +"BB_JOB_SLEEP=0\nBB_JOB_SPINS=0\nBB_JOB_WAIT_US=1000\nBB_JOB_RESUME_WAKE=0\n"));
        String u2=ArmCompatibility.environment("u2",true,true,true);
        check(u2.contains("BB_JOB_SPIN=256,20\n"));
        check(u2.contains("BB_JOB_SLEEP=0\nBB_JOB_SPINS=0\nBB_JOB_WAIT_US=1000\nBB_JOB_RESUME_WAKE=0\n"));
        System.out.println("PASS: all 24 combinations; U2 falls back to Daedalus 0.2.9 worker policy; U1/U3 2-spin guard and rollback; stale flags cleaned");
    }
}
