package dev.unchartedg2.game;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import java.util.Locale;
import org.json.JSONObject;

/** Shared language selection for launcher, installer and next native game session. */
final class AppLanguage {
    static final String[] CODES={"auto","en","pt","es","fr","de","it","ru","ja","ko","zh","tr","pl"};
    static final String[] LABELS={"Automatic","English","Português","Español","Français","Deutsch","Italiano","Русский","日本語","한국어","简体中文","Türkçe","Polski"};
    private static volatile String selected="en";
    private static JSONObject catalog=new JSONObject();
    static String resolve(String choice,Locale system) {
        String code="auto".equals(choice)?system.getLanguage():choice;
        for(int i=1;i<CODES.length;i++)if(CODES[i].equals(code))return code;
        return "en";
    }
    static Context attach(Context base) {
        String choice=base.getSharedPreferences("launcher",0).getString("app_language","auto");
        selected=resolve(choice,Resources.getSystem().getConfiguration().getLocales().get(0));
        try(java.io.InputStream in=base.getAssets().open("translations.json")) {
            catalog=new JSONObject(new String(in.readAllBytes(),java.nio.charset.StandardCharsets.UTF_8));
        } catch(Exception ignored) {catalog=new JSONObject();}
        Configuration cfg=new Configuration(base.getResources().getConfiguration());
        cfg.setLocale(Locale.forLanguageTag(selected.equals("zh")?"zh-CN":selected));
        return base.createConfigurationContext(cfg);
    }
    static String code(){return selected;}
    static String text(CharSequence raw) {
        if(raw==null)return "";
        String source=raw.toString();
        if(selected.equals("en"))return source;
        JSONObject row=catalog.optJSONObject(source);
        if(row!=null)return row.optString(selected,source);
        // Composite labels preserve their value (resolution, driver name, paths).
        int sep=source.indexOf(" · ");
        if(sep>=0)return text(source.substring(0,sep))+" · "+text(source.substring(sep+3));
        if(source.startsWith("FSR 3.1 "))return "FSR 3.1 "+text(source.substring(8));
        if(source.startsWith("FSR "))return "FSR "+text(source.substring(4));
        return source; // Unknown technical diagnostics retain their exact English meaning.
    }
    static String summary(Context a) {
        String choice=a.getSharedPreferences("launcher",0).getString("app_language","auto");
        int n=java.util.Arrays.asList(CODES).indexOf(choice);
        return text("Language")+" · "+(n<=0?text("Automatic"):LABELS[n]);
    }
    static void show(Activity a) {
        String choice=a.getSharedPreferences("launcher",0).getString("app_language","auto");
        int current=Math.max(0,java.util.Arrays.asList(CODES).indexOf(choice));
        String[] names=LABELS.clone();names[0]=text("Automatic");
        new android.app.AlertDialog.Builder(a).setTitle(text("Language"))
            .setSingleChoiceItems(names,current,(dialog,n)->{
                if(n!=current && a.getSharedPreferences("launcher",0).edit().putString("app_language",CODES[n]).commit()) {
                    dialog.dismiss();a.getWindow().getDecorView().post(()->{if(!a.isFinishing()&&!a.isDestroyed())a.recreate();});
                } else dialog.dismiss();
            }).setNegativeButton(text("Cancel"),null).show();
    }
}
