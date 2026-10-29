package dev.unchartedg2.game;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/** Standalone, Android-API-independent save archive handling; testable on the host JDK.
 * Export archives use user/<relative path>. Import rejects foreign paths, validates and
 * unpacks entirely into staging, then atomically swaps directories on the same filesystem.
 * The previous user directory stays beside it as user-before-import-* for recovery. */
final class SaveZipImporter {
    private static final long MAX_UNCOMPRESSED_BYTES = 2L * 1024 * 1024 * 1024;
    private static final int MAX_FILES = 8192;
    private SaveZipImporter() {}

    static int exportZip(File user, ZipOutputStream zip) throws IOException {
        if (!user.isDirectory()) throw new IOException("No save directory found");
        return exportTree(zip, user, user);
    }

    private static int exportTree(ZipOutputStream zip, File root, File current) throws IOException {
        if (current.isDirectory()) {
            File[] children = current.listFiles();
            if (children == null) throw new IOException("Cannot read " + current.getName());
            int count = 0;
            for (File child : children) count += exportTree(zip, root, child);
            return count;
        }
        if (!current.isFile()) throw new IOException("Cannot read " + current.getName());
        String relative = root.toPath().relativize(current.toPath()).toString().replace(File.separatorChar, '/');
        validateRelative(relative);
        zip.putNextEntry(new ZipEntry("user/" + relative));
        try (InputStream in = new FileInputStream(current)) {
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) != -1;) zip.write(buffer, 0, n);
        }
        zip.closeEntry();
        return 1;
    }

    /** Imports a ZIP produced by exportZip. Invalid or partial archives never replace saves. */
    static int importZip(InputStream input, File appFiles) throws IOException {
        if (!appFiles.isDirectory()) throw new IOException("Save storage is not accessible");
        File staging = File.createTempFile("user-import-", ".tmp", appFiles);
        if (!staging.delete() || !staging.mkdir()) throw new IOException("Cannot create save staging directory");
        try {
            int count = 0;
            long total = 0;
            Path base = staging.toPath().toAbsolutePath().normalize();
            Set<String> names = new HashSet<>();
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(input))) {
                for (ZipEntry entry; (entry = zip.getNextEntry()) != null;) {
                    String name = entry.getName();
                    if ("user/".equals(name)) { zip.closeEntry(); continue; }
                    if (!name.startsWith("user/")) throw new IOException("ZIP contains an unexpected save path");
                    String relative = name.substring("user/".length());
                    if (entry.isDirectory() && relative.endsWith("/")) relative = relative.substring(0, relative.length()-1);
                    validateRelative(relative);
                    if (!names.add(relative)) throw new IOException("Duplicate save path in ZIP");
                    Path to = base.resolve(relative).normalize();
                    if (!to.startsWith(base) || to.equals(base)) throw new IOException("Unsafe ZIP path");
                    if (entry.isDirectory()) {
                        Files.createDirectories(to);
                    } else {
                        if (++count > MAX_FILES) throw new IOException("Too many files in save ZIP");
                        Files.createDirectories(to.getParent());
                        try (OutputStream out = new FileOutputStream(to.toFile())) {
                            byte[] buffer = new byte[65536];
                            for (int n; (n = zip.read(buffer)) != -1;) {
                                total += n;
                                if (total > MAX_UNCOMPRESSED_BYTES) throw new IOException("Save ZIP exceeds size limit");
                                out.write(buffer, 0, n);
                            }
                        }
                    }
                    zip.closeEntry(); // Verifies the entry checksum before touching live saves.
                }
            }
            if (count == 0) throw new IOException("ZIP does not contain saved games");
            replaceSaves(appFiles, staging);
            return count;
        } finally {
            deleteTree(staging);
        }
    }

    private static void validateRelative(String path) throws IOException {
        if (path.isEmpty() || path.startsWith("/") || path.indexOf('\\') >= 0 ||
            path.indexOf(':') >= 0 || path.indexOf('\0') >= 0) throw new IOException("Unsafe ZIP path");
        for (String segment : path.split("/", -1))
            if (segment.isEmpty() || ".".equals(segment) || "..".equals(segment))
                throw new IOException("Unsafe ZIP path");
    }

    private static void replaceSaves(File appFiles, File staged) throws IOException {
        File current = new File(appFiles, "user");
        if (current.exists() && !current.isDirectory()) throw new IOException("Invalid current save directory");
        File backup = null;
        if (current.exists()) {
            for (int n = 0; n < 100; n++) {
                File candidate = new File(appFiles, "user-before-import-" + System.currentTimeMillis() + "-" + n);
                if (!candidate.exists()) { backup = candidate; break; }
            }
            if (backup == null) throw new IOException("Cannot allocate save backup");
            Files.move(current.toPath(), backup.toPath());
        }
        try {
            Files.move(staged.toPath(), current.toPath());
        } catch (IOException e) {
            if (backup != null) {
                try { Files.move(backup.toPath(), current.toPath()); }
                catch (IOException rollbackError) { e.addSuppressed(rollbackError); }
            }
            throw e;
        }
    }

    private static void deleteTree(File file) throws IOException {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot list staging files");
            for (File child : children) deleteTree(child);
        }
        if (!file.delete() && file.exists()) throw new IOException("Cannot remove temporary save file");
    }
}
