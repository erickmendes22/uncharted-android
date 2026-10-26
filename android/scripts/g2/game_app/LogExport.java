package dev.unchartedg2.game;
import android.app.Activity;
import android.content.Intent;
import android.widget.Toast;
import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;
final class LogExport {
 static final int REQUEST=5100;
 static File source(Activity a){File root=a.getExternalFilesDir(null);if(root==null)root=a.getFilesDir();File f=new File(root,"uncharted.log");return f.isFile()?f:new File(root,"bb-probe.log");}
 static void pick(Activity a){if(!source(a).isFile()){Toast.makeText(a,"No game log yet.",Toast.LENGTH_LONG).show();return;}
  a.startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("text/plain").putExtra(Intent.EXTRA_TITLE,"Uncharted-"+new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.ROOT).format(new Date())+".log"),REQUEST);
 }
 static boolean result(Activity a,int request,int result,Intent intent){
  if(request!=REQUEST)return false;
  if(result!=Activity.RESULT_OK||intent==null||intent.getData()==null)return true;
  android.net.Uri uri=intent.getData();
  new Thread(()->{String message;try(InputStream in=new FileInputStream(source(a));OutputStream out=a.getContentResolver().openOutputStream(uri,"wt")){
   if(out==null)throw new IOException("Cannot open destination");byte[] buf=new byte[65536];for(int n;(n=in.read(buf))!=-1;)out.write(buf,0,n);message="Uncharted log exported.";
  }catch(Exception e){message="Log export failed: "+e.getMessage();}
  final String text=message;a.runOnUiThread(()->Toast.makeText(a,text,Toast.LENGTH_LONG).show());
  },"uncharted-log-export").start();return true;
 }
}
