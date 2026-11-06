package dev.unchartedg2.game;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.security.MessageDigest;

/**
 * Refuse incomplete or mixed Nathan Drake Collection archives before the
 * ingame-shaders.cpp files.json assertion. The shader list is IN bin.psarc;
 * checking for a loose files.json would incorrectly reject valid installs.
 * No game assets are supplied or modified by this check.
 */
final class GameArchiveCheck {
    private GameArchiveCheck() {}
    private static final String[] GAME = {"u1", "u2", "u3"};
    private static final String[] EXEC = {"eboot.bin", "big2-ps4_Shipping.elf", "big3-ps4_Shipping.elf"};
    private static final String[] BASE_HASH = {
        "8953b6ed5cf4d1d26aa3aa4cd1e0c72817ea155f6a5774fb17f6fec7d787f953",
        "61c0456314a8de2dbd32e1c426f7828092de7d25a58b83386ced1a48d7765ebe",
        "acc952e5a9ac7062f8ecfc52700faa599d5f29deb1e24f9e81e6de832326d1d0"
    };
    private static final long[] UPDATE_BIN_SIZE = {9283355L, 14157742L, 30065818L};
    private static final String[] UPDATE_BIN_HASH = {
        "675276726ee25bd1b3b6f540eb37fad6f0570a9c2d1006fcdcee70c1811a7829",
        "9728077fc5fc7f3f000fa7f2716b6d29178cb28c3e434b427f01c1485c7ff4fd",
        "2c40ad5b3b6dabb5d8b9ea639be4d6748291c661aa3b35f646929d3b5bf8aa14"
    };
    /** Returns an actionable error, or null. Read-only. */
    static String problem(File root, String game) {
        int n=-1;
        for(int i=0;i<GAME.length;i++)if(GAME[i].equals(game)){n=i;break;}
        if(n<0)return "Unknown Uncharted game: "+game;
        File main=new File(root,GAME[n]+"data/build/main");
        File loose=new File(main,"bin/shaders/src/files.json");
        File bin=new File(main,"bin.psarc");
        File core=new File(main,"core.psarc");
        if (!bin.isFile() && !loose.isFile())
            return game.toUpperCase()+": missing "+bin.getPath()+" (shader list files.json is stored in this archive). Reinstall the matching base/update game files.";
        for(File file:new File[]{bin,core}) {
            if(!file.exists()){
                // A fully unpacked game can have loose shader/engine resources.
                if(loose.isFile())continue;
                return game.toUpperCase()+": missing "+file.getPath()+". Reinstall the matching PKG/update.";
            }
            if(file.length()<32)
                return game.toUpperCase()+": truncated archive "+file.getPath();
            try(RandomAccessFile in=new RandomAccessFile(file,"r")){
                if(in.readInt()!=0x50534152)
                    return game.toUpperCase()+": invalid PSARC header in "+file.getPath();
            }catch(IOException e){return game.toUpperCase()+": cannot read "+file.getPath()+": "+e.getMessage();}
        }
        // Proven 01.00 executable plus byte-identical 01.02 shader archive is a
        // mismatched pair; this is a diagnostic, NOT a blanket rejection of
        // different regional archives with otherwise legitimate hashes.
        File exec=new File(root,EXEC[n]);
        if(bin.isFile() && exec.isFile() && bin.length()==UPDATE_BIN_SIZE[n]){
            try{
                if(UPDATE_BIN_HASH[n].equals(sha256(bin)) && BASE_HASH[n].equals(sha256(exec)))
                    return game.toUpperCase()+": 01.00 executable with 01.02 bin.psarc. Install matching game/update executables and archives together; mixing them causes files.json initialization failure.";
            }catch(Exception e){return game.toUpperCase()+": failed to verify game archive: "+e.getMessage();}
        }
        return null;
    }
    private static String sha256(File file) throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");
        try(FileInputStream in=new FileInputStream(file)){
            byte[] buf=new byte[64*1024];
            for(int read;(read=in.read(buf))!=-1;)hash.update(buf,0,read);
        }
        StringBuilder result=new StringBuilder(64);
        for(byte b:hash.digest())result.append(Character.forDigit((b>>>4)&15,16)).append(Character.forDigit(b&15,16));
        return result.toString();
    }
}
