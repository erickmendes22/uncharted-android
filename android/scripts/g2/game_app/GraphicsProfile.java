package dev.unchartedg2.game;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Graphics-only recovery; never edits executables, saves, drivers or controller layout. */
final class GraphicsProfile {
    static final Set<String> KEYS = new HashSet<>(Arrays.asList(
        "upscaler", "preset", "output_res", "sharpen", "sharpness", "jitter",
        "reactive", "object_motion", "reactive_scale", "reactive_threshold", "reactive_max",
        "debug_view", "fsr4_auto_exposure", "fsr4_invert_jitter", "model_lod", "live_resolution",
        "frame_gen", "frame_gen_multiplier", "lsfg_flow_scale", "lsfg_fp16", "fps_limit"));
    static final String[] ENV = {"BB_UPSCALER", "BB_RENDER_RES", "BB_OUTPUT_RES",
        "BB_UPSCALE_PRESET", "BB_MOBILE_UPSCALE", "BB_FRAME_GEN", "BB_FRAME_GEN_MULTIPLIER",
        "BB_JITTER", "BB_REACTIVE", "BB_OBJECT_MOTION", "BB_DEFERRED_SUBMIT", "BB_DRAW_PIPE"};

    static LinkedHashMap<String,String> read(File file) throws IOException {
        LinkedHashMap<String,String> values=new LinkedHashMap<>();
        if(!file.isFile())return values;
        for(String line:Files.readAllLines(file.toPath(),StandardCharsets.UTF_8)){
            int eq=line.indexOf('=');
            if(eq>0 && !line.trim().startsWith("#"))values.put(line.substring(0,eq).trim(),line.substring(eq+1).trim());
        }
        return values;
    }
    static void write(File file,String text) throws IOException {
        Path temp=Files.createTempFile(file.getParentFile().toPath(),"graphics-",".tmp");
        try {
            Files.write(temp,text.getBytes(StandardCharsets.UTF_8));
            try{Files.move(temp,file.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);}
            catch(AtomicMoveNotSupportedException e){Files.move(temp,file.toPath(),StandardCopyOption.REPLACE_EXISTING);}
        } finally {Files.deleteIfExists(temp);}
    }
    static void resetIni(File file) throws IOException {
        LinkedHashMap<String,String> values=read(file);
        values.keySet().removeIf(k->KEYS.contains(k)||k.startsWith("effect_"));
        values.put("upscaler","off");values.put("preset","0");values.put("output_res","1280x720");
        values.put("frame_gen","off");values.put("frame_gen_multiplier","2");
        values.put("fps_limit","60");values.put("debug_view","0");values.put("object_motion","0");
        StringBuilder out=new StringBuilder();
        values.forEach((k,v)->out.append(k).append('=').append(v).append('\n'));
        write(file,out.toString());
    }
    static boolean graphicsPreference(String key) {
        return key.startsWith("patch_")||Arrays.asList("resolution","native","mobile_upscale","shadow_cascades").contains(key);
    }
    static String cleanOverrides(String text) {
        StringBuilder out=new StringBuilder();
        for(String line:text.split("\n")){
            String key=line.trim().replaceFirst("^-","").split("=",2)[0];
            if(Arrays.asList(ENV).contains(key)||key.startsWith("BB_FSR")||key.startsWith("BB_REACTIVE")||key.startsWith("BB_LSFG_"))continue;
            if(!line.isEmpty())out.append(line).append('\n');
        }
        return out.toString().trim();
    }
    static File backup(File root,Map<String,?> prefs) throws IOException {
        File dir=new File(root,"graphics-backup-"+System.currentTimeMillis());
        Files.createDirectory(dir.toPath());
        for(String name:new String[]{"bbport.ini","env.txt","env-u1.txt","env-u2.txt","env-u3.txt","args.txt","args-u1.txt","args-u2.txt","args-u3.txt"}){
            File source=new File(root,name);
            if(source.isFile())Files.copy(source.toPath(),new File(dir,name).toPath());
        }
        StringBuilder out=new StringBuilder();
        new TreeMap<>(prefs).forEach((k,v)->{if(graphicsPreference(k)||k.equals("extra_env"))out.append(k).append('=').append(v).append('\n');});
        write(new File(dir,"launcher-graphics.txt"),out.toString());
        return dir;
    }
    static String snapshot(File root,Map<String,?> prefs,Map<String,String> environment,String device,String game) throws IOException {
        StringBuilder out=new StringBuilder("--- Graphics configuration captured before native startup ---\n");
        out.append("Device: ").append(device).append("; game=").append(game).append('\n');
        new TreeMap<>(prefs).forEach((k,v)->{if(graphicsPreference(k))out.append("Launcher ").append(k).append('=').append(v).append('\n');});
        read(new File(root,"bbport.ini")).forEach((k,v)->{if(KEYS.contains(k))out.append("INI ").append(k).append('=').append(v).append('\n');});
        for(String key:ENV)out.append("Environment ").append(key).append('=').append(environment.getOrDefault(key,"<unset>")).append('\n');
        File args=new File(root,"args-"+game+".txt");
        if(args.isFile()){
            List<String> lines=Files.readAllLines(args.toPath(),StandardCharsets.UTF_8);
            for(int i=0;i+1<lines.size();i++)if(lines.get(i).equals("--patches")){
                File patch=new File(lines.get(i+1));
                out.append("Patch file: ").append(patch.getName()).append("; bytes=").append(patch.length()).append('\n');
                if(patch.isFile())try{
                    byte[] digest=java.security.MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(patch.toPath()));
                    out.append("Patch SHA256: ");for(byte b:digest)out.append(String.format(Locale.ROOT,"%02x",b));out.append('\n');
                }catch(java.security.NoSuchAlgorithmException impossible){throw new IOException(impossible);}
            }
        }
        return out.append("--- Native session log follows ---\n").toString();
    }
}
