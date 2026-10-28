package dev.unchartedg2.game;

import android.app.*;
import android.content.*;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;
import java.io.*;
import java.util.regex.*;

/** Adapted from iss_android experimental-bionic BionicPkgInstaller.
 * orbis-pkg-util, installed directly into the selected local directory. */
final class PkgInstaller {
 static final int BASE=20, UPDATE=21, DESTINATION=22;
 interface State {void set(boolean busy,String message);}
 private final Activity activity;private final State state;
 private Uri selected;private boolean update;private volatile boolean running;
 PkgInstaller(Activity a,State s){activity=a;state=s;}
 boolean busy(){return running||selected!=null;}
 void pick(boolean replacing){
  if(busy())return;
  if(replacing&&!new File(game(),"eboot.bin").isFile()){state.set(false,text("Select the base game before installing an update.", "Selecione o jogo base antes da atualização."));return;}
  update=replacing;
  activity.startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION),replacing?UPDATE:BASE);
 }
 private static String text(String en,String pt){return AppLanguage.text(en);}
 private File game(){return new File(activity.getSharedPreferences("launcher",0).getString("game_dir",""));}
 boolean result(int request,int result,Intent data){
  if(request!=BASE&&request!=UPDATE&&request!=DESTINATION)return false;
  if(result!=Activity.RESULT_OK||data==null||data.getData()==null){selected=null;state.set(false,text("Installation canceled.", "Instalação cancelada."));return true;}
  try{
   if(request==BASE||request==UPDATE){selected=data.getData();update=request==UPDATE;
    if(update){new AlertDialog.Builder(activity).setTitle(text("Install update?", "Instalar atualização?")).setMessage(text("Matching files will be replaced directly in the game folder. TITLE_ID will be validated before extraction.", "Os arquivos correspondentes serão substituídos diretamente na pasta do jogo. TITLE_ID será validado antes da extração.")).setNegativeButton(text("Cancel", "Cancelar"),(d,w)->{selected=null;state.set(false,text("Installation canceled.", "Instalação cancelada."));}).setPositiveButton(text("Install", "Instalar"),(d,w)->install(Uri.fromFile(game()))).setOnCancelListener(d->{selected=null;state.set(false,text("Installation canceled.", "Instalação cancelada."));}).show();}
    else {
     Intent choose=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
       .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
     activity.startActivityForResult(choose,DESTINATION);
     state.set(false,text("Choose a parent folder for Uncharted The Nathan Drake Collection.", "Escolha uma pasta onde Uncharted The Nathan Drake Collection será criada."));
    }
   }else{
    Uri dest=data.getData();try{activity.getContentResolver().takePersistableUriPermission(dest,data.getFlags()&(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION));}catch(SecurityException ignored){}
    // The selected tree is a parent; never install directly into it.
    install(dest);
   }
  }catch(Exception e){selected=null;state.set(false,e.getMessage());}return true;
 }
 private void install(Uri destination){
  if(running||selected==null)return;final Uri source=selected;selected=null;running=true;state.set(true,text("Preparing PKG installation…", "Preparando instalação PKG…"));
  new Thread(()->{String message;
   try(LocalStorage.PkgSource input=LocalStorage.PkgSource.open(activity,source)){
    File pkg=input.file,dest=local(destination,true);
    if(!update){
     File parent=dest;
     if(!parent.isDirectory())throw new IOException(text("The selected destination is not a folder.", "O destino selecionado não é uma pasta."));
     dest=new File(parent,"Uncharted The Nathan Drake Collection").getCanonicalFile();
     if(!dest.toPath().startsWith(parent.getCanonicalFile().toPath()))
      throw new IOException("Invalid destination path.");
    }
    if(!update&&!dest.exists()&&!dest.mkdirs())throw new IOException(text("Could not create Uncharted The Nathan Drake Collection directory at: ", "Não foi possível criar a pasta Uncharted The Nathan Drake Collection em: ")+dest.getPath()+text(". External SD/USB storage may require SAF-native extraction.", ". Cartão SD/USB pode exigir extração compatível com SAF."));
    if(!pkg.isFile()||!pkg.canRead())throw new IOException(text("Local PKG unavailable. Choose a file from device storage.", "PKG local indisponível. Escolha um arquivo no armazenamento do aparelho."));
    if(!dest.isDirectory()||!dest.canWrite())throw new IOException(text("No write permission for the selected folder.", "Sem permissão de escrita na pasta escolhida."));
    File[] entries=dest.listFiles();
    if(entries==null)throw new IOException(text("Could not list the destination folder.", "Não foi possível listar a pasta de destino."));
    if(!update&&entries.length!=0){
     // Never extract a base package over previous files, including incomplete installs.
     File parent=dest.getParentFile();String name=dest.getName();
     File free=null;
     for(int index=2;index<1000;index++){
      File candidate=new File(parent,name+" ("+index+")");
      if(!candidate.exists()){
       if(!candidate.mkdir())throw new IOException(text("Could not create a fresh game folder: ","Não foi possível criar uma nova pasta do jogo: ")+candidate);
       free=candidate.getCanonicalFile();break;
      }
     }
     if(free==null)throw new IOException(text("No free installation folder name.", "Não há nome de pasta livre para instalação."));
     dest=free;
    }
    if(!dest.isDirectory()||!dest.canWrite())throw new IOException(text("No write permission for the selected folder.", "Sem permissão de escrita na pasta escolhida."));
    // Test real file creation AFTER deciding the destination; do not mistake the probe for game content.
    File probe=new File(dest,".uncharted-pkg-write-test");
    try(FileOutputStream stream=new FileOutputStream(probe)){stream.write(1);stream.getFD().sync();}
    catch(IOException e){throw new IOException(text("Cannot create files in destination: ","Não é possível criar arquivos no destino: ")+dest.getPath(),e);}
    finally{if(probe.exists()&&!probe.delete())throw new IOException("Cannot remove destination write test: "+probe.getPath());}
    stateOnUi(true,text("Destination ready: ","Pasta de instalação pronta: ")+dest.getPath());
    if(!update&&pkg.toPath().startsWith(dest.toPath()))throw new IOException(text("The PKG must be outside the destination folder.", "O PKG deve ficar fora da pasta de destino."));
    try(DataInputStream in=new DataInputStream(new FileInputStream(pkg))){if(in.readInt()!=0x7f434e54)throw new IOException(text("File is not a PS4 PKG.", "Arquivo não é um PKG de PS4."));}
    File helper=new File(activity.getApplicationInfo().nativeLibraryDir,"liborbis_pkg.so");
    String info=execute(helper,dest,input,"info",input.argumentPath());
    Matcher title=Pattern.compile("(?im)^\\s*Title ID:\\s*(CUSA[0-9]{5})\\s*$").matcher(info);
    if(!title.find())throw new IOException(text("TITLE_ID missing from PKG header.", "TITLE_ID ausente no cabeçalho PKG."));String id=title.group(1).toUpperCase(java.util.Locale.US);
    if(!java.util.Arrays.asList("CUSA02320","CUSA02343","CUSA02344","CUSA01399","CUSA02826").contains(id))throw new IOException("The PKG is not a supported Nathan Drake Collection title.");
    if(update&&(!dest.equals(game().getCanonicalFile())||!id.equals(Launcher.sfoValue(dest.getPath(),"TITLE_ID"))))throw new IOException(text("The update does not match the selected game.", "A atualização não corresponde ao jogo selecionado."));
    stateOnUi(true,text("Extracting PKG directly into the selected folder…", "Extraindo PKG diretamente na pasta escolhida…"));
    if(update)execute(helper,dest,input,"extract",input.argumentPath(),"--output",dest.getPath(),"--force");
    else execute(helper,dest,input,"extract",input.argumentPath(),"--output",dest.getPath());
    if(!new File(dest,"eboot.bin").isFile()||!new File(dest,"u1data").isDirectory()||!id.equals(Launcher.sfoValue(dest.getPath(),"TITLE_ID")))throw new IOException(text("Extraction does not contain a complete Uncharted Collection dump. Check eboot.bin, u1data/ and sce_sys/param.sfo.", "Extração não contém um dump completo de Uncharted Collection. Confira eboot.bin, u1data/ e sce_sys/param.sfo."));
    if(!activity.getSharedPreferences("launcher",0).edit().putString("game_dir",dest.getPath()).commit())throw new IOException(text("Content extracted, but the folder could not be selected.", "Conteúdo extraído, mas não foi possível selecionar a pasta."));
    message=(update?text("Update", "Atualização"):text("Game", "Jogo"))+" "+id+text(" installed at ", " instalado em ")+dest.getPath();
   }catch(Exception e){message=text("Installation failed: ", "Falha na instalação: ")+e.getMessage();}
   running=false;stateOnUi(false,message);
  },"uncharted-pkg-install").start();
 }
 private void stateOnUi(boolean busy,String message){activity.runOnUiThread(()->{if(!activity.isFinishing()&&!activity.isDestroyed())state.set(busy,message);});}
 private String execute(File helper,File dest,LocalStorage.PkgSource input,String... args)throws Exception{
  java.util.ArrayList<String> command=new java.util.ArrayList<>();command.add("/system/bin/linker64");command.add(helper.getPath());java.util.Collections.addAll(command,args);
  ProcessBuilder builder=new ProcessBuilder(command).directory(dest).redirectErrorStream(true);
  if(input.descriptorBacked())builder.redirectInput(input.file);
  builder.environment().put("LD_LIBRARY_PATH",activity.getApplicationInfo().nativeLibraryDir);
  Process process=builder.start();
  final StringBuilder log=new StringBuilder();
  final java.util.concurrent.atomic.AtomicReference<String> progress=new java.util.concurrent.atomic.AtomicReference<>("");
  final java.util.concurrent.atomic.AtomicReference<IOException> readFailure=new java.util.concurrent.atomic.AtomicReference<>();
  Thread drain=new Thread(()->{
   try(Reader reader=new InputStreamReader(process.getInputStream())){
    char[] buffer=new char[4096];StringBuilder line=new StringBuilder();int n;
    while((n=reader.read(buffer))!=-1){
     for(int i=0;i<n;i++){
      char c=buffer[i];
      if(c=='\r'||c=='\n'){
       if(line.length()>0){
        String value=line.toString();progress.set(value);
        synchronized(log){if(log.length()>32000)log.delete(0,log.length()-16000);log.append(value).append('\n');}
        line.setLength(0);
       }
      }else if(line.length()<4096)line.append(c);
     }
    }
    if(line.length()>0){String value=line.toString();progress.set(value);synchronized(log){log.append(value).append('\n');}}
   }catch(IOException e){readFailure.set(e);}
  },"uncharted-pkg-output");
  drain.setDaemon(true);drain.start();
  long start=android.os.SystemClock.elapsedRealtime();
  boolean extracting=args.length>0&&"extract".equals(args[0]);
  try{
   while(!process.waitFor(3,java.util.concurrent.TimeUnit.SECONDS)){
    if(extracting){
     long elapsed=(android.os.SystemClock.elapsedRealtime()-start)/1000;
     String line=progress.get();if(line.length()>140)line=line.substring(line.length()-140);
     String msg=text("Extracting PKG", "Extraindo PKG")+" ("+(elapsed/60)+"m "+(elapsed%60)+"s)";
     if(!line.isEmpty())msg+="\n"+line;
     stateOnUi(true,msg);
    }
   }
   drain.join(5000);
   int exit=process.exitValue();
   String output; synchronized(log){output=log.toString();}
   if(exit!=0)throw new IOException(text("PKG extractor (","Extrator PKG (")+exit+"): "+output.substring(Math.max(0,output.length()-1800)));
   if(readFailure.get()!=null)throw readFailure.get();
   return output;
  }catch(InterruptedException e){process.destroyForcibly();Thread.currentThread().interrupt();throw new IOException("PKG extraction interrupted",e);}
 }
 private File local(Uri uri,boolean directory)throws Exception{
  if(directory&&"file".equals(uri.getScheme())){File file=new File(uri.getPath()).getCanonicalFile();if(file.equals(game().getCanonicalFile()))return file;throw new IOException(text("Invalid destination.", "Destino inválido."));}
  return LocalStorage.resolve(activity,uri,directory);
 }
}
