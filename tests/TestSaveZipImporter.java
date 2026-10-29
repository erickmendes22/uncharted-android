package dev.unchartedg2.game;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class TestSaveZipImporter {
    private static byte[] zip(String name, String content) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (ZipOutputStream z = new ZipOutputStream(out)) {
            z.putNextEntry(new ZipEntry(name));
            z.write(content.getBytes(StandardCharsets.UTF_8));
            z.closeEntry();
        }
        return out.toByteArray();
    }
    private static void check(boolean b, String what) { if (!b) throw new AssertionError(what); }
    public static void main(String[] args) throws Exception {
        Path app = Files.createTempDirectory("uncharted-save-tests-");
        Path user = Files.createDirectories(app.resolve("user/10000001"));
        Files.writeString(user.resolve("save.dat"), "original");
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try (ZipOutputStream z = new ZipOutputStream(output)) {
            check(SaveZipImporter.exportZip(app.resolve("user").toFile(), z) == 1, "export count");
        }
        check(SaveZipImporter.importZip(new ByteArrayInputStream(zip("user/10000001/save.dat", "imported")), app.toFile()) == 1, "import count");
        check(Files.readString(app.resolve("user/10000001/save.dat")).equals("imported"), "new save present");
        File[] backups = app.toFile().listFiles(f -> f.getName().startsWith("user-before-import-"));
        check(backups != null && backups.length == 1, "previous saves backed up");
        check(Files.readString(backups[0].toPath().resolve("10000001/save.dat")).equals("original"), "backup contents");
        for (String malicious : new String[]{"user/../../escaped", "other/filename", "user/../escaped", "user/C:\\foo"}) {
            try { SaveZipImporter.importZip(new ByteArrayInputStream(zip(malicious, "bad")), app.toFile()); throw new AssertionError("accepted: " + malicious); }
            catch (IOException expected) { }
            check(Files.readString(app.resolve("user/10000001/save.dat")).equals("imported"), "saves retained on invalid ZIP");
        }
        try { SaveZipImporter.importZip(new ByteArrayInputStream(new byte[]{1,2,3}), app.toFile()); throw new AssertionError("accepted invalid ZIP"); }
        catch (IOException expected) { }
        check(Files.readString(app.resolve("user/10000001/save.dat")).equals("imported"), "saves retained after corrupted ZIP");
        check(SaveZipImporter.importZip(new ByteArrayInputStream(output.toByteArray()), app.toFile()) == 1, "export/import round trip");
        check(Files.readString(app.resolve("user/10000001/save.dat")).equals("original"), "roundtrip data preserved");
        System.out.println("PASS: ZIP save import/export, old-save backup, rollback, path safety, invalid archive");
    }
}
