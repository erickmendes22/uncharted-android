package dev.unchartedg2.game;

import java.io.File;
import java.io.IOException;

/** Shared by the folder picker and PKG installer; no display-name path guessing. */
final class StoragePaths {
    static File external(String id, File primary) throws IOException {
        int at=id.indexOf(':');
        if(at<1)throw new IOException("Select local storage.");
        String volume=id.substring(0,at);
        if(!volume.matches("[A-Za-z0-9_-]+"))throw new IOException("Invalid volume.");
        File root="primary".equalsIgnoreCase(volume)?primary:new File("/storage",volume);
        File result=new File(root,id.substring(at+1)).getCanonicalFile();
        if(!result.toPath().startsWith(root.getCanonicalFile().toPath()))throw new IOException("Invalid path.");
        return result;
    }
    static File gameFolder(File selected) throws IOException {
        File root=selected.getCanonicalFile();
        if(game(root))return root;
        // Same bounded nested-folder discovery as GoW3. Do not recursively scan storage.
        String[] names={"Uncharted The Nathan Drake Collection","CUSA02320","CUSA02343","CUSA02344","CUSA01399","CUSA02826"};
        for(String name:names){File child=new File(root,name).getCanonicalFile();
            if(child.toPath().startsWith(root.toPath())&&game(child))return child;}
        return root;
    }
    private static boolean game(File root){return new File(root,"eboot.bin").isFile()&&new File(root,"u1data").isDirectory();}
}
