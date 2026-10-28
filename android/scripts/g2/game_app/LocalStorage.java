package dev.unchartedg2.game;

import android.app.Activity;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.Os;
import android.system.OsConstants;
import java.io.File;
import java.io.IOException;

final class LocalStorage {
    static File resolve(Activity activity,Uri uri,boolean directory) throws Exception {
        if("com.android.externalstorage.documents".equals(uri.getAuthority())) {
            String id=directory?DocumentsContract.getTreeDocumentId(uri):DocumentsContract.getDocumentId(uri);
            return StoragePaths.external(id,Environment.getExternalStorageDirectory());
        }
        String path=null;
        try {String id=directory?DocumentsContract.getTreeDocumentId(uri):DocumentsContract.getDocumentId(uri);
            if(id.startsWith("raw:"))path=id.substring(4);
        }catch(Exception ignored){}
        if(path==null)try(android.database.Cursor c=activity.getContentResolver().query(uri,new String[]{"_data"},null,null,null)){
            if(c!=null&&c.moveToFirst()&&!c.isNull(0))path=c.getString(0);
        }catch(Exception ignored){}
        if(path!=null){File f=new File(path).getCanonicalFile();if(accessible(f,directory))return f;}
        // A tree URI is a container, not an openable document. OEM providers
        // sometimes expose only the document's descriptor.
        Uri document=directory?DocumentsContract.buildDocumentUriUsingTree(uri,DocumentsContract.getTreeDocumentId(uri)):uri;
        try(ParcelFileDescriptor fd=activity.getContentResolver().openFileDescriptor(document,"r")){
            if(fd!=null){File f=new File(Os.readlink("/proc/self/fd/"+fd.getFd())).getCanonicalFile();
                if(accessible(f,directory))return f;}
        }
        throw new IOException(AppLanguage.text("Select a local folder or PKG. Cloud files do not support direct installation."));
    }
    private static boolean accessible(File file,boolean directory){return file.canRead()&&(directory?file.isDirectory():file.isFile());}

    /** Retain the seekable provider descriptor for both helper invocations.
     * ProcessBuilder opens this parent-fd path in the parent, then redirects it
     * to child stdin. The extractor opens /proc/self/fd/0, so it never requires
     * proc access to another process or a duplicate multi-GB PKG. */
    static final class PkgSource implements AutoCloseable {
        final File file;
        private final ParcelFileDescriptor descriptor;
        private PkgSource(File f,ParcelFileDescriptor fd){file=f;descriptor=fd;}
        static PkgSource open(Activity activity,Uri uri) throws Exception {
            try {File f=resolve(activity,uri,false);if(accessible(f,false))return new PkgSource(f,null);}
            catch(Exception ignored){}
            ParcelFileDescriptor fd=activity.getContentResolver().openFileDescriptor(uri,"r");
            if(fd==null)throw new IOException(AppLanguage.text("Local PKG unavailable. Choose a file from device storage."));
            try {
                if(!OsConstants.S_ISREG(Os.fstat(fd.getFileDescriptor()).st_mode))throw new IOException(AppLanguage.text("PKG must support random access."));
                Os.lseek(fd.getFileDescriptor(),0,OsConstants.SEEK_CUR); // A pipe cannot feed the native extractor.
                File f=new File("/proc/"+Os.readlink("/proc/self")+"/fd/"+fd.getFd());
                return new PkgSource(f,fd); // Preserve proc path; canonicalizing would lose the granted handle.
            }catch(Exception e){fd.close();throw e;}
        }
        boolean descriptorBacked(){return descriptor!=null;}
        String argumentPath(){return descriptorBacked()?"/proc/self/fd/0":file.getPath();}
        public void close() throws IOException {if(descriptor!=null)descriptor.close();}
    }
}
