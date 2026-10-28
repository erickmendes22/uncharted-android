package dev.unchartedg2.game;
import java.io.*;
import java.nio.file.*;
public final class TestStoragePaths {
    static void require(boolean ok){if(!ok)throw new AssertionError();}
    public static void main(String[] args) throws Exception {
        Path tmp=Files.createTempDirectory("uncharted-storage-");
        File root=tmp.toFile();
        require(StoragePaths.external("PRIMARY:Download/Uncharted",root).equals(new File(root,"Download/Uncharted").getCanonicalFile()));
        require(StoragePaths.external("primary:",root).equals(root.getCanonicalFile()));
        for(String bad:new String[]{"primary:../escape","bad/volume:foo",":foo","primary"}){
            boolean rejected=false;try{StoragePaths.external(bad,root);}catch(IOException expected){rejected=true;}require(rejected);
        }
        File game=new File(root,"CUSA02343");require(game.mkdir());
        require(new File(game,"u1data").mkdir());require(new File(game,"eboot.bin").createNewFile());
        require(StoragePaths.gameFolder(root).equals(game.getCanonicalFile()));
        require(StoragePaths.gameFolder(game).equals(game.getCanonicalFile()));
        File unrelated=new File(root,"Other");require(unrelated.mkdir());
        require(StoragePaths.gameFolder(unrelated).equals(unrelated.getCanonicalFile()));
        Path outside=Files.createTempDirectory("uncharted-outside-");
        Files.createSymbolicLink(tmp.resolve("link"),outside);
        boolean rejected=false;try{StoragePaths.external("primary:link",root);}catch(IOException expected){rejected=true;}require(rejected);
        try(java.util.stream.Stream<Path> paths=Files.walk(tmp)){paths.sorted(java.util.Comparator.reverseOrder()).forEach(p->{try{Files.delete(p);}catch(IOException e){throw new RuntimeException(e);}});}
        Files.delete(outside);
        System.out.println("PASS: volume case, root selection, traversal/symlink rejection and bounded game discovery");
    }
}
