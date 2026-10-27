package dev.unchartedg2.game;
import android.app.Activity;
import android.content.Intent;
import android.widget.Toast;
import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;
final class LogExport {
 static final int REQUEST=5100;
 static File source(Activity a){File root=a.getExternalFilesDir(null);if(root==null)root=a.getFilesDir();File f=new File(root,"uncharted-daedalus.log");if(f.isFile())return f;f=new File(root,"uncharted.log");return f.isFile()?f:new File(root,"bb-probe.log");}
 static void pick(Activity a){if(!source(a).isFile()){Toast.makeText(a,AppLanguage.text("No game log yet."),Toast.LENGTH_LONG).show();return;}
  a.startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("text/plain").putExtra(Intent.EXTRA_TITLE,"Uncharted-Daedalus-"+new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.ROOT).format(new Date())+".log"),REQUEST);
 }
 // Read Android's own exit classification only when exporting, with no per-frame work.
 // This is explicitly the previous process, not a diagnosis of the running session.
 static String exitReason(int reason){
  switch(reason){
   case android.app.ApplicationExitInfo.REASON_CRASH_NATIVE:return "native_crash";
   case android.app.ApplicationExitInfo.REASON_CRASH:return "java_crash";
   case android.app.ApplicationExitInfo.REASON_LOW_MEMORY:return "low_memory";
   case android.app.ApplicationExitInfo.REASON_ANR:return "anr";
   case android.app.ApplicationExitInfo.REASON_SIGNALED:return "signal";
   case android.app.ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE:return "resource_limit";
   case android.app.ApplicationExitInfo.REASON_USER_REQUESTED:return "user_requested";
   case android.app.ApplicationExitInfo.REASON_USER_STOPPED:return "user_stopped";
   case android.app.ApplicationExitInfo.REASON_EXIT_SELF:return "self_exit";
   default:return "other";
  }
 }
 static String previousExit(Activity a){
  try{
   android.app.ActivityManager am=a.getSystemService(android.app.ActivityManager.class);
   if(am==null)return "";
   java.util.List<android.app.ApplicationExitInfo> exits=am.getHistoricalProcessExitReasons(a.getPackageName(),0,5);
   for(android.app.ApplicationExitInfo exit:exits){
    String name=exit.getProcessName();if(name!=null && name.endsWith(":restart"))continue;
    return "Previous process exit (Android): time="+new java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z",Locale.ROOT).format(new Date(exit.getTimestamp()))
     +" reason="+exitReason(exit.getReason())+"("+exit.getReason()+") status="+exit.getStatus()+" rss_kib="+exit.getRss()+" pss_kib="+exit.getPss()
     +" description="+exit.getDescription()+"\n--- Current session log ---\n";
   }
  }catch(RuntimeException unavailable){}
  return "";
 }
 static boolean result(Activity a,int request,int result,Intent intent){
  if(request!=REQUEST)return false;
  if(result!=Activity.RESULT_OK||intent==null||intent.getData()==null)return true;
  android.net.Uri uri=intent.getData();
  new Thread(()->{String message;try(InputStream in=new FileInputStream(source(a));OutputStream out=a.getContentResolver().openOutputStream(uri,"wt")){
   if(out==null)throw new IOException("Cannot open destination");
   out.write(previousExit(a).getBytes(java.nio.charset.StandardCharsets.UTF_8));byte[] buf=new byte[65536];for(int n;(n=in.read(buf))!=-1;)out.write(buf,0,n);message=AppLanguage.text("Log exported successfully.");
  }catch(Exception e){message=AppLanguage.text("Log export failed")+": "+e.getMessage();}
  final String text=message;a.runOnUiThread(()->Toast.makeText(a,text,Toast.LENGTH_LONG).show());
  },"uncharted-log-export").start();return true;
 }
}
