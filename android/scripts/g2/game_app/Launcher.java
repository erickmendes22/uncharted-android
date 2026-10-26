package dev.unchartedg2.game;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.system.Os;
import android.system.OsConstants;
import android.text.InputType;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import org.json.JSONArray;
import org.json.JSONObject;

/** The app's first screen: the folder with the user's own decrypted game (a PS4 dump of
 *  Uncharted: The Nathan Drake Collection), the loader's files made from it on the device
 *  (libbbprepare.so: the port's scripts/prepare.py and link_modules.py in C), a few settings,
 *  then GameActivity with files/args*.txt and env*.txt written from them. Nothing of the game
 *  ships with the app: the patch presets are checked against the user's executables first.
 *  adb automation: am start -n <app>/dev.unchartedg2.game.Launcher --es game_dir <path> [--ez backup_saves true]
 *  [--es driver turnip|system|pkg:<name>] [--es driver_zip <path>: install and select that package]
 *  [--es extra_env "KEY=VALUE\\nKEY=VALUE": the extra environment field]
 *  [--ez prepare true] [--ez play true]. */
public class Launcher extends Activity {
    static {
        System.loadLibrary("bbprepare");
    }

    /** null on success, else the reason (game_prepare.c). */
    static native String prepareGame(String gameDir, String exe, String outDir);

    /** A value of sce_sys/param.sfo as text, null if missing. */
    static native String sfoValue(String gameDir, String key);
    static native long gpuChipId();

    /** The three games: id, executable, folder under files/ ("" = files/ itself), the
     *  resolution recipe asset (scripts/g2/recipe_from_preset.py). */
    private static final String[][] GAMES = {
        {"u1", "eboot.bin", "", "recipes/u1.json"},
        {"u2", "big2-ps4_Shipping.elf", "u2", "recipes/u2.json"},
        {"u3", "big3-ps4_Shipping.elf", "u3", "recipes/u3.json"},
    };
    /** The sizes the game can render at: its own 1920x1080 and three below it, made by the
     *  recipes' byte patches (the display configuration's size pairs and the light tile grid). */
    private static final String[][] RESOLUTIONS = {
        {"640x360", "640x360 (fastest, a soft picture)"},
        {"832x468", "832x468 (Bachata patch)"},
        {"960x540", "960x540"},
        {"1280x720", "1280x720 (recommended)"},
        {"1920x1080", "1920x1080, the game's own (much slower)"},
    };
    private static final String[] MODULES = {"sce_module/libc.prx", "sce_module/libSceFios2.prx", "sce_sys/param.sfo"};
    private static final int PICK_FOLDER = 1, PICK_DRIVER = 2, PICK_LOSSLESS = 3;
    /** Where driver packages are downloaded from: the GitHub releases of the community Turnip
     *  builds (the zips the Android emulators share: meta.json and the driver library). */
    private static final String[] DRIVER_SOURCES = {
        "K11MCH1/AdrenoToolsDrivers", "MrPurple666/purple-turnip", "StevenMXZ/Adreno-Tools-Drivers",
    };

    private SharedPreferences prefs;
    private File files;
    private TextView folderText, folderInfo, prepareInfo, statusText;
    private EditText pathEdit, extraEnv;
    private Button accessButton, prepareButton, playButton;
    private RadioGroup resolution;
    private RadioGroup fpsLimit;
    private RadioGroup touchPad;
    private CheckBox showFps, uncapped;
    private LinearLayout driverList;
    private RadioGroup drivers;
    private volatile boolean busy;
    private long gpuChip;
    private PkgInstaller pkgInstaller;
    private int pendingPkg=-1;

    @Override protected void attachBaseContext(android.content.Context base) {super.attachBaseContext(AppLanguage.attach(base));}

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        gpuChip = gpuChipId();
        // The home screen icon while a game runs: back to it, not a new menu. GameActivity is a
        // task of its own (singleInstance; after a restart into U2/U3 too) and this activity
        // finishes when it starts the game, so the icon found no task of the launcher to resume.
        Intent start = getIntent();
        if (Intent.ACTION_MAIN.equals(start.getAction()) && start.hasCategory(Intent.CATEGORY_LAUNCHER)
                && resumeGame()) {
            finish();
            return;
        }
        prefs = getSharedPreferences("launcher", MODE_PRIVATE);
        // Scripted tests (adb shell am start ... --es resolution 960x540 --es fps_limit 30): the
        // same settings a finger sets, stored before the views read them.
        if (getIntent().hasExtra("resolution")) prefs.edit().putString("resolution", getIntent().getStringExtra("resolution")).apply();
        if (getIntent().hasExtra("fps_limit")) prefs.edit().putString("fps_limit", getIntent().getStringExtra("fps_limit")).apply();
        if (getIntent().hasExtra("touch_pad")) prefs.edit().putString("touch_pad", getIntent().getStringExtra("touch_pad")).apply();
        if (getIntent().hasExtra("driver")) prefs.edit().putString("driver", getIntent().getStringExtra("driver")).apply();
        if (getIntent().hasExtra("extra_env")) // "\\n" between the lines (adb passes one string)
            prefs.edit().putString("extra_env", getIntent().getStringExtra("extra_env").replace("\\n", "\n")).apply();
        if (getIntent().hasExtra("driver_zip")) {
            try (InputStream in = new java.io.FileInputStream(getIntent().getStringExtra("driver_zip"))) {
                prefs.edit().putString("driver", "pkg:" + installDriver(in, "driver")).apply();
            } catch (IOException e) {
                android.util.Log.w("Launcher", "driver_zip: " + e.getMessage());
            }
        }
        File d = getExternalFilesDir(null);
        files = d != null ? d : getFilesDir();
        pkgInstaller = new PkgInstaller(this,(active,message)->{busy=active;status(message);refresh();});
        if(!prefs.getBoolean("ui_v027",false)) {
            prefs.edit().putBoolean("ui_v027",true).putInt("mobile_upscale",0).apply();
            iniPut("touch_mobile","0");iniPut("hud_scale","1.25");iniPut("upscaler","off");
        }
        if(!prefs.getBoolean("ui_v028",false)) {
            prefs.edit().putBoolean("ui_v028",true).apply();
            iniPut("hud_position","0");iniPut("hud_scale","1.0");
        }
        if(prefs.getInt("mobile_upscale",0)==5)prefs.edit().putInt("mobile_upscale",4).apply();
        buildViews();
        handle(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handle(intent);
    }

    /** Brings the running game's task to the front; false when no game runs. */
    private boolean resumeGame() {
        ActivityManager am = getSystemService(ActivityManager.class);
        if (am == null) return false;
        for (ActivityManager.AppTask task : am.getAppTasks()) {
            try {
                ActivityManager.RecentTaskInfo info = task.getTaskInfo();
                if (info.isRunning && info.baseActivity != null
                        && GameActivity.class.getName().equals(info.baseActivity.getClassName())) {
                    task.moveToFront();
                    return true;
                }
            } catch (IllegalArgumentException gone) {
                // the task ended meanwhile
            }
        }
        return false;
    }

    /** The adb automation extras: game_dir, backup_saves, prepare, play. */
    private void handle(Intent intent) {
        String dir = intent.getStringExtra("game_dir");
        if (dir != null) setFolder(dir);
        refresh();
        if (busy) return;
        if (intent.getBooleanExtra("backup_saves", false)) {
            String done = backupSaves();
            status(done != null ? "Saves copied to " + done : "Backing up failed.");
        }
        if (intent.getBooleanExtra("prepare", false)) prepare(intent.getBooleanExtra("play", false));
        else if (intent.getBooleanExtra("play", false) && prepared()) play();
    }

    @Override
    protected void onResume() {
        super.onResume();
        refresh(); // back from permissions
        if(pendingPkg>=0 && Environment.isExternalStorageManager()){boolean update=pendingPkg==1;pendingPkg=-1;pkgInstaller.pick(update);}
    }

    // ---- views ----
    // Plain widgets styled by hand (no AndroidX in this build): the theme's accent colour, sections
    // as rounded cards, a filled Play button, outlined secondary buttons, muted explanations.

    private int accent, muted, cardColor;

    private void initPalette() {
        accent = 0xFFCBA66B;
        muted = 0xFFB9AC94;
        cardColor = 0xEF191D1A;
        getWindow().setStatusBarColor(0xFF101511);
        getWindow().setNavigationBarColor(0xFF101511);
    }

    private TextView text(LinearLayout parent, String s, float size, boolean bold) {
        TextView t = new TextView(this);
        t.setText(AppLanguage.text(s));
        t.setTextSize(size);
        t.setTextColor(0xFFF4EFDF);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setPadding(0, dp(6), 0, dp(2));
        parent.addView(t);
        return t;
    }

    private TextView note(LinearLayout parent, String s) {
        TextView t = text(parent, s, 14, false);
        t.setTextColor(muted);
        t.setLineSpacing(0, 1.1f);
        return t;
    }

    private LinearLayout card(LinearLayout parent, String title) {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(cardColor);
        bg.setStroke(dp(1), 0x555F553A);
        bg.setCornerRadius(dp(16));
        c.setBackground(bg);
        c.setPadding(dp(18), dp(12), dp(18), dp(16));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                                                     ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(12);
        parent.addView(c, lp);
        if (title != null) {
            TextView heading=text(c,title,15,true);heading.setTextColor(accent);
            int kind=title.contains("CONTROL")?5:title.contains("DISPLAY")||title.contains("RESOLUTION")?1:title.contains("GAME")?0:2;
            ExpeditionIcon icon=new ExpeditionIcon(kind,accent);icon.setBounds(0,0,dp(20),dp(20));
            heading.setCompoundDrawables(icon,null,null,null);heading.setCompoundDrawablePadding(dp(10));
            heading.setPadding(0,0,0,dp(8));
        }
        return c;
    }

    private LinearLayout row(LinearLayout parent) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(getResources().getConfiguration().screenWidthDp < 600 ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        parent.addView(r);
        return r;
    }

    private Button button(LinearLayout parent, String s, View.OnClickListener l) {
        Button b = new Button(this, null, android.R.attr.borderlessButtonStyle);
        b.setText(AppLanguage.text(s));
        int iconKind=s.toLowerCase(Locale.ROOT).contains("folder")?4:s.toLowerCase(Locale.ROOT).contains("game")?5:s.toLowerCase(Locale.ROOT).contains("upscale")?1:2;
        ExpeditionIcon icon=new ExpeditionIcon(iconKind,accent);icon.setBounds(0,0,dp(22),dp(22));
        b.setCompoundDrawables(icon,null,null,null);b.setCompoundDrawablePadding(dp(10));
        b.setAllCaps(false);
        b.setOnClickListener(l);
        b.setTextColor(new ColorStateList(new int[][] {{-android.R.attr.state_enabled}, {}},
                                          new int[] {muted, accent}));
        b.setMinHeight(dp(52));
        b.setMinimumHeight(dp(52));
        b.setPadding(dp(18), 0, dp(18), 0);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.TRANSPARENT);
        bg.setStroke(dp(1), (accent & 0x00FFFFFF) | 0x66000000);
        bg.setCornerRadius(dp(22));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf((accent & 0x00FFFFFF) | 0x33000000), bg, null));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(parent.getOrientation() == LinearLayout.HORIZONTAL ? 0 : ViewGroup.LayoutParams.MATCH_PARENT,
                                                                     ViewGroup.LayoutParams.WRAP_CONTENT);
        if (parent.getOrientation() == LinearLayout.HORIZONTAL) lp.weight = 1;
        b.setMinWidth(0); b.setMinimumWidth(0);
        lp.topMargin = dp(8);
        lp.rightMargin = dp(8);
        parent.addView(b, lp);
        return b;
    }

    private Button primaryButton(LinearLayout parent, String s, View.OnClickListener l) {
        Button b = new Button(this, null, android.R.attr.borderlessButtonStyle);
        b.setText(AppLanguage.text(s));
        b.setAllCaps(false);
        b.setOnClickListener(l);
        ExpeditionIcon playIcon=new ExpeditionIcon(6,0xFF181910);playIcon.setBounds(0,0,dp(24),dp(24));
        b.setCompoundDrawables(playIcon,null,null,null);b.setCompoundDrawablePadding(dp(12));
        b.setTextSize(18);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setTextColor(0xFF181910);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(new ColorStateList(new int[][] {{-android.R.attr.state_enabled}, {}},
                                       new int[] {(accent & 0x00FFFFFF) | 0x59000000, accent}));
        bg.setCornerRadius(dp(28));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), bg, null));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(56));
        lp.topMargin = dp(14);
        parent.addView(b, lp);
        return b;
    }

    private CheckBox check(LinearLayout parent, String s, String key, boolean byDefault) {
        CheckBox c = new CheckBox(this);
        c.setText(AppLanguage.text(s));
        c.setChecked(prefs.getBoolean(key, byDefault));
        c.setOnCheckedChangeListener((v, on) -> prefs.edit().putBoolean(key, on).apply());
        parent.addView(c);
        return c;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private void buildViews() {
        initPalette();
        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(LinearLayout.VERTICAL);
        shell.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
            new int[] {0xFF26342A, 0xFF111713, 0xFF1D1913}));
        TextView brand = text(shell, "UNCHARTED", 29, true);
        brand.setTypeface(Typeface.create("serif", Typeface.BOLD));
        brand.setLetterSpacing(0.08f);
        brand.setPadding(dp(22), dp(14), dp(16), 0);
        TextView subtitle = note(shell, "THE NATHAN DRAKE COLLECTION  ·  DAEDALUS");
        subtitle.setTextSize(11);
        subtitle.setPadding(dp(22), 0, dp(16), dp(8));
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(20), dp(20), dp(28));
        scroll.addView(root);

        LinearLayout[] pages = new LinearLayout[4];
        for (int n = 0; n < pages.length; n++) {
            pages[n] = new LinearLayout(this);
            pages[n].setOrientation(LinearLayout.VERTICAL);
            root.addView(pages[n]);
            pages[n].setVisibility(n == 0 ? View.VISIBLE : View.GONE);
        }
        LinearLayout home = pages[0], patches = pages[1], config = pages[2], about = pages[3];
        android.widget.FrameLayout hero = new android.widget.FrameLayout(this);
        android.widget.ImageView artwork = new android.widget.ImageView(this);
        artwork.setScaleType(android.widget.ImageView.ScaleType.CENTER_CROP);
        try(java.io.InputStream in=getAssets().open("art/hero.png")) {
            android.graphics.BitmapFactory.Options options=new android.graphics.BitmapFactory.Options();options.inSampleSize=2;
            artwork.setImageBitmap(android.graphics.BitmapFactory.decodeStream(in,null,options));
        } catch(Exception ignored) {}
        hero.addView(artwork,new android.widget.FrameLayout.LayoutParams(-1,-1));
        TextView heroCaption=new TextView(this);heroCaption.setText(AppLanguage.text("THE NATHAN DRAKE COLLECTION\nDAEDALUS ANDROID"));
        heroCaption.setTextColor(0xFFF3DCA6);heroCaption.setTextSize(15);heroCaption.setTypeface(Typeface.DEFAULT_BOLD);
        heroCaption.setGravity(android.view.Gravity.BOTTOM);heroCaption.setPadding(dp(18),dp(16),dp(18),dp(16));
        heroCaption.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,new int[]{0x0015110C,0xE615110C}));
        hero.addView(heroCaption,new android.widget.FrameLayout.LayoutParams(-1,-1));
        home.addView(hero,new LinearLayout.LayoutParams(-1,dp(210)));
        LinearLayout games = card(home, "CHOOSE YOUR ADVENTURE");
        RadioGroup gameSelect = new RadioGroup(this);
        String[] names = {"UNCHARTED  ·  Drake's Fortune", "UNCHARTED 2  ·  Among Thieves", "UNCHARTED 3  ·  Drake's Deception"};
        for (int n = 0; n < GAMES.length; n++) {
            RadioButton game = new RadioButton(this);
            game.setId(View.generateViewId()); game.setTag(GAMES[n][0]);
            game.setText(AppLanguage.text(names[n])); game.setMinHeight(dp(56));
            gameSelect.addView(game);
            if (GAMES[n][0].equals(prefs.getString("selected_game", "u1"))) gameSelect.check(game.getId());
        }
        gameSelect.setOnCheckedChangeListener((group, id) -> {
            RadioButton game = group.findViewById(id);
            if (game != null) prefs.edit().putString("selected_game", (String)game.getTag()).apply();
        });
        games.addView(gameSelect);
        playButton = primaryButton(games, "CONTINUE ADVENTURE", v -> play());
        button(games,"Choose game folder",v->{startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE),PICK_FOLDER);});
        button(games,"Install game PKG",v->installPkg(false));
        button(games,"Install update PKG",v->installPkg(true));
        statusText = note(shell, "");
        statusText.setPadding(dp(22), 0, dp(22), dp(4));
        root = home;
        LinearLayout folder = card(config, "GAME INSTALLATION");
        folderText = text(folder, "", 15, false);
        folderInfo = note(folder, "");
        LinearLayout row = row(folder);
        button(row, "Choose folder", v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            startActivityForResult(i, PICK_FOLDER);
        });
        accessButton = button(row, "Allow access to all files", v -> {
            Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                  Uri.parse("package:" + getPackageName()));
            startActivity(i);
        });
        pathEdit = new EditText(this);
        pathEdit.setHint(AppLanguage.text("or type the path, e.g. /storage/XXXX-XXXX/PS4/CUSA02344"));
        pathEdit.setSingleLine(true);
        pathEdit.setTextSize(14);
        pathEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        folder.addView(pathEdit);
        button(folder, "Use this path", v -> {
            String p = pathEdit.getText().toString().trim();
            if (!p.isEmpty()) setFolder(p);
            refresh();
        });

        LinearLayout prepareCard = card(config, "PREPARE GAME FILES");
        prepareInfo = note(prepareCard, "");
        prepareButton = button(prepareCard, "Prepare", v -> prepare(false));

        LinearLayout page = root;
        root = card(patches, "RESOLUTION PATCHES"); // the settings widgets below go into the card
        text(root, "The game renders at:", 14, false);
        resolution = new RadioGroup(this); // retained for diagnostic source compatibility
        Button resolutionChoice=button(root,"Internal resolution · "+chosenResolution(),null);
        resolutionChoice.setOnClickListener(v->{
            String[] labels=new String[RESOLUTIONS.length];int selected=0;
            for(int n=0;n<labels.length;n++){labels[n]=RESOLUTIONS[n][0];if(labels[n].equals(chosenResolution()))selected=n;}
            new ExpeditionDialog(this).setTitle(AppLanguage.text("Internal resolution")).setSingleChoiceItems(labels,selected,(dialog,which)->{
                prefs.edit().putString("resolution",RESOLUTIONS[which][0]).apply();
                resolutionChoice.setText(AppLanguage.text("Internal resolution · "+RESOLUTIONS[which][0]));dialog.dismiss();
            }).setNegativeButton("Cancel",null).show();
        });
        note(root,"Changes apply on the next game start.");
        buildPatchOptions(patches);
        LinearLayout displayCard = card(home, "DISPLAY & PERFORMANCE");
        android.widget.Switch fullscreen=new android.widget.Switch(this);
        fullscreen.setText(AppLanguage.text("Fullscreen"));fullscreen.setTextColor(0xFFF4EFDF);fullscreen.setMinHeight(dp(54));
        fullscreen.setChecked("1".equals(iniGet("fullscreen")));
        fullscreen.setOnCheckedChangeListener((view,on)->iniPut("fullscreen",on?"1":"0"));displayCard.addView(fullscreen);
        note(displayCard,"Off: original aspect ratio. On: fill the screen.");
        Button upscaleChoice=button(displayCard,"Upscale · "+upscaleLabel(prefs.getInt("mobile_upscale",0)),null);
        upscaleChoice.setOnClickListener(v->{
            String[] labels={"Off · manual resolution","FSR 3.1 Native AA · 720p","FSR 3.1 Quality · 540p → 720p","FSR 3.1 Balanced · 468p → 720p","FSR 3.1 Performance · 360p → 720p"};
            int[] values={0,1,2,3,4};int selected=0;
            for(int n=0;n<values.length;n++)if(values[n]==prefs.getInt("mobile_upscale",0))selected=n;
            new ExpeditionDialog(this).setTitle(AppLanguage.text("Upscale")).setSingleChoiceItems(labels,selected,(dialog,which)->{
                prefs.edit().putInt("mobile_upscale",values[which]).apply();
                upscaleChoice.setText(AppLanguage.text("Upscale · "+upscaleLabel(values[which])));dialog.dismiss();
            }).setNegativeButton("Cancel",null).show();
        });
        button(displayCard,"Performance HUD",v->{
            String[] labels={"Minimal · FPS","Compact · FPS / GPU / CPU / RAM","Full · FPS / GPU / CPU / RAM / FSR"};
            int selected=1;try{selected=Integer.parseInt(iniGet("hud_mode"));}catch(Exception ignored){}
            new ExpeditionDialog(this).setTitle(AppLanguage.text("HUD style")).setSingleChoiceItems(labels,selected,(dialog,which)->{iniPut("hud_mode",Integer.toString(which));dialog.dismiss();}).setNegativeButton("Cancel",null).show();
        });
        LinearLayout languageCard=card(config,"LANGUAGE");
        button(languageCard,AppLanguage.summary(this),v->AppLanguage.show(this));
        note(languageCard,"Language changes apply on next game start.");
        root = card(config, "GPU DRIVER");
        text(root, "Vulkan driver:", 14, false);
        driverList = new LinearLayout(this);
        driverList.setOrientation(LinearLayout.VERTICAL);
        root.addView(driverList);
        buildDriverList();
        LinearLayout driverRow = row(root);
        button(driverRow, "Add a package (zip)", v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/octet-stream"});
            startActivityForResult(i, PICK_DRIVER);
        });
        button(driverRow, "Download...", v -> downloadDriver());
        button(driverRow, "Remove", v -> removeDriver());
        text(root, "The bundled Turnip (tested, with the port's KGSL fixes) is the one to keep on Adreno. Driver "
                 + "packages are the zips the Android emulators use (meta.json and the driver library): the Turnip "
                 + "builds by K11MCH1, MrPurple, StevenMXZ (the Gen8 ones for Adreno 8xx: Snapdragon 8 Elite) and "
                 + "others, or Qualcomm's own driver builds. They are untested with these games; if the bundled "
                 + "driver gives a black screen, try one of them.", 14, false);
        root = displayCard;
        ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
        ((ActivityManager) getSystemService(ACTIVITY_SERVICE)).getMemoryInfo(memory);
        // An 8 GB device shows ~7.1-7.5 GiB to apps (the G2: 7.15), a 6 GB one ~5.5.
        if (memory.totalMem < (6656L << 20))
            text(root, "This device has less than 8 GB of RAM: the games use about 6 GB (tested with 8 GB), "
                     + "so the system may close them.", 14, false);
        uncapped = check(root, "Show each frame at once (up to the display's refresh rate)", "uncapped", true);
        showFps = check(root, "Show the frame rate", "show_fps", true);
        text(root, "Frame rate limit:", 14, false);
        fpsLimit = new RadioGroup(this);
        fpsLimit.setOrientation(RadioGroup.HORIZONTAL);
        // Shared with the in-game panel through bbport.ini: what the panel last chose is shown.
        String current = iniGet("fps_limit");
        if (current == null || current.equals("auto")) current = prefs.getString("fps_limit", defaultFpsLimit());
        for (String l : new String[] {"30", "60", "120", "Off"}) {
            RadioButton b = new RadioButton(this);
            b.setText(AppLanguage.text(l));
            b.setId(View.generateViewId());
            b.setTag(l.equals("Off") ? "0" : l);
            fpsLimit.addView(b);
            if (b.getTag().equals(current)) fpsLimit.check(b.getId());
        }
        fpsLimit.setOnCheckedChangeListener((g, id) -> {
            RadioButton b = g.findViewById(id);
            if (b != null) {
                prefs.edit().putString("fps_limit", (String) b.getTag()).apply();
                iniPut("fps_limit", (String) b.getTag());
            }
        });
        root.addView(fpsLimit);
        text(root, "Off: no limit; above 120 FPS the game's movement timing breaks. The panel in the game "
                 + "has the same setting.", 14, false);
        root = card(config, "CONTROLS");
        text(root, "On-screen gamepad:", 14, false);
        touchPad = new RadioGroup(this);
        touchPad.setOrientation(RadioGroup.HORIZONTAL);
        String pad = iniGet("touch_pad");
        if (pad == null) pad = prefs.getString("touch_pad", "auto");
        for (String[] o : new String[][] {{"0", "Off"}, {"auto", "Auto"}, {"1", "On"}}) {
            RadioButton b = new RadioButton(this);
            b.setText(AppLanguage.text(o[1]));
            b.setId(View.generateViewId());
            b.setTag(o[0]);
            touchPad.addView(b);
            if (o[0].equals(pad)) touchPad.check(b.getId());
        }
        touchPad.setOnCheckedChangeListener((g, id) -> {
            RadioButton b = g.findViewById(id);
            if (b != null) {
                prefs.edit().putString("touch_pad", (String) b.getTag()).apply();
                iniPut("touch_pad", (String) b.getTag());
            }
        });
        root.addView(touchPad);
        text(root, "Auto: shown while no gamepad is connected (phones). Left half of the screen: a stick under "
                 + "the finger; right half: drag to look around; the buttons at the edges. The panel in the game "
                 + "has the same setting with opacity and size.", 14, false);
        note(root, "In the game, swipe in from the right edge of the screen (or press L3+R3) for the settings "
                 + "panel: upscaling, sharpening, frame generation and more, applied at once.");
        root = card(home, "FRAME GENERATION");
        text(root, "LSFG frame generation (your own Lossless.dll):", 14, false);
        button(root, "Import Lossless.dll", v -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            startActivityForResult(pick, PICK_LOSSLESS);
        });
        button(root, "Frame generation: Off / LSFG 2x / 3x / 4x", v -> {
            String[] modes = {"Off", "LSFG 2x", "LSFG 3x", "LSFG 4x"};
            int selected = 0;
            if ("lsfg".equals(iniGet("frame_gen"))) {
                try { selected = Math.max(1, Math.min(3, Integer.parseInt(iniGet("frame_gen_multiplier")) - 1)); }
                catch (Exception ignored) { selected = 1; }
            }
            new ExpeditionDialog(this).setTitle(AppLanguage.text("Frame generation"))
                .setSingleChoiceItems(modes, selected, (dialog, which) -> {
                    if (which > 0 && !new File(files, "Lossless.dll").isFile()) {
                        status("Import your own Lossless.dll first.");
                        dialog.dismiss();
                        return;
                    }
                    iniPut("frame_gen", which == 0 ? "off" : "lsfg");
                    if (which > 0) iniPut("frame_gen_multiplier", Integer.toString(which + 1));
                    status("Frame generation: " + modes[which]);
                    dialog.dismiss();
                }).setNegativeButton("Cancel", null).show();
        });
        text(root, "No DLL or Lossless Scaling shaders are included. LSFG needs a compatible Vulkan driver and adds GPU and memory cost.", 14, false);
        root = card(config, "ADVANCED");
        text(root, "More environment for the port (KEY=VALUE per line, optional):", 14, false);
        extraEnv = new EditText(this);
        extraEnv.setText(prefs.getString("extra_env", ""));
        extraEnv.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        extraEnv.setMinLines(2);
        root.addView(extraEnv);
        root = page;



        LinearLayout savesCard = card(config, "SAVES");
        note(savesCard, "The game's saves are in the app's own folder, which Android deletes together with the app. "
                      + "Back them up to " + backups() + " before uninstalling.");
        LinearLayout saves = row(savesCard);
        button(saves, "Back up saves", v -> {
            String done = backupSaves();
            status(done != null ? "Saves copied to " + done : "Backing up failed (no saves yet, or no access).");
        });
        button(saves, "Restore saves", v -> restoreSaves());
        LinearLayout helpCard = card(config, "DIAGNOSTICS");
        note(helpCard, "The game writes its log to the app's own folder, which other apps cannot read on recent "
                     + "Android. Copy it to " + backups() + " to attach it to a report.");
        button(helpCard, "Export Uncharted log...", v -> LogExport.pick(this));
        text(about, "Uncharted — Daedalus Android", 22, true);
        note(about, "Android fork by erickmendes22 / Daedalus Emulator. Based on revvv991/uncharted-android and Supermedo/uncharted_pc, with selected integrations from RedVorteX-TV/Uncharted-arm.");
        note(about, "Use your own decrypted Nathan Drake Collection files. No game files or Lossless.dll are included.");
        button(about, "Licenses and upstream credits", v -> showLicenses());
        shell.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        LinearLayout nav = new LinearLayout(this);
        nav.setPadding(dp(8), dp(6), dp(8), dp(6));
        nav.setBackgroundColor(0xF5101511);
        String[] tabs = {"Home", "Patches", "Settings", "About"};
        Button[] navButtons = new Button[4];
        for (int n = 0; n < tabs.length; n++) {
            final int index = n;
            Button item = new Button(this, null, android.R.attr.borderlessButtonStyle);
            item.setText(AppLanguage.text(tabs[n])); item.setAllCaps(false); item.setTextSize(11);
            ExpeditionIcon navIcon=new ExpeditionIcon(n,n==0?accent:muted);navIcon.setBounds(0,0,dp(22),dp(22));
            item.setCompoundDrawables(null,navIcon,null,null);item.setCompoundDrawablePadding(dp(3));
            item.setMinWidth(0); item.setMinimumWidth(0); item.setPadding(0, dp(12), 0, dp(12));
            item.setTextColor(n == 0 ? accent : muted);
            item.setOnClickListener(v -> {
                prefs.edit().putString("extra_env", extraEnv.getText().toString()).apply();
                for (int j = 0; j < pages.length; j++) {
                    pages[j].setVisibility(j == index ? View.VISIBLE : View.GONE);
                    navButtons[j].setTextColor(j == index ? accent : muted);
                    android.graphics.drawable.Drawable activeIcon=navButtons[j].getCompoundDrawables()[1];
                    if(activeIcon!=null)activeIcon.setColorFilter(j==index?accent:muted,android.graphics.PorterDuff.Mode.SRC_IN);
                }
                scroll.scrollTo(0, 0);
            });
            navButtons[n] = item;
            nav.addView(item, new LinearLayout.LayoutParams(0, dp(68), 1));
        }
        shell.addView(nav);
        styleChildren(shell);
        shell.setOnApplyWindowInsetsListener((view,insets)->{
            android.graphics.Insets bars=insets.getInsets(android.view.WindowInsets.Type.systemBars());
            view.setPadding(bars.left,bars.top,bars.right,bars.bottom);return insets;
        });
        setContentView(shell);shell.requestApplyInsets();
    }

    private final class ExpeditionDialog extends android.app.AlertDialog.Builder {
        ExpeditionDialog(android.content.Context context) { super(context); }
        @Override public android.app.AlertDialog.Builder setSingleChoiceItems(CharSequence[] labels,int checked,android.content.DialogInterface.OnClickListener listener) {
            CharSequence[] translated=new CharSequence[labels.length];for(int i=0;i<labels.length;i++)translated[i]=AppLanguage.text(labels[i]);
            return super.setSingleChoiceItems(translated,checked,listener);
        }
        @Override public android.app.AlertDialog.Builder setNegativeButton(CharSequence label,android.content.DialogInterface.OnClickListener listener) {return super.setNegativeButton(AppLanguage.text(label),listener);}
        @Override public android.app.AlertDialog.Builder setPositiveButton(CharSequence label,android.content.DialogInterface.OnClickListener listener) {return super.setPositiveButton(AppLanguage.text(label),listener);}
        @Override public android.app.AlertDialog show() {
            android.app.AlertDialog dialog = super.show();
            if (dialog.getWindow() != null) {
                GradientDrawable background = new GradientDrawable();
                background.setColor(0xFF191F19); background.setCornerRadius(dp(22));
                background.setStroke(dp(1), 0xFF62563D);
                dialog.getWindow().setBackgroundDrawable(background);
            }
            for (int which : new int[] {-1, -2, -3}) {
                Button action = dialog.getButton(which);
                if (action != null) { action.setTextColor(accent); action.setMinHeight(dp(48)); }
            }
            return dialog;
        }
    }

    private void styleChildren(View view) {
        if (view instanceof RadioButton) {
            RadioButton radio = (RadioButton)view;
            radio.setTextColor(0xFFF4EFDF);
            radio.setButtonTintList(ColorStateList.valueOf(accent));
        } else if (view instanceof CheckBox) {
            CheckBox check = (CheckBox)view;
            check.setTextColor(0xFFF4EFDF);
            check.setButtonTintList(ColorStateList.valueOf(accent));
        } else if (view instanceof EditText) {
            ((EditText)view).setTextColor(0xFFF4EFDF);
            ((EditText)view).setHintTextColor(muted);
            view.setBackgroundTintList(ColorStateList.valueOf(accent));
        }
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup)view;
            for (int i = 0; i < group.getChildCount(); i++) styleChildren(group.getChildAt(i));
        }
    }

    private void installPkg(boolean update) {
        if(busy||pkgInstaller.busy())return;
        if(!Environment.isExternalStorageManager()){
            pendingPkg=update?1:0;
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,Uri.parse("package:"+getPackageName())));
            return;
        }
        pkgInstaller.pick(update);
    }

    // ---- the game folder ----

    private String folder() {
        return prefs.getString("game_dir", "");
    }

    private void setFolder(String path) {
        while (path.length() > 1 && path.endsWith("/")) path = path.substring(0, path.length() - 1);
        prefs.edit().putString("game_dir", path).apply();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if(LogExport.result(this,request,result,data))return;
        if(pkgInstaller!=null&&pkgInstaller.result(request,result,data))return;
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        if (request == PICK_LOSSLESS) {
            status("Importing Lossless.dll ...");
            new Thread(() -> {
                File tmp = new File(files, "Lossless.dll.import");
                try (InputStream in = getContentResolver().openInputStream(data.getData());
                     OutputStream out = new FileOutputStream(tmp)) {
                    if (in == null) throw new IOException("cannot open selected file");
                    int first = in.read(), second = in.read();
                    if (first != 'M' || second != 'Z') throw new IOException("not a Windows DLL (missing MZ header)");
                    out.write(first); out.write(second);
                    long total = 2;
                    byte[] buf = new byte[65536];
                    for (int n; (n = in.read(buf)) != -1;) {
                        if ((total += n) > (128L << 20)) throw new IOException("DLL exceeds 128 MB");
                        out.write(buf, 0, n);
                    }
                } catch (Exception ex) {
                    tmp.delete(); status("DLL import failed: " + ex.getMessage()); return;
                }
                try {
                    java.nio.file.Files.move(tmp.toPath(), new File(files, "Lossless.dll").toPath(),
                        java.nio.file.StandardCopyOption.REPLACE_EXISTING, java.nio.file.StandardCopyOption.ATOMIC_MOVE);
                    status("Lossless.dll imported. Select LSFG to enable frame generation.");
                } catch (Exception ex) { tmp.delete(); status("DLL import failed: " + ex.getMessage()); }
            }, "Lossless import").start();
            return;
        }
        if (request == PICK_DRIVER) {
            Uri uri = data.getData();
            status("Installing the driver package ...");
            new Thread(() -> {
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    String name = installDriver(in, "driver");
                    prefs.edit().putString("driver", "pkg:" + name).apply();
                    runOnUiThread(this::buildDriverList);
                    status("Driver package installed and selected: " + name);
                } catch (Exception e) {
                    status("Installing the package failed: " + e.getMessage());
                }
            }).start();
            return;
        }
        if (request != PICK_FOLDER) return;
        String path = treeToPath(data.getData());
        if (path == null) status("That folder is not on the device's own storage or SD card: type its path instead.");
        else setFolder(path);
        refresh();
    }

    /** A folder picked with ACTION_OPEN_DOCUMENT_TREE as a file path (the native loader opens
     *  files by path): "primary:x" is internal storage, "XXXX-XXXX:x" a volume under /storage. */
    private static String treeToPath(Uri tree) {
        if (!"com.android.externalstorage.documents".equals(tree.getAuthority())) return null;
        String id = DocumentsContract.getTreeDocumentId(tree);
        int colon = id.indexOf(':');
        if (colon < 0) return null;
        String volume = id.substring(0, colon), rest = id.substring(colon + 1);
        String base = volume.equals("primary") ? Environment.getExternalStorageDirectory().getAbsolutePath()
                                               : "/storage/" + volume;
        return rest.isEmpty() ? base : base + "/" + rest;
    }

    /** What is missing in the folder, or null when it has the three games and their modules. */
    private String problems(String dir) {
        // FEXCore manages memory in 4 KB pages (FEX_PAGE_SIZE); a kernel with 16 KB pages refuses it.
        long page = Os.sysconf(OsConstants._SC_PAGESIZE);
        if (page != 4096)
            return "This device uses " + page / 1024 + " KB memory pages: the x86-64 emulation (FEX) needs 4 KB pages.";
        if (dir.isEmpty()) return "Choose the folder of your decrypted game.";
        if (!new File(dir).isDirectory()) return "Not a folder the app can open: " + dir;
        List<String> missing = new ArrayList<>();
        for (String[] g : GAMES)
            if (!new File(dir, g[1]).isFile()) missing.add(g[1]);
        for (String m : MODULES)
            if (!new File(dir, m).isFile()) missing.add(m);
        if (!missing.isEmpty()) return "Missing in the folder: " + String.join(", ", missing);
        return null;
    }

    /** The stamp of a preparation: the folder and its executables' sizes and times. */
    private String stamp(String dir) {
        StringBuilder b = new StringBuilder(dir).append('\n');
        for (String[] g : GAMES) {
            File f = new File(dir, g[1]);
            b.append(g[1]).append(' ').append(f.length()).append(' ').append(f.lastModified()).append('\n');
        }
        return b.toString();
    }

    private boolean prepared() {
        String dir = folder();
        if (dir.isEmpty()) return false;
        for (String[] g : GAMES) {
            File out = g[2].isEmpty() ? files : new File(files, g[2]);
            if (!new File(out, "boot-linked.bin").isFile() || !new File(out, "content.bin").isFile()) return false;
        }
        return stamp(dir).equals(readText(new File(files, "prepared.txt")));
    }

    private void refresh() {
        String dir = folder();
        boolean access = Environment.isExternalStorageManager();
        accessButton.setVisibility(access ? View.GONE : View.VISIBLE);
        folderText.setText(AppLanguage.text(dir.isEmpty() ? "(none)" : dir));
        String problem = access ? problems(dir) : "Allow the app to read all files first (the game's folder is opened by path).";
        if (problem == null) {
            // (APP_VER is the base game's even when the folder has the patched executables.)
            String title = sfoValue(dir, "TITLE"), id = sfoValue(dir, "TITLE_ID");
            folderInfo.setText(AppLanguage.text((title != null ? title : "?") + "  " + (id != null ? id : "?")));
        } else {
            folderInfo.setText(AppLanguage.text(problem));
        }
        boolean ready = problem == null && prepared();
        String presets = "";
        if (ready) {
            List<String> fit = new ArrayList<>(), not = new ArrayList<>();
            for (String[] g : GAMES) {
                File out = g[2].isEmpty() ? files : new File(files, g[2]);
                (recipeFits(g[3], new File(out, "boot-linked.bin")) ? fit : not).add(g[0].toUpperCase(Locale.ROOT));
            }
            presets = not.isEmpty() ? " The resolution recipes fit all three games."
                    : " The resolution recipes fit " + (fit.isEmpty() ? "none of the games" : String.join(", ", fit))
                      + " (another version of the executables): " + String.join(", ", not)
                      + " run at 1920x1080 without patches.";
        }
        prepareInfo.setText(AppLanguage.text(problem != null ? "-" : ready ? "Prepared for this folder." + presets
                                                     : "Not prepared yet: the app makes the loader's files from the game (a few seconds)."));
        prepareButton.setEnabled(problem == null && !busy);
        playButton.setEnabled(problem == null && !busy);
    }

    private void status(String s) {
        runOnUiThread(() -> statusText.setText(AppLanguage.text(s)));
    }

    // ---- preparing ----

    private void prepare(boolean thenPlay) {
        String dir = folder();
        String problem = problems(dir);
        if (problem != null) {
            status(problem);
            return;
        }
        busy = true;
        refresh();
        new Thread(() -> {
            String error = null;
            for (String[] g : GAMES) {
                File out = g[2].isEmpty() ? files : new File(files, g[2]);
                out.mkdirs();
                status("Preparing " + g[1] + " ...");
                error = prepareGame(dir, g[1], out.getAbsolutePath());
                if (error != null) {
                    error = g[1] + ": " + error;
                    break;
                }
            }
            if (error == null) writeText(new File(files, "prepared.txt"), stamp(dir));
            final String failure = error;
            runOnUiThread(() -> {
                busy = false;
                statusText.setText(AppLanguage.text(failure == null ? "The game's files are ready." : "Preparing failed: " + failure));
                refresh();
                if (failure == null && thenPlay) play();
            });
        }).start();
    }

    // ---- playing ----

    /** The chosen size, "WxH" (the old "native" setting maps to 1920x1080). */
    private String chosenResolution() {
        return prefs.getString("resolution", prefs.getBoolean("native", false) ? "1920x1080" : "1280x720");
    }

    /** Whether a recipe fits the prepared executable: every write's original bytes hash as the
     *  recipe says (another version of the game has other bytes there). */
    private boolean recipeFits(String recipe, File boot) {
        return makePreset(recipe, boot, 1280, 720) != null;
    }

    /** The BBPATCH2 preset for WxH from a recipe asset and the prepared executable
     *  (boot-linked.bin: the image is the file's tail), as scripts/g2/recipe_from_preset.py makes
     *  it: a write is the executable's own bytes with the recipe's edits and its resolution fields
     *  (W, H, W/8, H/4) put in; writes marked below_1080p are left out at 1920x1080. Null when a
     *  write's original bytes do not hash as the recipe says. */
    private byte[] makeBasePreset(String recipe, File boot, int width, int height) {
        String text = readAsset(recipe);
        if (text == null) return null;
        try (RandomAccessFile f = new RandomAccessFile(boot, "r")) {
            byte[] head = new byte[16];
            f.readFully(head);
            long imageSize = ByteBuffer.wrap(head, 8, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
            long imageAt = f.length() - imageSize;
            JSONObject json = new JSONObject(text);
            long base = Long.decode(json.getString("base"));
            JSONArray writes = json.getJSONArray("writes");
            MessageDigest sha = MessageDigest.getInstance("SHA-256");
            List<long[]> places = new ArrayList<>();
            List<byte[]> contents = new ArrayList<>();
            int total = 0;
            for (int i = 0; i < writes.length(); i++) {
                JSONObject w = writes.getJSONObject(i);
                if (w.getString("when").equals("below_1080p") && height >= 1080) continue;
                long offset = Long.decode(w.getString("address")) - base;
                int length = w.getInt("length");
                if (offset < 0 || offset + length > imageSize) return null;
                byte[] data = new byte[length];
                f.seek(imageAt + offset);
                f.readFully(data);
                if (!hex(sha.digest(data)).equals(w.getString("sha256"))) return null;
                JSONArray edits = w.optJSONArray("edits");
                for (int j = 0; edits != null && j < edits.length(); j++) {
                    JSONObject e = edits.getJSONObject(j);
                    byte[] b = unhex(e.getString("bytes"));
                    System.arraycopy(b, 0, data, e.getInt("offset"), b.length);
                }
                JSONArray fields = w.optJSONArray("fields");
                for (int j = 0; fields != null && j < fields.length(); j++) {
                    JSONObject fd = fields.getJSONObject(j);
                    String formula = fd.getString("value");
                    int value = formula.equals("W") ? width : formula.equals("H") ? height
                              : formula.equals("W/8") ? width / 8 : formula.equals("H/4") ? height / 4 : -1;
                    if (value < 0) return null;
                    int at = fd.getInt("offset"), bytes = fd.getInt("width");
                    for (int k = 0; k < bytes; k++) data[at + k] = (byte) (value >> (8 * k));
                }
                places.add(new long[] {offset, length});
                contents.add(data);
                total += length;
            }
            ByteBuffer out = ByteBuffer.allocate(24 + places.size() * 16 + total).order(ByteOrder.LITTLE_ENDIAN);
            out.put("BBPATCH2".getBytes("US-ASCII")).putLong(base).putLong(places.size());
            for (int i = 0; i < places.size(); i++)
                out.putLong(places.get(i)[0]).putLong(places.get(i)[1]).put(contents.get(i));
            return out.array();
        } catch (Exception e) {
            return null;
        }
    }

    private String upscaleLabel(int mode) {
        return UpscalePreset.label(mode);
    }

    private void effectSwitch(LinearLayout parent,String title,String subtitle,String key) {
        LinearLayout line=new LinearLayout(this);line.setGravity(android.view.Gravity.CENTER_VERTICAL);line.setPadding(0,dp(9),0,dp(9));
        LinearLayout labels=new LinearLayout(this);labels.setOrientation(LinearLayout.VERTICAL);
        TextView name=text(labels,title,15,true);name.setPadding(0,0,0,0);

        line.addView(labels,new LinearLayout.LayoutParams(0,ViewGroup.LayoutParams.WRAP_CONTENT,1));
        Button state=new Button(this,null,android.R.attr.borderlessButtonStyle);
        state.setAllCaps(false);state.setTextSize(13);state.setMinWidth(0);state.setMinimumWidth(0);
        state.setPadding(dp(8),0,dp(8),0);
        Runnable update=()->{
            boolean enabled=!prefs.getBoolean(key,false);
            state.setText(AppLanguage.text(enabled?"Enabled":"Disabled"));
            state.setContentDescription(AppLanguage.text(title)+": "+AppLanguage.text(enabled?"Enabled in game":"Disabled in game"));
            state.setTextColor(enabled?accent:0xFFB0AAA0);
            GradientDrawable bg=new GradientDrawable();bg.setCornerRadius(dp(10));
            bg.setColor(enabled?0x333D3321:0xFF222522);bg.setStroke(dp(1),enabled?accent:0xFF555B54);state.setBackground(bg);
        };
        state.setOnClickListener(v->{
            String[] choices={"Enabled in game","Disabled in game"};
            new ExpeditionDialog(this).setTitle(AppLanguage.text(title)).setSingleChoiceItems(choices,prefs.getBoolean(key,false)?1:0,(dialog,which)->{
                prefs.edit().putBoolean(key,which==1).apply();update.run();dialog.dismiss();
            }).setNegativeButton("Cancel",null).show();
        });
        update.run();line.addView(state,new LinearLayout.LayoutParams(dp(100),dp(44)));
        parent.addView(line);
        View divider=new View(this);divider.setBackgroundColor(0x225F553A);parent.addView(divider,new LinearLayout.LayoutParams(-1,dp(1)));
    }

    private void buildPatchOptions(LinearLayout parent) {
        LinearLayout shadows=card(parent,"SHADOWS");
        Button shadowChoice=button(shadows,"Shadow quality · "+new String[]{"Original","Reduced","Minimum"}[prefs.getInt("shadow_cascades",0)],null);
        shadowChoice.setOnClickListener(v->{String[] labels={"Original","Reduced · 2 cascades","Minimum · 1 cascade"};
            new ExpeditionDialog(this).setTitle(AppLanguage.text("Shadow quality")).setSingleChoiceItems(labels,prefs.getInt("shadow_cascades",0),(dialog,which)->{
                prefs.edit().putInt("shadow_cascades",which).apply();shadowChoice.setText(AppLanguage.text("Shadow quality · "+labels[which]));dialog.dismiss();
            }).setNegativeButton("Cancel",null).show();
        });
        LinearLayout lighting=card(parent,"LIGHTING & ATMOSPHERE");
        note(lighting,"Enabled keeps the effect. Disabled removes it.");
        effectSwitch(lighting,"Water reflections","Reflections on water surfaces","patch_Disable water reflections");
        effectSwitch(lighting,"Ambient occlusion","Contact shading · SSAO","patch_Disable SSAO");
        effectSwitch(lighting,"Fog","Atmospheric haze","patch_Disable fog");
        effectSwitch(lighting,"Volumetric lights","Visible light shafts","patch_Disable volumetric lights");
        effectSwitch(lighting,"Dynamic lights","Local lighting effects","patch_Disable dynamic lights");
        effectSwitch(lighting,"Sunlight occlusion","Shading from sunlight","patch_Disable sunlight ambient occlusion");
        LinearLayout image=card(parent,"IMAGE & EFFECTS");
        effectSwitch(image,"Motion blur","Blur during movement","patch_Disable motion blur");
        effectSwitch(image,"Depth of field","Background focus effects","patch_Disable depth of field");
        effectSwitch(image,"Bloom","Glow around bright areas","patch_Disable bloom");
        effectSwitch(image,"Anti-aliasing","The game's original edge smoothing","patch_Disable anti-aliasing");
        effectSwitch(image,"Texture filtering","Anisotropic filtering","patch_Disable anisotropic filtering");
        effectSwitch(image,"Full draw distance","Off: use the reduced 40 m patch","patch_Draw distance: 40 m (extreme)");
        LinearLayout startup=card(parent,"STARTUP");
        effectSwitch(startup,"Intro videos","Off: skip the opening logos","patch_Skip startup logos");
        note(startup,"Applies on next start when compatible with the installed game.");
    }

    /** Merge existing recipes with independently validated Bachata batches. No game bytes are changed on disk. */
    private byte[] makePreset(String recipe, File boot, int width, int height) {
        byte[] nativePreset = makeBasePreset(recipe, boot, width, height);
        try (RandomAccessFile file = new RandomAccessFile(boot, "r")) {
            byte[] head = new byte[16]; file.readFully(head);
            long imageSize = ByteBuffer.wrap(head, 8, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
            long imageAt = file.length() - imageSize;
            if (imageSize < 0 || imageAt < 16) return nativePreset;
            final long base = 0x400000L;
            List<long[]> places = new ArrayList<>();
            List<byte[]> data = new ArrayList<>();
            if (nativePreset != null) {
                ByteBuffer b = ByteBuffer.wrap(nativePreset).order(ByteOrder.LITTLE_ENDIAN);
                b.position(16); long count = b.getLong();
                for (long i = 0; i < count; i++) {
                    long address = b.getLong(); int size = Math.toIntExact(b.getLong());
                    byte[] bytes = new byte[size]; b.get(bytes);
                    places.add(new long[] {address, size}); data.add(bytes);
                }
            }
            String elf = new JSONObject(readAsset(recipe)).getString("executable");
            JSONArray catalog = new JSONObject(readAsset("recipes/bachata.json")).getJSONArray("patches");
            java.util.Map<String, List<JSONObject>> groups = new java.util.LinkedHashMap<>();
            for (int i = 0; i < catalog.length(); i++) {
                JSONObject patch = catalog.getJSONObject(i);
                if (!elf.equals(patch.getString("elf"))) continue;
                String name = patch.getString("name");
                boolean resolution = name.startsWith("Resolution:");
                boolean wanted = resolution ? nativePreset == null && height < 1080 && name.startsWith("Resolution: " + height + "p ")
                    : name.startsWith("Shadows: 2") ? prefs.getInt("shadow_cascades", 0) == 1
                    : name.startsWith("Shadows: 1") ? prefs.getInt("shadow_cascades", 0) == 2
                    : prefs.getBoolean("patch_" + name, false);
                if (wanted) groups.computeIfAbsent(name, key -> new ArrayList<>()).add(patch);
            }
            boolean resolutionReady = nativePreset != null || height == 1080;
            MessageDigest sha = MessageDigest.getInstance("SHA-256");
            for (java.util.Map.Entry<String, List<JSONObject>> group : groups.entrySet()) {
                JSONObject match = null; int matches = 0;
                for (JSONObject patch : group.getValue()) {
                    boolean fits = true; JSONArray writes = patch.getJSONArray("writes");
                    for (int j = 0; j < writes.length(); j++) {
                        JSONObject write = writes.getJSONObject(j);
                        long offset = Long.decode(write.getString("address")) - base;
                        int length = write.getInt("length");
                        if (offset < 0 || length <= 0 || offset > imageSize - length) { fits = false; break; }
                        byte[] original = new byte[length]; file.seek(imageAt + offset); file.readFully(original);
                        if (!hex(sha.digest(original)).equals(write.getString("sha256"))) { fits = false; break; }
                    }
                    if (fits) { match = patch; matches++; }
                }
                if (matches != 1) {
                    android.util.Log.w("Patches", elf + ": " + group.getKey() + " skipped (" + matches + " matching versions)");
                    continue;
                }
                JSONArray writes = match.getJSONArray("writes");
                List<long[]> batchPlaces = new ArrayList<>(); List<byte[]> batchData = new ArrayList<>();
                boolean conflict = false;
                for (int j = 0; j < writes.length(); j++) {
                    JSONObject write = writes.getJSONObject(j);
                    long offset = Long.decode(write.getString("address")) - base;
                    byte[] bytes = unhex(write.getString("bytes"));
                    if (bytes.length != write.getInt("length")) { conflict = true; break; }
                    for (long[] range : places)
                        if (offset < range[0] + range[1] && range[0] < offset + bytes.length) conflict = true;
                    for (long[] range : batchPlaces)
                        if (offset < range[0] + range[1] && range[0] < offset + bytes.length) conflict = true;
                    batchPlaces.add(new long[] {offset, bytes.length}); batchData.add(bytes);
                }
                if (!conflict) {
                    places.addAll(batchPlaces); data.addAll(batchData);
                    if (group.getKey().startsWith("Resolution:")) resolutionReady = true;
                    android.util.Log.i("Patches", elf + ": " + group.getKey() + " verified and enabled");
                }
            }
            if (!resolutionReady) return null; // never label an unpatched native render as low resolution
            int total = 24;
            for (byte[] bytes : data) total = Math.addExact(total, 16 + bytes.length);
            ByteBuffer out = ByteBuffer.allocate(total).order(ByteOrder.LITTLE_ENDIAN);
            out.put("BBPATCH2".getBytes("US-ASCII")).putLong(base).putLong(places.size());
            for (int i = 0; i < places.size(); i++) out.putLong(places.get(i)[0]).putLong(data.get(i).length).put(data.get(i));
            return out.array();
        } catch (Exception ex) {
            android.util.Log.w("Patches", "Catalog: " + ex.getMessage());
            return nativePreset;
        }
    }

    private static String hex(byte[] bytes) {
        StringBuilder s = new StringBuilder();
        for (byte b : bytes) s.append(String.format(Locale.ROOT, "%02x", b));
        return s.toString();
    }

    private static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++) out[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16);
        return out;
    }

    /** The CPUs of every cluster but the slowest (the port's GPU threads: BB_RENDER_CPUS), when
     *  the slowest is little cores (under 3/4 of the fastest clock; the G2's run at 0.68). Else
     *  none: on a Snapdragon 8 Elite (2 + 6 big cores, 0.79) it put them all on two (issue #8). */
    private static String fastCpus() {
        List<Integer> cpus = new ArrayList<>();
        List<Long> freqs = new ArrayList<>();
        for (int i = 0; i < 64; i++) {
            String f = readText(new File("/sys/devices/system/cpu/cpu" + i + "/cpufreq/cpuinfo_max_freq")).trim();
            if (f.isEmpty()) {
                if (!new File("/sys/devices/system/cpu/cpu" + i).exists()) break;
                continue;
            }
            cpus.add(i);
            freqs.add(Long.parseLong(f));
        }
        if (cpus.isEmpty()) return null;
        long slowest = Long.MAX_VALUE, fastest = 0;
        for (long f : freqs) {
            slowest = Math.min(slowest, f);
            fastest = Math.max(fastest, f);
        }
        if (slowest * 4 >= fastest * 3) return null;
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < cpus.size(); i++)
            if (freqs.get(i) > slowest) b.append(b.length() > 0 ? "," : "").append(cpus.get(i));
        return b.length() > 0 ? b.toString() : null;
    }

    /** 60 whatever the display does: at 120 a phone's GPU falls behind and the game runs
     *  ahead of it (issue #5); 120 stays a choice. */
    private String defaultFpsLimit() {
        return "60";
    }

    /** The port's settings file (bbport.ini, BB_CONFIG): the in-game panel writes it; the
     *  launcher reads and sets the keys it shares with the panel. */
    private File iniFile() {
        return new File(files, "bbport.ini");
    }

    private String iniGet(String key) {
        File f = iniFile();
        if (!f.exists()) return null;
        try {
            for (String line : new String(java.nio.file.Files.readAllBytes(f.toPath()), "UTF-8").split("\n")) {
                int eq = line.indexOf('=');
                if (eq > 0 && line.substring(0, eq).trim().equals(key)) return line.substring(eq + 1).trim();
            }
        } catch (java.io.IOException e) { }
        return null;
    }

    private void iniPut(String key, String value) {
        File f = iniFile();
        StringBuilder out = new StringBuilder();
        boolean done = false;
        try {
            if (f.exists())
                for (String line : new String(java.nio.file.Files.readAllBytes(f.toPath()), "UTF-8").split("\n")) {
                    int eq = line.indexOf('=');
                    if (eq > 0 && line.substring(0, eq).trim().equals(key)) {
                        if (!done) out.append(key).append('=').append(value).append('\n');
                        done = true;
                    } else if (!line.trim().isEmpty()) {
                        out.append(line.trim()).append('\n');
                    }
                }
        } catch (java.io.IOException e) { }
        if (!done) out.append(key).append('=').append(value).append('\n');
        writeText(f, out.toString());
    }

    private void play() {
        if (!prepared()) {
            prepare(true);
            return;
        }
        prefs.edit().putString("extra_env", extraEnv.getText().toString()).apply();
        String dir = folder(), d = files.getAbsolutePath();
        int upscale=prefs.getInt("mobile_upscale",0);
        String[] size = UpscalePreset.input(upscale,chosenResolution()).split("x");
        int width = Integer.parseInt(size[0]), height = Integer.parseInt(size[1]);
        boolean below1080 = height < 1080;
        new File(files, "patches").mkdirs();
        new File(files, "user").mkdirs();
        StringBuilder report = new StringBuilder();
        String[] env = new String[GAMES.length];
        for (int i = 0; i < GAMES.length; i++) {
            String[] g = GAMES[i];
            String out = g[2].isEmpty() ? d : d + "/" + g[2];
            String preset = null;
            // The preset for the chosen size, made from the recipe and the prepared executable;
            // at 1920x1080 it holds only the writes that apply at every size (empty for U1).
            byte[] made = makePreset(g[3], new File(out, "boot-linked.bin"), width, height);
            if (made == null) {
                if (below1080)
                    report.append(g[0].toUpperCase(Locale.ROOT)).append(": the resolution recipe does not fit this version of the executable, it runs at 1920x1080 without patches. ");
            } else if (made.length > 24) {
                File p = new File(files, "patches/" + g[0] + "-" + width + "x" + height + ".bin");
                try (FileOutputStream o = new FileOutputStream(p)) {
                    o.write(made);
                    preset = p.getAbsolutePath();
                } catch (IOException e) {
                    report.append(g[0].toUpperCase(Locale.ROOT)).append(": cannot write its patches. ");
                }
            }
            StringBuilder args = new StringBuilder();
            args.append(out).append("/boot-linked.bin\n--content-profile\n").append(out).append("/content.bin\n")
                .append("--app0\n").append(dir).append("\n--user\n").append(d).append("/user\n")
                .append("--timeout\n0\n--log\n").append(d).append("/uncharted.log\n");
            if (preset != null) args.append("--patches\n").append(preset).append('\n');
            writeText(new File(files, "args-" + g[0] + ".txt"), args.toString());
            if (g[0].equals(prefs.getString("selected_game", "u1"))) writeText(new File(files, "args.txt"), args.toString());
            // The movie decoder's picture size goes with a size below 1080p (port 0015).
            env[i] = preset != null && below1080 ? "BB_VDEC_CONTENT=" + width + "x" + height + "\n" : "-BB_VDEC_CONTENT\n";
            if(upscale>0 && made!=null)env[i]+="BB_RENDER_RES="+width+"x"+height+"\nBB_OUTPUT_RES="+UpscalePreset.OUTPUT+"\n";
            else env[i]+="-BB_RENDER_RES\n-BB_OUTPUT_RES\n";
            if(upscale>0 && made==null)env[i]+="BB_UPSCALER=none\n";
            // U2's aiming autofocus reads a 1x1 render result on the CPU: copied back for U2
            // only, as upstream does (issue #7; it costs elsewhere). More environment overrides.
            if (g[0].equals("u2") && !extraEnv.getText().toString().contains("BB_TINY_READBACK"))
                env[i] += "BB_TINY_READBACK=1\n";
        }
        StringBuilder e = new StringBuilder();
        e.append("BB_MOBILE_UPSCALE=0\n");
        if(upscale>0){iniPut("output_res","1280x720");iniPut("upscaler","fsr3");iniPut("preset",Integer.toString(upscale-1));}
        else {iniPut("preset","0");iniPut("upscaler","off");}
        e.append("# Written by the launcher on every start; edit the settings there.\n");
        e.append("BB_FULLSCREEN=1\nBB_FPS_LOG=1\nBB_MIDFRAME_FLUSH=10\nBB_DEFERRED_SUBMIT=1\nBB_SKIN_BATCH=1\n");
        e.append("BB_CS_SPLIT=256:8f067540\nBB_OBJECT_MOTION=0\nBB_THREAD_NICE=0\n");
        // The game waits when it is two frames ahead of the GPU side (gnmdriver.cpp): a GPU that
        // falls behind (shaders compiling, a slow device) made it trip its own command buffer
        // check (issue #5); no cost on the G2.
        e.append("BB_MAX_FRAMES_AHEAD=2\n");
        e.append("BB_DUMP_REQUEST=").append(d).append("/dump-request\n");
        String cpus = fastCpus();
        if (cpus != null) e.append("BB_RENDER_CPUS=").append(cpus).append('\n');
        File lossless = new File(files, "Lossless.dll");
        if (lossless.isFile()) e.append("BB_LSFG_DLL=").append(lossless.getAbsolutePath()).append('\n');
        String driver = driver();
        if (driver.equals("auto")) driver = automaticDriver();
        if (driver.equals("turnip")) e.append("BB_VK_DRIVER=libvulkan_freedreno.so\n");
        else if (driver.startsWith("pkg:")) {
            File pkg = new File(driversDir(), driver.substring(4));
            String lib = packageLibrary(pkg);
            if (lib != null) e.append("BB_VK_DRIVER=").append(lib).append("\nBB_VK_DRIVER_DIR=").append(pkg.getAbsolutePath()).append('\n');
            else report.append("The chosen driver package is missing or broken: the system driver is used. ");
        }
        if (uncapped.isChecked()) e.append("BB_VBLANK_HZ=0\n");
        e.append("BB_SHOW_FPS=").append(showFps.isChecked()?1:0).append('\n');
        e.append(extraEnv.getText().toString().trim()).append('\n');
        writeText(new File(files, "env.txt"), e.toString());
        for (int i = 0; i < GAMES.length; i++) writeText(new File(files, "env-" + GAMES[i][0] + ".txt"), env[i]);
        new File(files, "loadexec.txt").delete();
        RadioButton limit = fpsLimit.findViewById(fpsLimit.getCheckedRadioButtonId());
        if (limit != null) iniPut("fps_limit", (String) limit.getTag());
        RadioButton pad = touchPad.findViewById(touchPad.getCheckedRadioButtonId());
        if (pad != null) iniPut("touch_pad", (String) pad.getTag());
        if (report.length() > 0) status(report.toString());
        Intent i = new Intent(this, GameActivity.class);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
        finish();
    }

    // ---- Vulkan drivers ----

    /** Installed driver packages live in the app's private storage: Android loads libraries from
     *  there, not from shared storage. One folder per package: meta.json and the library. */
    private File driversDir() {
        File d = new File(getFilesDir(), "drivers");
        d.mkdirs();
        return d;
    }

    /** "turnip" (the bundled Turnip), "system", or "pkg:<folder>" (an installed package). */
    private String driver() {
        String d = prefs.getString("driver", null);
        if (d == null) d = prefs.contains("turnip") ? (prefs.getBoolean("turnip", false) ? "turnip" : "system") : "auto";
        return d;
    }

    private String automaticDriver() {
        if ((gpuChip >>> 24) == 0x44) {
            try {
                File marker = new File(driversDir(), ".gen8-v37");
                String installed = marker.isFile() ? readText(marker).trim() : "";
                if (!installed.isEmpty() && packageLibrary(new File(driversDir(), installed)) != null) return "pkg:" + installed;
                try (InputStream in = getAssets().open("drivers/Turnip_Gen8_V37.zip")) {
                    installed = installDriver(in, "Turnip_Gen8_V37");
                }
                writeText(marker, installed);
                return "pkg:" + installed;
            } catch (Exception ex) {
                status("Automatic Gen8 driver failed: " + ex.getMessage() + ". Using bundled Turnip.");
                return "turnip";
            }
        }
        return new File("/dev/kgsl-3d0").exists() ? "turnip" : "system";
    }

    private static JSONObject packageMeta(File dir) {
        try {
            String text = readText(new File(dir, "meta.json"));
            return text.isEmpty() ? null : new JSONObject(text);
        } catch (Exception e) {
            return null;
        }
    }

    /** The package's library (meta.json libraryName) when the file is there, else null. */
    private static String packageLibrary(File dir) {
        JSONObject meta = packageMeta(dir);
        String lib = meta != null ? meta.optString("libraryName", "") : "";
        return !lib.isEmpty() && new File(dir, lib).isFile() ? lib : null;
    }

    private void buildDriverList() {
        driverList.removeAllViews();
        drivers = new RadioGroup(this);
        boolean kgsl = new File("/dev/kgsl-3d0").exists();
        List<String[]> options = new ArrayList<>();
        options.add(new String[] {"auto", "Automatic: " + ((gpuChip >>> 24) == 0x44 ? "Turnip Gen8 V37 (Adreno 8xx)" : kgsl ? "bundled Turnip (Adreno)" : "system Vulkan driver")});
        options.add(new String[] {"turnip", "Bundled Turnip (Mesa 26.2.5 with the port's fixes; Adreno)"
                                            + (kgsl ? "" : " - no Adreno GPU (KGSL) found here")});
        options.add(new String[] {"system", "The system's Vulkan driver" + (kgsl ? " (untested with these games)" : "")});
        File[] installed = driversDir().listFiles(File::isDirectory);
        if (installed != null) {
            java.util.Arrays.sort(installed);
            for (File d : installed) {
                JSONObject meta = packageMeta(d);
                String label = meta != null ? meta.optString("name", d.getName()) : d.getName();
                String version = meta != null ? meta.optString("driverVersion", "") : "";
                String author = meta != null ? meta.optString("author", "") : "";
                String detail = version.isEmpty() ? author : author.isEmpty() ? version : version + ", " + author;
                if (!detail.isEmpty()) label += " (" + detail + ")";
                if (packageLibrary(d) == null) label += " - broken: no library";
                options.add(new String[] {"pkg:" + d.getName(), label});
            }
        }
        drivers.setOnCheckedChangeListener((g, id) -> {
            RadioButton b = g.findViewById(id);
            if (b != null) prefs.edit().putString("driver", (String) b.getTag()).apply();
        });
        String current = prefs.getString("driver", "auto");
        for (String[] o : options) {
            RadioButton b = new RadioButton(this);
            b.setText(AppLanguage.text(o[1]));
            b.setId(View.generateViewId());
            b.setTag(o[0]);
            drivers.addView(b);
            if (o[0].equals(current)) drivers.check(b.getId());
        }
        // The chosen package is gone: back to the default.
        if (drivers.getCheckedRadioButtonId() == -1) drivers.check(drivers.getChildAt(0).getId());
        driverList.addView(drivers);
        styleChildren(driverList);
    }

    /** Unpacks a driver package (a zip with meta.json and the driver library) into
     *  driversDir()/<name>; the folder's name. */
    private String installDriver(InputStream in, String fallbackName) throws IOException {
        File tmp = new File(driversDir(), ".new-" + System.currentTimeMillis());
        if (!tmp.mkdirs()) throw new IOException("cannot create " + tmp);
        try {
            long total = 0;
            try (ZipInputStream zip = new ZipInputStream(new java.io.BufferedInputStream(in))) {
                for (ZipEntry e; (e = zip.getNextEntry()) != null;) {
                    if (e.isDirectory()) continue;
                    String name = new File(e.getName()).getName(); // a package is one flat folder
                    if (name.isEmpty() || name.startsWith(".")) continue;
                    try (OutputStream out = new FileOutputStream(new File(tmp, name))) {
                        byte[] buf = new byte[1 << 16];
                        for (int n; (n = zip.read(buf)) > 0;) {
                            out.write(buf, 0, n);
                            if ((total += n) > (256L << 20)) throw new IOException("the package is over 256 MB");
                        }
                    }
                }
            }
            JSONObject meta = packageMeta(tmp);
            if (meta == null) throw new IOException("no meta.json in the package (not a driver package?)");
            if (packageLibrary(tmp) == null) throw new IOException("the library named in meta.json is not in the package");
            String name = meta.optString("name", fallbackName).replaceAll("[^A-Za-z0-9._-]+", "_");
            if (name.isEmpty() || name.startsWith(".")) name = "driver";
            if (name.length() > 60) name = name.substring(0, 60);
            File dir = new File(driversDir(), name);
            if (dir.exists()) deleteTree(dir);
            if (!tmp.renameTo(dir)) throw new IOException("cannot rename to " + dir);
            return name;
        } finally {
            if (tmp.exists()) deleteTree(tmp);
        }
    }

    /** The zip assets of the sources' latest releases, picked from a list, then fetchDriver. */
    private void downloadDriver() {
        status("Looking up the driver releases ...");
        new Thread(() -> {
            List<String[]> found = new ArrayList<>(); // {label, url, file name}
            StringBuilder errors = new StringBuilder();
            for (String repo : DRIVER_SOURCES) {
                try {
                    JSONArray releases = new JSONArray(fetch("https://api.github.com/repos/" + repo + "/releases?per_page=6"));
                    for (int i = 0; i < releases.length(); i++) {
                        JSONObject r = releases.getJSONObject(i);
                        JSONArray assets = r.getJSONArray("assets");
                        for (int j = 0; j < assets.length(); j++) {
                            JSONObject a = assets.getJSONObject(j);
                            String name = a.getString("name");
                            long size = a.getLong("size");
                            if (!name.toLowerCase(Locale.ROOT).endsWith(".zip") || size > (256L << 20)) continue;
                            found.add(new String[] {repo.substring(0, repo.indexOf('/')) + ": " + name + "  ("
                                                    + r.optString("published_at", "").replaceAll("T.*", "") + ", "
                                                    + Math.max(1, size >> 20) + " MB)",
                                                    a.getString("browser_download_url"), name});
                        }
                    }
                } catch (Exception e) {
                    errors.append(repo).append(": ").append(e.getMessage()).append(". ");
                }
            }
            runOnUiThread(() -> {
                if (found.isEmpty()) {
                    status("No packages found. " + errors);
                    return;
                }
                status(errors.toString());
                CharSequence[] items = new CharSequence[found.size()];
                for (int i = 0; i < items.length; i++) items[i] = found.get(i)[0];
                new ExpeditionDialog(this).setTitle(AppLanguage.text("Download a driver package"))
                    .setItems(items, (d, which) -> fetchDriver(found.get(which)[1], found.get(which)[2]))
                    .setNegativeButton("Cancel", null).show();
            });
        }).start();
    }

    private void fetchDriver(String url, String fileName) {
        status("Downloading " + fileName + " ...");
        new Thread(() -> {
            try (InputStream in = open(url).getInputStream()) {
                String name = installDriver(in, fileName.replaceAll("(?i)\\.zip$", ""));
                prefs.edit().putString("driver", "pkg:" + name).apply();
                runOnUiThread(this::buildDriverList);
                status("Driver package installed and selected: " + name);
            } catch (Exception e) {
                status("Downloading failed: " + e.getMessage());
            }
        }).start();
    }

    private void removeDriver() {
        String d = driver();
        if (!d.startsWith("pkg:")) {
            status("Select an installed package to remove it.");
            return;
        }
        File dir = new File(driversDir(), d.substring(4));
        new ExpeditionDialog(this).setTitle(AppLanguage.text("Remove the driver package"))
            .setMessage(AppLanguage.text("Remove " + dir.getName() + "?"))
            .setPositiveButton("Remove", (dl, w) -> {
                deleteTree(dir);
                prefs.edit().putString("driver", new File("/dev/kgsl-3d0").exists() ? "turnip" : "system").apply();
                buildDriverList();
                status("Removed " + dir.getName());
            })
            .setNegativeButton("Cancel", null).show();
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(15000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", "uncharted-android");
        c.setRequestProperty("Accept", "application/vnd.github+json, */*");
        if (c.getResponseCode() != 200) throw new IOException("HTTP " + c.getResponseCode() + " from " + new URL(url).getHost());
        return c;
    }

    private static String fetch(String url) throws IOException {
        try (InputStream in = open(url).getInputStream()) {
            return new String(in.readAllBytes(), "UTF-8");
        }
    }

    // ---- saves ----

    /** Where save backups go: shared storage survives the app (all files access). */
    private static File backups() {
        return new File(Environment.getExternalStorageDirectory(), "Documents/uncharted-android");
    }

    /** files/user (the PS4 user profile: saves, settings) copied to backups()/saves-<time>; the
     *  backup's path, or null. */
    private String backupSaves() {
        File user = new File(files, "user");
        if (!user.isDirectory()) return null;
        String stamp = new java.text.SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new java.util.Date());
        File to = new File(backups(), "saves-" + stamp);
        try {
            copyTree(user, to);
            return to.getAbsolutePath();
        } catch (IOException e) {
            return null;
        }
    }

    /** files/bb-probe.log copied to backups() (other apps cannot read the app's folder on recent
     *  Android); the copy's path, or null. */
    private String exportLog() {
        File log = new File(files, "uncharted.log");
        if (!log.isFile()) return null;
        String stamp = new java.text.SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new java.util.Date());
        File to = new File(backups(), "Uncharted-" + stamp + ".log");
        try {
            backups().mkdirs();
            copyTree(log, to);
            return to.getAbsolutePath();
        } catch (IOException e) {
            return null;
        }
    }

    /** The newest backup back into files/user, after backing up the current saves. */
    private void restoreSaves() {
        File[] list = backups().listFiles(f -> f.isDirectory() && f.getName().startsWith("saves-"));
        if (list == null || list.length == 0) {
            status("No backups in " + backups());
            return;
        }
        java.util.Arrays.sort(list);
        File newest = list[list.length - 1];
        new ExpeditionDialog(this).setTitle(AppLanguage.text("Restore saves"))
            .setMessage(AppLanguage.text("Replace the app's saves with " + newest.getName() + "? The current ones are backed up first."))
            .setPositiveButton("Restore", (d, w) -> {
                File user = new File(files, "user");
                if (user.isDirectory() && backupSaves() == null) {
                    status("Backing up the current saves failed: nothing was changed.");
                    return;
                }
                try {
                    deleteTree(user);
                    copyTree(newest, user);
                    status("Saves restored from " + newest.getName());
                } catch (IOException e) {
                    status("Restoring failed: " + e.getMessage());
                }
            })
            .setNegativeButton("Cancel", null).show();
    }

    private static void copyTree(File from, File to) throws IOException {
        if (from.isDirectory()) {
            if (!to.isDirectory() && !to.mkdirs()) throw new IOException("cannot create " + to);
            String[] names = from.list();
            if (names != null)
                for (String n : names) copyTree(new File(from, n), new File(to, n));
        } else {
            try (InputStream in = new java.io.FileInputStream(from); OutputStream out = new FileOutputStream(to)) {
                byte[] buf = new byte[1 << 16];
                for (int n; (n = in.read(buf)) > 0;) out.write(buf, 0, n);
            }
        }
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null)
            for (File c : children) deleteTree(c);
        f.delete();
    }

    /** The licenses of everything in the app (assets/licenses.txt, game_app/licenses.sh). */
    private void showLicenses() {
        String text = readAsset("licenses.txt");
        ScrollView scroll = new ScrollView(this);
        TextView t = new TextView(this);
        t.setText(AppLanguage.text(text.isEmpty() ? "This build has no license file (build.sh PUBLIC=1 with licenses.sh's output)." : text));
        t.setTextSize(12);
        t.setTypeface(Typeface.MONOSPACE);
        t.setPadding(dp(16), dp(16), dp(16), dp(16));
        t.setTextIsSelectable(true);
        scroll.addView(t);
        new ExpeditionDialog(this).setTitle(AppLanguage.text("Licenses")).setView(scroll)
            .setPositiveButton("Close", null).show();
    }

    // ---- files ----

    private static String readText(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            return new String(in.readAllBytes(), "UTF-8");
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeText(File f, String s) {
        try (FileWriter w = new FileWriter(f)) {
            w.write(s);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private byte[] readAssetBytes(String name) {
        try (InputStream in = getAssets().open(name)) {
            return in.readAllBytes();
        } catch (IOException e) {
            return null;
        }
    }

    private String readAsset(String name) {
        byte[] b = readAssetBytes(name);
        try {
            return b != null ? new String(b, "UTF-8") : "";
        } catch (IOException e) {
            return "";
        }
    }

    private boolean copyAsset(String name, File to) {
        byte[] b = readAssetBytes(name);
        if (b == null) return false;
        try (OutputStream out = new FileOutputStream(to)) {
            out.write(b);
            return true;
        } catch (IOException e) {
            return false;
        }
    }
}
