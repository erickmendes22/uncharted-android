package dev.unchartedg2.game;
import java.util.*;
public final class TestRendererOptions {
    static void check(boolean v){if(!v)throw new AssertionError();}
    public static void main(String[] args){
        for(String game:new String[]{"u1","u2","u3"}){
            String env=RendererOptions.environment(game);
            Map<String,String> flags=new HashMap<>();
            for(String line:env.split("\n")){String[] pair=line.split("=");check(flags.put(pair[0],pair[1])==null);}
            check(flags.size()==8);
            for(String k:RendererOptions.KEYS)
                check(flags.get(k).equals(k.equals("BB_SCALAR_EXEC_FIX")||k.equals("BB_READBACK_SMALL_BUFFERS")?"0":"1"));
            check(!env.contains("BB_GPU_MERGE")&&!env.contains("BB_SIDE_DOWNLOAD"));
            String enabled=RendererOptions.environment(game,true);
            check(enabled.contains("BB_READBACK_SMALL_BUFFERS="+(game.equals("u1")?"0":"1")+"\n"));
            check(RendererOptions.environment(game,false).equals(env));
            check(!env.contains("BB_SCALAR_COPY64")&&!env.contains("BB_TINY_READBACK"));
        }
        List<String> lines=new ArrayList<>();
        for(String key:RendererOptions.KEYS){lines.add(key+"=1");lines.add(key+"=0");lines.add("-"+key);}
        lines.add("BB_SCALAR_COPY64_DEFER=1");lines.add("BB_TINY_READBACK=1");
        check(RendererOptions.cleanPerGame(lines).equals(Arrays.asList("BB_SCALAR_COPY64_DEFER=1","BB_TINY_READBACK=1")));
        System.out.println("PASS: small readbacks optional off by default, on for U2/U3 when selected; fixed renderer options unchanged");
    }
}
