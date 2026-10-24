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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** The app's first screen: the folder with the user's own decrypted game (a PS4 dump of
 *  Uncharted: The Nathan Drake Collection), the loader's files made from it on the device
 *  (libbbprepare.so: the port's scripts/prepare.py and link_modules.py in C), a few settings,
 *  then GameActivity with files/args*.txt and env*.txt written from them. Nothing of the game
 *  ships with the app: the patch presets are checked against the user's executables first.
 *  adb automation: am start -n <app>/dev.unchartedg2.game.Launcher --es game_dir <path> [--ez backup_saves true]
 *  [--ez prepare true] [--ez play true]. */
public class Launcher extends Activity {
    static {
        System.loadLibrary("bbprepare");
    }

    /** null on success, else the reason (game_prepare.c). */
    static native String prepareGame(String gameDir, String exe, String outDir);

    /** A value of sce_sys/param.sfo as text, null if missing. */
    static native String sfoValue(String gameDir, String key);

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
        {"960x540", "960x540"},
        {"1280x720", "1280x720 (recommended)"},
        {"1920x1080", "1920x1080, the game's own (much slower)"},
    };
    private static final String[] MODULES = {"sce_module/libc.prx", "sce_module/libSceFios2.prx", "sce_sys/param.sfo"};
    private static final int PICK_FOLDER = 1;

    private SharedPreferences prefs;
    private File files;
    private TextView folderText, folderInfo, prepareInfo, statusText;
    private EditText pathEdit, extraEnv;
    private Button accessButton, prepareButton, playButton;
    private RadioGroup resolution;
    private RadioGroup fpsLimit;
    private RadioGroup touchPad;
    private CheckBox turnip, showFps, uncapped;
    private volatile boolean busy;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
        File d = getExternalFilesDir(null);
        files = d != null ? d : getFilesDir();
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
        refresh(); // back from the system's "all files access" screen
    }

    // ---- views ----
    // Plain widgets styled by hand (no AndroidX in this build): the theme's accent colour, sections
    // as rounded cards, a filled Play button, outlined secondary buttons, muted explanations.

    private int accent, muted, cardColor;

    private void initPalette() {
        TypedValue v = new TypedValue();
        accent = 0xFF3D8BFF;
        if (getTheme().resolveAttribute(android.R.attr.colorAccent, v, true)) {
            if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT)
                accent = v.data;
            else if (v.resourceId != 0) accent = getColor(v.resourceId);
        }
        boolean night = (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK)
                == Configuration.UI_MODE_NIGHT_YES;
        muted = night ? 0xB3FFFFFF : 0x99000000;
        cardColor = night ? 0x16FFFFFF : 0x0D000000;
    }

    private TextView text(LinearLayout parent, String s, float size, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(size);
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
        bg.setCornerRadius(dp(16));
        c.setBackground(bg);
        c.setPadding(dp(18), dp(12), dp(18), dp(16));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                                                     ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(12);
        parent.addView(c, lp);
        if (title != null) text(c, title, 17, true).setPadding(0, 0, 0, dp(4));
        return c;
    }

    private LinearLayout row(LinearLayout parent) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        parent.addView(r);
        return r;
    }

    private Button button(LinearLayout parent, String s, View.OnClickListener l) {
        Button b = new Button(this, null, android.R.attr.borderlessButtonStyle);
        b.setText(s);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        b.setTextColor(new ColorStateList(new int[][] {{-android.R.attr.state_enabled}, {}},
                                          new int[] {muted, accent}));
        b.setMinHeight(dp(44));
        b.setMinimumHeight(dp(44));
        b.setPadding(dp(18), 0, dp(18), 0);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.TRANSPARENT);
        bg.setStroke(dp(1), (accent & 0x00FFFFFF) | 0x66000000);
        bg.setCornerRadius(dp(22));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf((accent & 0x00FFFFFF) | 0x33000000), bg, null));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                                                                     ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(8);
        lp.rightMargin = dp(8);
        parent.addView(b, lp);
        return b;
    }

    private Button primaryButton(LinearLayout parent, String s, View.OnClickListener l) {
        Button b = new Button(this, null, android.R.attr.borderlessButtonStyle);
        b.setText(s);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        b.setTextSize(18);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setTextColor(Color.WHITE);
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
        c.setText(s);
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
        ScrollView scroll = new ScrollView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(20), dp(20), dp(28));
        scroll.addView(root);

        text(root, "Uncharted: The Nathan Drake Collection", 24, true);
        String version = "";
        try {
            version = " " + getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
        }
        note(root, "uncharted-android" + version + ": uncharted_pc for Android, the PS4 game's code on "
                 + "FEXCore and the shadPS4 renderer. It needs your own copy of the game, already "
                 + "decrypted (a dump of CUSA02344, CUSA02320 or CUSA02343, version 01.02 for the "
                 + "sizes below 1920x1080).");

        LinearLayout folder = card(root, "Game folder");
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
        pathEdit.setHint("or type the path, e.g. /storage/XXXX-XXXX/PS4/CUSA02344");
        pathEdit.setSingleLine(true);
        pathEdit.setTextSize(14);
        pathEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        folder.addView(pathEdit);
        button(folder, "Use this path", v -> {
            String p = pathEdit.getText().toString().trim();
            if (!p.isEmpty()) setFolder(p);
            refresh();
        });

        LinearLayout prepareCard = card(root, "Game files");
        prepareInfo = note(prepareCard, "");
        prepareButton = button(prepareCard, "Prepare", v -> prepare(false));

        LinearLayout page = root;
        root = card(page, "Settings"); // the settings widgets below go into the card
        text(root, "The game renders at:", 14, false);
        resolution = new RadioGroup(this);
        String chosen = chosenResolution();
        for (String[] r : RESOLUTIONS) {
            RadioButton b = new RadioButton(this);
            b.setText(r[1]);
            b.setId(View.generateViewId());
            b.setTag(r[0]);
            resolution.addView(b);
            if (r[0].equals(chosen)) resolution.check(b.getId());
        }
        resolution.setOnCheckedChangeListener((g, id) -> {
            RadioButton b = g.findViewById(id);
            if (b != null) prefs.edit().putString("resolution", (String) b.getTag()).apply();
        });
        root.addView(resolution);
        text(root, "Below 1920x1080 the game's size is set by a few byte patches made here from your own "
                 + "executables (the app ships recipes, no code of the game) and the movies play through the "
                 + "port's decoder. FSR 3.1 and TAA (the panel in the game) are anti-aliasing at that size; the port "
                 + "scales the picture to the screen.", 14, false);
        boolean kgsl = new File("/dev/kgsl-3d0").exists();
        turnip = check(root, "Turnip Vulkan driver (Adreno; recommended there)", "turnip", kgsl);
        if (!kgsl)
            text(root, "No Adreno GPU (KGSL) found: the system's Vulkan driver will be used, which has not been tested "
                     + "with these games.", 14, false);
        ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
        ((ActivityManager) getSystemService(ACTIVITY_SERVICE)).getMemoryInfo(memory);
        // An 8 GB device shows ~7.1-7.5 GiB to apps (the G2: 7.15), a 6 GB one ~5.5.
        if (memory.totalMem < (6656L << 20))
            text(root, "This device has less than 8 GB of RAM: the games use about 6 GB (tested with 8 GB), "
                     + "so the system may close them.", 14, false);
        uncapped = check(root, "Show each frame at once (up to the display's refresh rate)", "uncapped", true);
        showFps = check(root, "Show the frame rate", "show_fps", false);
        text(root, "Frame rate limit:", 14, false);
        fpsLimit = new RadioGroup(this);
        fpsLimit.setOrientation(RadioGroup.HORIZONTAL);
        // Shared with the in-game panel through bbport.ini: what the panel last chose is shown.
        String current = iniGet("fps_limit");
        if (current == null || current.equals("auto")) current = prefs.getString("fps_limit", defaultFpsLimit());
        for (String l : new String[] {"30", "60", "120", "Off"}) {
            RadioButton b = new RadioButton(this);
            b.setText(l);
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
        text(root, "On-screen gamepad:", 14, false);
        touchPad = new RadioGroup(this);
        touchPad.setOrientation(RadioGroup.HORIZONTAL);
        String pad = iniGet("touch_pad");
        if (pad == null) pad = prefs.getString("touch_pad", "auto");
        for (String[] o : new String[][] {{"0", "Off"}, {"auto", "Auto"}, {"1", "On"}}) {
            RadioButton b = new RadioButton(this);
            b.setText(o[1]);
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
        text(root, "More environment for the port (KEY=VALUE per line, optional):", 14, false);
        extraEnv = new EditText(this);
        extraEnv.setText(prefs.getString("extra_env", ""));
        extraEnv.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        extraEnv.setMinLines(2);
        root.addView(extraEnv);
        root = page;

        playButton = primaryButton(root, "Play", v -> play());
        statusText = note(root, "");

        LinearLayout savesCard = card(root, "Saves");
        note(savesCard, "The game's saves are in the app's own folder, which Android deletes together with the app. "
                      + "Back them up to " + backups() + " before uninstalling.");
        LinearLayout saves = row(savesCard);
        button(saves, "Back up saves", v -> {
            String done = backupSaves();
            status(done != null ? "Saves copied to " + done : "Backing up failed (no saves yet, or no access).");
        });
        button(saves, "Restore saves", v -> restoreSaves());
        button(root, "Licenses", v -> showLicenses());
        setContentView(scroll);
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
        if (request != PICK_FOLDER || result != RESULT_OK || data == null || data.getData() == null) return;
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
        folderText.setText(dir.isEmpty() ? "(none)" : dir);
        String problem = access ? problems(dir) : "Allow the app to read all files first (the game's folder is opened by path).";
        if (problem == null) {
            // (APP_VER is the base game's even when the folder has the patched executables.)
            String title = sfoValue(dir, "TITLE"), id = sfoValue(dir, "TITLE_ID");
            folderInfo.setText((title != null ? title : "?") + "  " + (id != null ? id : "?"));
        } else {
            folderInfo.setText(problem);
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
        prepareInfo.setText(problem != null ? "-" : ready ? "Prepared for this folder." + presets
                                                     : "Not prepared yet: the app makes the loader's files from the game (a few seconds).");
        prepareButton.setEnabled(problem == null && !busy);
        playButton.setEnabled(ready && !busy);
    }

    private void status(String s) {
        runOnUiThread(() -> statusText.setText(s));
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
                statusText.setText(failure == null ? "The game's files are ready." : "Preparing failed: " + failure);
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
    private byte[] makePreset(String recipe, File boot, int width, int height) {
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

    /** The CPUs of every cluster but the slowest (the port's GPU threads: BB_RENDER_CPUS). */
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
        long slowest = Long.MAX_VALUE;
        for (long f : freqs) slowest = Math.min(slowest, f);
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < cpus.size(); i++)
            if (freqs.get(i) > slowest) b.append(b.length() > 0 ? "," : "").append(cpus.get(i));
        return b.length() > 0 ? b.toString() : null;
    }

    /** The frame rate limit to offer first: the display's rate when it is 120 Hz, else 60. */
    private String defaultFpsLimit() {
        float hz = 60;
        try { hz = getWindowManager().getDefaultDisplay().getRefreshRate(); } catch (Exception e) { }
        return hz >= 110 ? "120" : "60";
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
            status("Prepare the game's files first.");
            return;
        }
        prefs.edit().putString("extra_env", extraEnv.getText().toString()).apply();
        String dir = folder(), d = files.getAbsolutePath();
        String[] size = chosenResolution().split("x");
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
                .append("--timeout\n0\n--log\n").append(d).append("/bb-probe.log\n");
            if (preset != null) args.append("--patches\n").append(preset).append('\n');
            writeText(new File(files, "args-" + g[0] + ".txt"), args.toString());
            if (g[0].equals("u1")) writeText(new File(files, "args.txt"), args.toString());
            // The movie decoder's picture size goes with a size below 1080p (port 0015).
            env[i] = preset != null && below1080 ? "BB_VDEC_CONTENT=" + width + "x" + height + "\n" : "-BB_VDEC_CONTENT\n";
        }
        StringBuilder e = new StringBuilder();
        e.append("# Written by the launcher on every start; edit the settings there.\n");
        e.append("BB_FULLSCREEN=1\nBB_FPS_LOG=1\nBB_MIDFRAME_FLUSH=10\nBB_DEFERRED_SUBMIT=1\nBB_SKIN_BATCH=1\n");
        e.append("BB_CS_SPLIT=256:8f067540\nBB_OBJECT_MOTION=0\nBB_THREAD_NICE=0\n");
        e.append("BB_DUMP_REQUEST=").append(d).append("/dump-request\n");
        String cpus = fastCpus();
        if (cpus != null) e.append("BB_RENDER_CPUS=").append(cpus).append('\n');
        if (turnip.isChecked()) e.append("BB_VK_DRIVER=libvulkan_freedreno.so\n");
        if (uncapped.isChecked()) e.append("BB_VBLANK_HZ=0\n");
        if (showFps.isChecked()) e.append("BB_SHOW_FPS=1\n");
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

    /** The newest backup back into files/user, after backing up the current saves. */
    private void restoreSaves() {
        File[] list = backups().listFiles(f -> f.isDirectory() && f.getName().startsWith("saves-"));
        if (list == null || list.length == 0) {
            status("No backups in " + backups());
            return;
        }
        java.util.Arrays.sort(list);
        File newest = list[list.length - 1];
        new android.app.AlertDialog.Builder(this).setTitle("Restore saves")
            .setMessage("Replace the app's saves with " + newest.getName() + "? The current ones are backed up first.")
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
        t.setText(text.isEmpty() ? "This build has no license file (build.sh PUBLIC=1 with licenses.sh's output)." : text);
        t.setTextSize(12);
        t.setTypeface(Typeface.MONOSPACE);
        t.setPadding(dp(16), dp(16), dp(16), dp(16));
        t.setTextIsSelectable(true);
        scroll.addView(t);
        new android.app.AlertDialog.Builder(this).setTitle("Licenses").setView(scroll)
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
