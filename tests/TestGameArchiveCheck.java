package dev.unchartedg2.game;
import java.nio.file.Files;
import java.nio.file.Path;
import java.io.File;
public final class TestGameArchiveCheck {
    static void ok(boolean b){if(!b)throw new AssertionError();}
    static void psarc(Path f) throws Exception {
        Files.createDirectories(f.getParent());
        byte[] buf=new byte[64];
        buf[0]='P';buf[1]='S';buf[2]='A';buf[3]='R';
        Files.write(f,buf);
    }
    public static void main(String[] args) throws Exception {
        Path root=Files.createTempDirectory("uncharted-archive-check");
        ok(GameArchiveCheck.problem(root.toFile(),"u3").contains("missing"));
        Path main=root.resolve("u3data/build/main");
        Path bin=main.resolve("bin.psarc"),core=main.resolve("core.psarc");
        psarc(bin);psarc(core);
        ok(GameArchiveCheck.problem(root.toFile(),"u3")==null);
        Files.write(core,new byte[]{'X','X'});
        ok(GameArchiveCheck.problem(root.toFile(),"u3").contains("truncated"));
        Files.write(core,new byte[64]);
        ok(GameArchiveCheck.problem(root.toFile(),"u3").contains("invalid PSARC header"));
        Files.delete(bin);Files.delete(core);
        Path loose=main.resolve("bin/shaders/src/files.json");
        Files.createDirectories(loose.getParent());Files.writeString(loose,"{}");
        ok(GameArchiveCheck.problem(root.toFile(),"u3")==null); // unpacked layouts stay supported
        ok(GameArchiveCheck.problem(root.toFile(),"u9").contains("Unknown"));
        System.out.println("PASS: archive header/truncation checks, missing shader data and unpacked layouts");
    }
}
