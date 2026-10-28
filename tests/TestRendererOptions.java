package dev.unchartedg2.game;
import java.util.*;
public final class TestRendererOptions {
    static void check(boolean v){if(!v)throw new AssertionError();}
    public static void main(String[] args){
        for(String game:new String[]{"u1","u2","u3"})for(int mask=0;mask<128;mask++){
            boolean[] v=new boolean[7];for(int i=0;i<7;i++)v[i]=(mask&(1<<i))!=0;
            String env=RendererOptions.environment(game,v[0],v[1],v[2],v[3],v[4],v[5],v[6]);
            Map<String,String> flags=new HashMap<>();
            for(String line:env.split("\n")){String[] pair=line.split("=");check(flags.put(pair[0],pair[1])==null);}
            check(flags.size()==8);
            int[] slot={0,1,2,2,3,4,5,6};
            for(int i=0;i<8;i++)check(flags.get(RendererOptions.KEYS[i]).equals(v[slot[i]]&&(i!=7||game.equals("u3"))?"1":"0"));
            check(!env.contains("BB_GPU_MERGE")&&!env.contains("BB_SIDE_DOWNLOAD"));
            check(!env.contains("BB_SCALAR_COPY64")&&!env.contains("BB_TINY_READBACK"));
        }
        List<String> lines=new ArrayList<>();
        for(String key:RendererOptions.KEYS){lines.add(key+"=1");lines.add("-"+key);}
        lines.add("BB_SCALAR_COPY64_DEFER=1");lines.add("BB_TINY_READBACK=1");
        check(RendererOptions.cleanPerGame(lines).equals(Arrays.asList("BB_SCALAR_COPY64_DEFER=1","BB_TINY_READBACK=1")));
        System.out.println("PASS: 384 renderer combinations, U3-only readbacks, stale flags, no readback/queue policy override");
    }
}
