package dev.unchartedg2.game;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.Typeface;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/** Translucent PlayStation-style controls with a persistent, draggable layout. */
final class TouchControllerOverlay extends View {
    private static final String[] NAMES={
        "l2","l1","touchpad","options","r1","r2",
        "up","left","right","down","triangle","circle","cross","square",
        "leftstick","l3","r3","rightstick"
    };
    private static final float[][] DEFAULT={
        {87,105},{213,105},{640,105},{773,105},{1067,105},{1193,105},
        {160,395},{90,465},{230,465},{160,535},
        {1120,405},{1190,475},{1120,545},{1050,475},
        {320,612},{420,614},{860,614},{960,612}
    };
    // The approved Uncharted action layout follows the GoW III geometry.
    // Positions are independent of the original PlayStation layout.
    private static final float[][] ACTION_DEFAULT={
        // Unlike the physical PlayStation diamond, these are practical mobile
        // gameplay zones: interact top-right, melee on the inner right column,
        // cover outer right, jump low-right, camera stick free below them.
        // Shoulder shortcuts remain high at the edges.
        {260,108},{112,108},{585,65},{690,65},{1016,108},{1166,108},
        // D-pad is secondary; well above the movement thumb.
        {145,242},{85,302},{205,302},{145,362},
        // Interact (triangle), cover (circle), jump (cross), melee (square).
        {1125,285},{1190,455},{1050,565},{980,402},
        // Independent movement/camera zones; L3/R3 are stick double-taps.
        {142,570},{210,535},{905,535},{825,575}
    };
    private static final String ACTION_LAYOUT_VERSION_KEY="overlay_uncharted_layout_version";
    private static final int ACTION_LAYOUT_VERSION=4;
    private final PadBridge pad;
    private final SharedPreferences preferences;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    // Cache detailed action silhouettes using steel/sandstone colors, not the old GoW gold.
    // This paint is separate from the circles/labels so bitmap alpha is not
    // affected by the current stroke color or style.
    private final Paint glyphPaint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
    private final UnchartedActionGlyphs glyphs=new UnchartedActionGlyphs();
    private float sizeMultiplier;
    private float surfaceOpacity;
    private boolean unchartedStyle;
    private final Map<String,PointF> centers=new LinkedHashMap<>();
    private final SparseArray<String> controls=new SparseArray<>();
    private final SparseArray<PointF> positions=new SparseArray<>();
    // Virtual L3/R3 are activated by a deliberate double-tap on their own
    // sticks in the Uncharted style. No invented L3+R3 combined button.
    private final SparseArray<PointF> touchDownPositions=new SparseArray<>();
    private final SparseArray<Long> touchDownTimes=new SparseArray<>();
    private long lastLeftTapMs,lastRightTapMs;
    private boolean leftStickClicked,rightStickClicked;
    private final Runnable clearLeftClick=()->{leftStickClicked=false;publish();};
    private final Runnable clearRightClick=()->{rightStickClicked=false;publish();};
    private final Set<String> pressed=new LinkedHashSet<>();
    private boolean editing;
    private int editPointer=-1;
    // Dedicated pointer for the left-edge menu swipe; never enters pad state.
    private int menuSwipePointer=-1;
    private float menuSwipeStartX,menuSwipeStartY;
    private String dragged;
    private Runnable onEditDone;

    TouchControllerOverlay(Context context,PadBridge pad) {
        super(context); this.pad=pad;
        preferences=context.getSharedPreferences("uncharted_touch",Context.MODE_PRIVATE);
        loadStyle();
        setContentDescription("On-screen PlayStation controller, GoW III / inFAMOUS layout");
    }
    static final String CLASSIC_STYLE="classic";
    static final String UNCHARTED_STYLE="uncharted";
    boolean isEditing() { return editing; }
    boolean isUnchartedStyle() { return unchartedStyle; }
    String getStyle() { return unchartedStyle ? UNCHARTED_STYLE : CLASSIC_STYLE; }
    private String positionPrefix() { return unchartedStyle ? "overlay_uncharted_" : "overlay_"; }
    private String sizeKey() { return unchartedStyle ? "overlay_uncharted_size" : "overlay_size"; }
    private String opacityKey() { return unchartedStyle ? "overlay_uncharted_opacity" : "overlay_opacity"; }
    private void loadStyle() {
        unchartedStyle=UNCHARTED_STYLE.equals(preferences.getString("overlay_style",UNCHARTED_STYLE));
        if(unchartedStyle && preferences.getInt(ACTION_LAYOUT_VERSION_KEY,0)<ACTION_LAYOUT_VERSION) {
            // Bad first-generation positions could persist after an APK update.
            // Migrate only the Uncharted layout. The classic PS pad is untouched.
            SharedPreferences.Editor migration=preferences.edit();
            for(String name:NAMES)
                migration.remove("overlay_uncharted_x_"+name)
                         .remove("overlay_uncharted_y_"+name);
            migration.putFloat("overlay_uncharted_size",1.0f);
            migration.putInt(ACTION_LAYOUT_VERSION_KEY,ACTION_LAYOUT_VERSION).apply();
        }
        sizeMultiplier=Math.max(.75f,Math.min(unchartedStyle?1.30f:1.65f,
            preferences.getFloat(sizeKey(),unchartedStyle?1.0f:1.20f)));
        surfaceOpacity=Math.max(.10f,Math.min(.9f,preferences.getFloat(opacityKey(),unchartedStyle?.15f:.40f)));
        float[][] defaults=unchartedStyle?ACTION_DEFAULT:DEFAULT;
        centers.clear();
        for (int i=0;i<NAMES.length;i++) {
            String name=NAMES[i],prefix=positionPrefix();
            centers.put(name,new PointF(preferences.getFloat(prefix+"x_"+name,defaults[i][0]),
                                        preferences.getFloat(prefix+"y_"+name,defaults[i][1])));
        }
    }
    void syncStyle() {
        String saved=preferences.getString("overlay_style",UNCHARTED_STYLE);
        if (!saved.equals(getStyle())) { release(); loadStyle(); invalidate(); }
    }
    void setStyle(String style) {
        if (!CLASSIC_STYLE.equals(style) && !UNCHARTED_STYLE.equals(style)) return;
        if (style.equals(getStyle())) return;
        release();
        preferences.edit().putString("overlay_style",style).apply();
        loadStyle(); invalidate();
    }
    void toggleStyle() { setStyle(unchartedStyle?CLASSIC_STYLE:UNCHARTED_STYLE); }
    void setSize(float size) {
        sizeMultiplier=Math.max(.75f,Math.min(unchartedStyle?1.3f:1.65f,size));
        preferences.edit().putFloat(sizeKey(),sizeMultiplier).apply();
        release();invalidate();
    }
    void setOpacity(float opacity) {
        surfaceOpacity=Math.max(.1f,Math.min(.9f,opacity));
        preferences.edit().putFloat(opacityKey(),surfaceOpacity).apply();
        invalidate();
    }
    private PointF center(String name) { return centers.get(name); }
    // Round buttons always use a UNIFORM scale. The original Classic style
    // keeps its exact 16:9 canvas. For Uncharted mode, expand just the X
    // coordinates to use the entire width on 20:9/21:9 phones. Scaling the
    // whole Canvas nonuniformly would produce oval controls and wrong hitboxes.
    private float scale() { return Math.max(1e-3f,Math.min(getWidth()/1280f,getHeight()/720f)); }
    private float offsetX() { return (getWidth()-1280f*scale())/2f; }
    private float offsetY() { return (getHeight()-720f*scale())/2f; }
    private float actionHorizontalScale() {
        return Math.max(1f,getWidth()/(1280f*scale()));
    }
    private float actionDisplayX(float x) {
        return 640f+(x-640f)*actionHorizontalScale();
    }
    private float actionLogicalX(float x) {
        return 640f+(x-640f)/actionHorizontalScale();
    }
    private float screenLayoutX(MotionEvent e,int index) {
        return (e.getX(index)-offsetX())/scale();
    }
    private float layoutX(MotionEvent e,int index) {
        float x=screenLayoutX(e,index);
        return unchartedStyle?actionLogicalX(x):x;
    }
    private float layoutY(MotionEvent e,int index) { return (e.getY(index)-offsetY())/scale(); }
    private PointF actionCenter(String name) {
        PointF p=center(name);
        return new PointF(actionDisplayX(p.x),p.y);
    }
    private PointF actionDpadCenter() {
        PointF p=dpadCenter();
        return new PointF(actionDisplayX(p.x),p.y);
    }
    private static float distance(float x,float y,PointF p) { return (float)Math.hypot(x-p.x,y-p.y); }
    private static boolean stick(String name) { return name.equals("leftstick") || name.equals("rightstick"); }
    private static boolean rectangle(String name) {
        return name.equals("l1") || name.equals("l2") || name.equals("r1") ||
               name.equals("r2") || name.equals("touchpad") || name.equals("options");
    }
    private PointF dpadCenter() {
        PointF u=center("up"),l=center("left"),r=center("right"),d=center("down");
        return new PointF((u.x+l.x+r.x+d.x)/4f,(u.y+l.y+r.y+d.y)/4f);
    }
    private float actionDistance(float x,float y,PointF logicalCenter) {
        // Hit detection measures actual screen distance, not the unwarped
        // logical X axis: round drawings must also have round tap targets.
        return (float)Math.hypot((x-logicalCenter.x)*actionHorizontalScale(),
                                 y-logicalCenter.y);
    }
    private String hit(float x,float y) {
        PointF dpad=dpadCenter();
        if (unchartedStyle &&
            actionDistance(x,y,dpad)<58*sizeMultiplier*DPAD_VISUAL_SCALE) return "dpad";
        if (!unchartedStyle && !editing && distance(x,y,dpad)<112*sizeMultiplier &&
            distance(center("up").x,center("up").y,dpad)<115*sizeMultiplier &&
            distance(center("left").x,center("left").y,dpad)<115*sizeMultiplier &&
            distance(center("right").x,center("right").y,dpad)<115*sizeMultiplier &&
            distance(center("down").x,center("down").y,dpad)<115*sizeMultiplier) return "dpad";
        String winner=null; float best=Float.MAX_VALUE;
        for (String name:NAMES) {
            if (unchartedStyle && (name.equals("up") || name.equals("down") ||
                                    name.equals("left") || name.equals("right") ||
                                    name.equals("l3") || name.equals("r3"))) continue;
            PointF p=center(name);
            float dx=Math.abs(x-p.x),dy=Math.abs(y-p.y);
            boolean inside;
            if (unchartedStyle) {
                float radius=stick(name)?84:
                             name.equals("touchpad")||name.equals("options")?38:49;
                inside=actionDistance(x,y,p)<radius*sizeMultiplier;
            } else {
                inside=rectangle(name) ? dx<(name.equals("touchpad") ? 72 : 62)*sizeMultiplier && dy<38*sizeMultiplier
                    : distance(x,y,p)<(stick(name) ? 70 : name.equals("l3") || name.equals("r3") ? 39 : 48)*sizeMultiplier;
            }
            float visibleDistance=unchartedStyle?actionDistance(x,y,p):distance(x,y,p);
            if (inside && visibleDistance<best) { best=visibleDistance; winner=name; }
        }
        return winner;
    }
    private static int axis(float v) { return Math.max(0,Math.min(255,Math.round((v+1f)*127.5f))); }

    private void publish() {
        pressed.clear();
        int lx=128,ly=128,rx=128,ry=128,l2=0,r2=0;
        boolean left=false,right=false;
        for (int i=0;i<controls.size();i++) {
            String control=controls.valueAt(i);
            PointF p=positions.get(controls.keyAt(i));
            if (p==null) continue;
            if (control.equals("dpad")) {
                PointF c=dpadCenter();
                // Hit testing/drawing use physical X on wide screens. Resolve
                // directions in those same units, with only a tiny central deadzone.
                final float dx=(p.x-c.x)*(unchartedStyle?actionHorizontalScale():1f);
                final int directions=DpadResolver.resolve(dx,p.y-c.y,sizeMultiplier);
                if ((directions & DpadResolver.UP)!=0) pressed.add("up");
                if ((directions & DpadResolver.RIGHT)!=0) pressed.add("right");
                if ((directions & DpadResolver.DOWN)!=0) pressed.add("down");
                if ((directions & DpadResolver.LEFT)!=0) pressed.add("left");
            } else if (control.equals("leftstick")) {
                left=true; PointF c=center(control);
                // Normalize against the *drawn* distance on widescreen so
                // thumb movement retains identical sensitivity on both pads.
                float rangeX=56f*sizeMultiplier/(unchartedStyle?actionHorizontalScale():1f);
                lx=axis(Math.max(-1f,Math.min(1f,(p.x-c.x)/rangeX)));
                ly=axis(Math.max(-1f,Math.min(1f,(p.y-c.y)/(56f*sizeMultiplier))));
            } else if (control.equals("rightstick")) {
                right=true; PointF c=center(control);
                float rangeX=56f*sizeMultiplier/(unchartedStyle?actionHorizontalScale():1f);
                rx=axis(Math.max(-1f,Math.min(1f,(p.x-c.x)/rangeX)));
                ry=axis(Math.max(-1f,Math.min(1f,(p.y-c.y)/(56f*sizeMultiplier))));
            } else if (control.equals("l2")) { l2=255; pressed.add(control); }
            else if (control.equals("r2")) { r2=255; pressed.add(control); }
            else if (control.equals("touchpad")) pressed.add("touchpad_right"); /* 右侧点击:打开游戏暂停菜单,进而可进存档列表 */
            else pressed.add(control);
        }
        if (unchartedStyle) {
            if(leftStickClicked)pressed.add("l3");
            if(rightStickClicked)pressed.add("r3");
        }
        pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2);
        invalidate();
    }
    void release() {
        menuSwipePointer=-1; controls.clear(); positions.clear();
        touchDownPositions.clear(); touchDownTimes.clear();
        lastLeftTapMs=lastRightTapMs=0;
        leftStickClicked=rightStickClicked=false;
        removeCallbacks(clearLeftClick);removeCallbacks(clearRightClick);
        publish();
    }
    private void clickStickIfDoubleTapped(int id,long when) {
        if(!unchartedStyle)return;
        String target=controls.get(id);
        if(!"leftstick".equals(target)&&!"rightstick".equals(target))return;
        PointF start=touchDownPositions.get(id),last=positions.get(id);
        Long begin=touchDownTimes.get(id);
        if(start==null||last==null||begin==null||when-begin>230 ||
           actionDistance(last.x,last.y,start)>18*sizeMultiplier)return;
        if("leftstick".equals(target)){
            if(lastLeftTapMs>0&&when-lastLeftTapMs<=365){
                leftStickClicked=true;lastLeftTapMs=0;
                removeCallbacks(clearLeftClick);postDelayed(clearLeftClick,135);
            }else lastLeftTapMs=when;
        }else{
            if(lastRightTapMs>0&&when-lastRightTapMs<=365){
                rightStickClicked=true;lastRightTapMs=0;
                removeCallbacks(clearRightClick);postDelayed(clearRightClick,135);
            }else lastRightTapMs=when;
        }
    }
    void edit(Runnable done) { release(); editing=true; onEditDone=done; invalidate(); }
    private void finishEdit() {
        editPointer=-1; dragged=null; editing=false; invalidate();
        if (onEditDone!=null) onEditDone.run();
        onEditDone=null;
    }
    void resetLayout() {
        SharedPreferences.Editor editor=preferences.edit();
        for (int i=0;i<NAMES.length;i++) {
            String name=NAMES[i]; float[][] defaults=unchartedStyle?ACTION_DEFAULT:DEFAULT;
            centers.get(name).set(defaults[i][0],defaults[i][1]);
            editor.remove(positionPrefix()+"x_"+name).remove(positionPrefix()+"y_"+name);
        }
        editor.apply(); release(); invalidate();
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int action=e.getActionMasked(),at=e.getActionIndex(),id=e.getPointerId(at);
        if (editing) {
            if (action==MotionEvent.ACTION_CANCEL) { editPointer=-1; dragged=null; return true; }
            if (action==MotionEvent.ACTION_DOWN) {
                float x=layoutX(e,at),y=layoutY(e,at);
                // Toolbar stays at fixed 16:9 positions even when gameplay
                // controls expand horizontally across the full phone screen.
                float toolbarX=screenLayoutX(e,at);
                if (y<75f) {
                    if (toolbarX>548 && toolbarX<732) { finishEdit(); return true; }
                    if (toolbarX>325 && toolbarX<430) { setSize(sizeMultiplier-.10f); return true; }
                    if (toolbarX>432 && toolbarX<540) { setSize(sizeMultiplier+.10f); return true; }
                    if (toolbarX>745 && toolbarX<945) { toggleStyle(); return true; }
                    if (toolbarX>951 && toolbarX<1124) { resetLayout(); return true; }
                }
                dragged=hit(x,y); editPointer=dragged==null ? -1 : id;
            } else if (action==MotionEvent.ACTION_MOVE && editPointer>=0 && dragged!=null) {
                int index=e.findPointerIndex(editPointer);
                float barX=offsetX()/scale(),barY=offsetY()/scale();
                if (index>=0) {
                    float tx=Math.max(36-barX,Math.min(1244+barX,layoutX(e,index)));
                    float ty=Math.max(36-barY,Math.min(684+barY,layoutY(e,index)));
                    if (unchartedStyle && dragged.equals("dpad")) {
                        PointF c=dpadCenter();float dx=tx-c.x,dy=ty-c.y;
                        for(String dir:new String[]{"up","down","left","right"})
                            centers.get(dir).offset(dx,dy);
                    } else center(dragged).set(tx,ty);
                }
                invalidate();
            } else if ((action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP)
                       && id==editPointer && dragged!=null) {
                SharedPreferences.Editor editor=preferences.edit();
                if (unchartedStyle && dragged.equals("dpad")) {
                    for(String dir:new String[]{"up","down","left","right"}) {
                        PointF p=center(dir);
                        editor.putFloat(positionPrefix()+"x_"+dir,p.x)
                              .putFloat(positionPrefix()+"y_"+dir,p.y);
                    }
                } else {
                    PointF p=center(dragged);
                    editor.putFloat(positionPrefix()+"x_"+dragged,p.x)
                          .putFloat(positionPrefix()+"y_"+dragged,p.y);
                }
                editor.apply();
                editPointer=-1; dragged=null;
            }
            return true;
        }
        if (action==MotionEvent.ACTION_CANCEL) { release(); return true; }
        if (action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            float x=layoutX(e,at),y=layoutY(e,at);
            // The first 40 logical units are outside the D-pad/buttons.
            // This gesture is not forwarded as a PS button or camera motion.
            // L2 and draggable controls may extend into this strip. Do
            // not hijack a finger if it actually landed on a PS button.
            // A full-height left-edge zone, including the lower part of
            // landscape screens. Never steal touches from the D-pad/stick.
            if (menuSwipePointer<0 && x<=Math.min(getWidth()*.10f,
                    getResources().getDisplayMetrics().density*96f) && hit(x,y)==null) {
                menuSwipePointer=id;
                menuSwipeStartX=x;
                menuSwipeStartY=y;
                return true;
            }
            String name=hit(x,y);
            if (name==null) return true;
            controls.put(id,name);
            positions.put(id,new PointF(x,y));
            touchDownPositions.put(id,new PointF(x,y));
            touchDownTimes.put(id,e.getEventTime());
        } else if (action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
            if (id==menuSwipePointer) menuSwipePointer=-1;
            clickStickIfDoubleTapped(id,e.getEventTime());
            controls.remove(id); positions.remove(id);
            touchDownPositions.remove(id);touchDownTimes.remove(id);
        }
        if (action==MotionEvent.ACTION_MOVE) {
            if (menuSwipePointer>=0) {
                int pointer=e.findPointerIndex(menuSwipePointer);
                if (pointer>=0) {
                    float x=layoutX(e,pointer),y=layoutY(e,pointer);
                    float density=getResources().getDisplayMetrics().density;
                    final float distance=Math.max(density*85f,getWidth()*.065f);
                    if (Math.abs(y-menuSwipeStartY)>density*105f &&
                        x-menuSwipeStartX<distance)
                        menuSwipePointer=-1;
                    else if (x-menuSwipeStartX>=distance &&
                             Math.abs(y-menuSwipeStartY)<=density*105f) {
                        menuSwipePointer=-1;
                        release();
                        GameActivity.nativeToggleMenu();
                        return true;
                    }
                }
            }
            for (int i=0;i<e.getPointerCount();i++) {
                PointF p=positions.get(e.getPointerId(i));
                if (p!=null) p.set(layoutX(e,i),layoutY(e,i));
            }
        }
        publish(); return true;
    }

    // GoW III and inFAMOUS use the original translucent PlayStation controls.
    // The optional action overlay draws native Canvas vectors, not OS-dependent
    // emoji (the previous font glyphs rendered inconsistently on Android).
    private static String actionLabel(String name) {
        switch(name) {
            case "triangle": return "PEGAR";
            case "square": return "SOCO";
            case "cross": return "PULAR";
            case "circle": return "COBERT.";
            case "l2": return "MIRAR";
            case "r2": return "ATIRAR";
            case "l1": return "GRANADA";
            case "r1": return "RECAR.";
            default: return null;
        }
    }
    // Compact action pictograms, not emoji or generic shapes.
    private void drawActionGlyph(Canvas c,String name,float x,float y,float size) {
        c.save(); c.translate(x,y); c.scale(size,size);
        paint.setColor(ACTION_GLYPH);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(.13f);
        paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);
        android.graphics.Path path=new android.graphics.Path();
        switch(name) {
            case "l1": // recognisable grenade with pin and grooves
                paint.setStyle(Paint.Style.FILL);
                c.drawOval(-.43f,-.27f,.43f,.72f,paint);
                paint.setColor(0xff304650);paint.setStyle(Paint.Style.STROKE);
                c.drawLine(-.33f,.03f,.33f,.03f,paint);
                c.drawLine(-.30f,.32f,.30f,.32f,paint);
                paint.setColor(ACTION_GLYPH);
                c.drawRoundRect(-.17f,-.49f,.17f,-.23f,.05f,.05f,paint);
                c.drawCircle(.31f,-.55f,.17f,paint);
                break;
            case "l2": // aim sight
                c.drawCircle(0,0,.36f,paint);
                c.drawLine(-.75f,0,-.41f,0,paint);c.drawLine(.41f,0,.75f,0,paint);
                c.drawLine(0,-.75f,0,-.41f,paint);c.drawLine(0,.41f,0,.75f,paint);
                c.drawCircle(0,0,.055f,paint);
                break;
            case "r1": // reload: bullets and directional arrow
                paint.setStyle(Paint.Style.FILL);
                for(int i=-1;i<=1;i++)c.drawRoundRect(i*.25f-.07f,-.22f,
                    i*.25f+.07f,.43f,.045f,.045f,paint);
                paint.setStyle(Paint.Style.STROKE);
                c.drawArc(-.66f,-.70f,.66f,.70f,-140f,235f,false,paint);
                path.moveTo(.52f,-.04f);path.lineTo(.69f,.30f);
                path.lineTo(.28f,.20f);c.drawPath(path,paint);
                break;
            case "r2": // pistol silhouette, rather than an ambiguous sunburst
                paint.setStyle(Paint.Style.FILL);
                path.moveTo(-.74f,-.23f);path.lineTo(.63f,-.23f);
                path.lineTo(.70f,.05f);path.lineTo(.12f,.05f);
                path.lineTo(-.10f,.65f);path.lineTo(-.49f,.65f);
                path.lineTo(-.36f,.06f);path.lineTo(-.74f,.06f);
                path.close();c.drawPath(path,paint);
                paint.setColor(0xff304650);paint.setStyle(Paint.Style.STROKE);
                c.drawLine(-.41f,-.08f,.33f,-.08f,paint);
                break;
            case "triangle": // open palm reaching for an object
                paint.setStyle(Paint.Style.FILL);
                path.moveTo(-.48f,-.16f);path.lineTo(-.47f,-.57f);
                path.quadTo(-.42f,-.75f,-.28f,-.62f);
                path.lineTo(-.19f,-.32f);path.lineTo(-.17f,-.75f);
                path.quadTo(-.13f,-.89f,.02f,-.80f);
                path.lineTo(.08f,-.38f);
                path.quadTo(.38f,-.70f,.49f,-.51f);
                path.quadTo(.57f,-.39f,.45f,-.20f);
                path.lineTo(.20f,.15f);path.lineTo(.20f,.70f);
                path.lineTo(-.31f,.70f);path.close();
                c.drawPath(path,paint);
                break;
            case "square": // solid clenched fist with visible knuckles
                paint.setStyle(Paint.Style.FILL);
                c.drawRoundRect(-.66f,-.32f,.61f,.31f,.16f,.16f,paint);
                c.drawRoundRect(-.41f,.13f,.40f,.68f,.13f,.13f,paint);
                paint.setStyle(Paint.Style.STROKE);paint.setColor(0xff304650);
                for(int i=-1;i<=1;i++)c.drawLine(i*.29f,-.24f,i*.29f,.10f,paint);
                break;
            case "circle": // crouching figure behind cover
                paint.setStyle(Paint.Style.FILL);
                c.drawCircle(-.33f,-.39f,.18f,paint);
                paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(.21f);
                c.drawLine(-.24f,-.15f,-.10f,.20f,paint);
                c.drawLine(-.11f,.20f,-.51f,.50f,paint);
                c.drawLine(-.07f,.21f,.30f,.55f,paint);
                c.drawLine(-.23f,-.07f,.19f,.04f,paint);
                paint.setStrokeWidth(.12f);
                c.drawLine(.40f,-.66f,.40f,.71f,paint);
                c.drawLine(.40f,.12f,.78f,.12f,paint);
                break;
            case "cross": // jumping/climbing figure
                paint.setStyle(Paint.Style.FILL);
                c.drawCircle(-.07f,-.56f,.16f,paint);
                paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(.21f);
                c.drawLine(-.04f,-.28f,.14f,.13f,paint);
                c.drawLine(.02f,-.16f,-.47f,-.27f,paint);
                c.drawLine(.02f,-.16f,.48f,-.54f,paint);
                c.drawLine(.14f,.13f,-.42f,.60f,paint);
                c.drawLine(.14f,.13f,.60f,.47f,paint);
                break;
            default: break;
        }
        c.restore();paint.setStyle(Paint.Style.FILL);
        paint.setStrokeCap(Paint.Cap.BUTT);paint.setStrokeJoin(Paint.Join.MITER);
    }

    // Identical labels and hitboxes as the original PlayStation layout.
    private void circle(Canvas c,String name,float r,String label,int accent) {
        PointF p=center(name);
        r*=sizeMultiplier;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(name.equals(dragged)?0xffe8dfc9:classicFill(pressed.contains(name)));
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(3); paint.setColor(accent);
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.FILL); paint.setTypeface(Typeface.DEFAULT_BOLD);
        paint.setTextSize((label.length()>2 ? 19 : 33)*sizeMultiplier);
        paint.setTextAlign(Paint.Align.CENTER);
        c.drawText(label,p.x,p.y-(paint.ascent()+paint.descent())/2f,paint);
    }
    private void rectangle(Canvas c,String name,float width,String label) {
        PointF p=center(name);
        width*=sizeMultiplier;
        boolean lit=pressed.contains(name)||(name.equals("touchpad")&&pressed.contains("touchpad_right"));
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(name.equals(dragged)?0xffe8dfc9:classicFill(lit));
        c.drawRoundRect(p.x-width/2,p.y-25*sizeMultiplier,p.x+width/2,p.y+25*sizeMultiplier,14,14,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setColor(0xbbc6ad76); paint.setStrokeWidth(2);
        c.drawRoundRect(p.x-width/2,p.y-25*sizeMultiplier,p.x+width/2,p.y+25*sizeMultiplier,14,14,paint);
        paint.setStyle(Paint.Style.FILL); paint.setTypeface(Typeface.DEFAULT_BOLD);
        paint.setTextSize(19*sizeMultiplier); paint.setTextAlign(Paint.Align.CENTER);
        c.drawText(label,p.x,p.y-(paint.ascent()+paint.descent())/2f,paint);
    }
    private void drawStick(Canvas c,String name) {
        PointF center=center(name);
        paint.setStyle(Paint.Style.FILL); paint.setColor(name.equals(dragged)?0xffe8dfc9:classicFill(false));
        c.drawCircle(center.x,center.y,57*sizeMultiplier,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setColor(0xffa0aec0); paint.setStrokeWidth(3);
        c.drawCircle(center.x,center.y,57*sizeMultiplier,paint);
        float knobX=center.x,knobY=center.y;
        for (int i=0;i<controls.size();i++) {
            if (name.equals(controls.valueAt(i))) {
                PointF p=positions.get(controls.keyAt(i));
                if (p!=null) {
                    knobX=center.x+Math.max(-27*sizeMultiplier,Math.min(27*sizeMultiplier,p.x-center.x));
                    knobY=center.y+Math.max(-27*sizeMultiplier,Math.min(27*sizeMultiplier,p.y-center.y));
                }
                break;
            }
        }
        paint.setStyle(Paint.Style.FILL); paint.setColor(0xbba0aec0);
        c.drawCircle(knobX,knobY,28*sizeMultiplier,paint);
        paint.setColor(0xff15202c); paint.setTypeface(Typeface.DEFAULT_BOLD);
        paint.setTextAlign(Paint.Align.CENTER); paint.setTextSize(22*sizeMultiplier);
        c.drawText(name.equals("leftstick") ? "L" : "R",knobX,
                   knobY-(paint.ascent()+paint.descent())/2,paint);
    }
    // Uncharted: desaturated slate surfaces and weathered metallic edges.
    // Gold is reserved for selected-state highlights, not the entire controller.
    private static final int ACTION_EDGE=0xff8d9da2;
    private static final int ACTION_EDGE_ACTIVE=0xffc8b28a;
    private static final int ACTION_GLYPH=0xffd0d8d6;
    private static final int ACTION_GLYPH_ACTIVE=0xfff1dfba;
    private int actionFill(boolean down) {
        float a=Math.min(.95f,surfaceOpacity+(down?.19f:0f));
        return (Math.round(a*255f)<<24)|(down?0x00354c5a:0x00132230);
    }
    private int classicFill(boolean down) {
        float a=Math.min(.95f,surfaceOpacity+(down?.25f:0f));
        return (Math.round(a*255f)<<24)|(down?0x00e8dfc9:0x0030394c);
    }
    // Large, immediately recognisable pictograms for the two controls that
    // were unreadable in the real RedMagic screenshot. Both use the same gold
    // palette as GoW III, and are actually rendered in Android Canvas.
    private void drawFirePistol(Canvas c,float x,float y,float size) {
        c.save();c.translate(x,y);c.scale(size,size);
        android.graphics.Path p=new android.graphics.Path();
        paint.setStyle(Paint.Style.FILL);paint.setColor(ACTION_GLYPH);
        // A horizontal pistol slide/barrel pointed right, with a proper grip.
        p.moveTo(-.78f,-.28f);p.lineTo(.53f,-.28f);
        p.lineTo(.68f,-.14f);p.lineTo(.66f,.04f);
        p.lineTo(.15f,.09f);p.lineTo(-.06f,.68f);
        p.lineTo(-.46f,.68f);p.lineTo(-.41f,.07f);
        p.lineTo(-.78f,.05f);p.close();c.drawPath(p,paint);
        // Slide and trigger cutout give the small silhouette its gun profile.
        paint.setColor(0xff513820);paint.setStrokeWidth(.07f);
        c.drawRect(-.64f,-.13f,.30f,-.05f,paint);
        c.drawOval(-.30f,.02f,.03f,.27f,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setColor(ACTION_GLYPH);
        paint.setStrokeWidth(.09f);
        c.drawLine(-.43f,.17f,-.37f,.52f,paint);
        // Small muzzle flash is explicitly attached to the barrel's tip.
        paint.setStyle(Paint.Style.FILL);paint.setColor(ACTION_GLYPH_ACTIVE);
        p.reset();p.moveTo(.71f,-.26f);
        p.lineTo(1.07f,-.37f);p.lineTo(.92f,-.07f);
        p.lineTo(1.19f,.04f);p.lineTo(.88f,.12f);
        p.lineTo(.94f,.38f);p.lineTo(.68f,.14f);p.close();
        c.drawPath(p,paint);
        c.restore();paint.setStyle(Paint.Style.FILL);
    }
    private void drawInteractHand(Canvas c,float x,float y,float size) {
        c.save();c.translate(x,y);c.scale(size,size);
        paint.setStyle(Paint.Style.FILL);paint.setColor(ACTION_GLYPH);
        // Four separated fingers, a thumb, and a broad open palm; no thin
        // line-art that disappears at a 46-unit button radius.
        c.drawRoundRect(-.41f,-.63f,-.19f,.13f,.10f,.10f,paint);
        c.drawRoundRect(-.15f,-.86f,.08f,.13f,.11f,.11f,paint);
        c.drawRoundRect(.11f,-.73f,.34f,.15f,.10f,.10f,paint);
        c.drawRoundRect(.37f,-.52f,.59f,.28f,.10f,.10f,paint);
        android.graphics.Path palm=new android.graphics.Path();
        palm.moveTo(-.47f,-.04f);
        palm.cubicTo(-.56f,-.38f,-.80f,-.31f,-.77f,-.08f);
        palm.lineTo(-.48f,.43f);
        palm.cubicTo(-.37f,.70f,-.18f,.80f,.13f,.78f);
        palm.cubicTo(.48f,.76f,.53f,.45f,.49f,.04f);
        palm.lineTo(-.47f,-.04f);palm.close();
        c.drawPath(palm,paint);
        // Fingers remain individually readable against the gold silhouette.
        paint.setColor(0xff344955);paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(.045f);
        c.drawLine(-.18f,-.35f,-.18f,.04f,paint);
        c.drawLine(.09f,-.42f,.09f,.04f,paint);
        c.drawLine(.36f,-.30f,.36f,.11f,paint);
        c.restore();paint.setStyle(Paint.Style.FILL);
    }
    private void actionButton(Canvas c,String name,float radius,String label) {
        PointF p=actionCenter(name);
        final float r=radius*sizeMultiplier;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(actionFill(pressed.contains(name)));
        c.drawCircle(p.x,p.y,r,paint);
        paint.setColor(pressed.contains(name)?ACTION_EDGE_ACTIVE:ACTION_EDGE);paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2.3f);c.drawCircle(p.x,p.y,r,paint);
        if (name.equals("touchpad")) {
            c.drawRoundRect(p.x-r*.49f,p.y-r*.22f,p.x+r*.26f,p.y+r*.36f,
                            5f,5f,paint);
            c.drawRoundRect(p.x-r*.26f,p.y-r*.40f,p.x+r*.50f,p.y+r*.18f,
                            5f,5f,paint);
        } else if (name.equals("options")) {
            paint.setStrokeWidth(4.6f);
            c.drawLine(p.x-r*.22f,p.y-r*.46f,p.x-r*.22f,p.y+r*.46f,paint);
            c.drawLine(p.x+r*.22f,p.y-r*.46f,p.x+r*.22f,p.y+r*.46f,paint);
        } else if ("r2".equals(name)) {
            drawFirePistol(c,p.x,p.y,r*.77f);
        } else if ("triangle".equals(name)) {
            drawInteractHand(c,p.x,p.y,r*.76f);
        } else {
            Bitmap detailed=glyphs.get(name);
            if(detailed!=null) {
                float iconRadius=r*.82f;
                c.drawBitmap(detailed,null,
                    new RectF(p.x-iconRadius,p.y-iconRadius,p.x+iconRadius,p.y+iconRadius),
                    glyphPaint);
            } else {
                // Decoding errors are non-fatal and never affect control input.
                drawActionGlyph(c,name,p.x,p.y,r*.67f);
            }
        }
        paint.setStyle(Paint.Style.FILL);
        if(label!=null&&!label.isEmpty()) {
            paint.setColor(ACTION_GLYPH);paint.setTextAlign(Paint.Align.CENTER);
            paint.setTypeface(Typeface.DEFAULT_BOLD);
            paint.setTextSize(18*sizeMultiplier);
            c.drawText(label,p.x,p.y+r+22*sizeMultiplier,paint);
        }
    }
    // Four independently highlighted, bevel-edged arms of a real PS-style
    // D-pad, instead of the old circle with two crossing lines.
    // Make the cross large enough to operate with a thumb. Only the D-pad
    // uses this factor: the analog sticks and action buttons stay unchanged.
    // Arm extent: 52 * 1.5 = 78 (near the 80-unit analog base radius).
    private static final float DPAD_VISUAL_SCALE=1.50f;
    private static final float[][] DPAD_ARMS={
        {-18f,-52f,18f,-7f}, {7f,-18f,52f,18f},
        {-18f,7f,18f,52f}, {-52f,-18f,-7f,18f}
    };
    private static final String[] DPAD_DIRECTIONS={"up","right","down","left"};
    private void actionDpad(Canvas c) {
        final PointF p=actionDpadCenter();
        final float s=sizeMultiplier*DPAD_VISUAL_SCALE;
        c.save();
        c.translate(p.x,p.y);
        for (int i=0;i<4;i++) {
            final float[] arm=DPAD_ARMS[i];
            final boolean selected=pressed.contains(DPAD_DIRECTIONS[i]);
            final float x0=arm[0]*s,y0=arm[1]*s,x1=arm[2]*s,y1=arm[3]*s;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(selected?0xff405868:0xd91a2b38);
            c.drawRoundRect(x0,y0,x1,y1,6f*s,6f*s,paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(2.1f*s);
            paint.setColor(selected?ACTION_EDGE_ACTIVE:ACTION_EDGE);
            c.drawRoundRect(x0,y0,x1,y1,6f*s,6f*s,paint);
            // Engraved directional chevrons: visible on each separate arm.
            paint.setColor(selected?ACTION_GLYPH_ACTIVE:ACTION_GLYPH);
            paint.setStrokeWidth(3.3f*s);
            paint.setStrokeCap(Paint.Cap.ROUND);
            if (i==0) {
                c.drawLine(-7f*s,-29f*s,0,-37f*s,paint);
                c.drawLine(0,-37f*s,7f*s,-29f*s,paint);
            } else if (i==1) {
                c.drawLine(29f*s,-7f*s,37f*s,0,paint);
                c.drawLine(37f*s,0,29f*s,7f*s,paint);
            } else if (i==2) {
                c.drawLine(-7f*s,29f*s,0,37f*s,paint);
                c.drawLine(0,37f*s,7f*s,29f*s,paint);
            } else {
                c.drawLine(-29f*s,-7f*s,-37f*s,0,paint);
                c.drawLine(-37f*s,0,-29f*s,7f*s,paint);
            }
        }
        paint.setStrokeCap(Paint.Cap.BUTT);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(0xff1a2c39);
        c.drawRoundRect(-12f*s,-12f*s,12f*s,12f*s,3f*s,3f*s,paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.4f*s);
        paint.setColor(ACTION_EDGE);
        c.drawRoundRect(-12f*s,-12f*s,12f*s,12f*s,3f*s,3f*s,paint);
        paint.setStyle(Paint.Style.FILL);
        c.restore();
    }
    private void actionStick(Canvas c,String name) {
        PointF p=actionCenter(name);
        float r=80*sizeMultiplier;
        paint.setStyle(Paint.Style.FILL);paint.setColor(actionFill(false));
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setColor(ACTION_EDGE);
        paint.setStrokeWidth(2.5f);c.drawCircle(p.x,p.y,r,paint);
        float kx=p.x,ky=p.y;
        for(int i=0;i<controls.size();i++) {
            if(name.equals(controls.valueAt(i))) {
                PointF q=positions.get(controls.keyAt(i));
                if(q!=null) {
                    float dx=actionDisplayX(q.x)-p.x;
                    kx=p.x+Math.max(-r*.58f,Math.min(r*.58f,dx));
                    ky=p.y+Math.max(-r*.58f,Math.min(r*.58f,q.y-p.y));
                }
                break;
            }
        }
        paint.setStyle(Paint.Style.FILL);paint.setColor(0xa04d6573);
        c.drawCircle(kx,ky,31*sizeMultiplier,paint);
    }
    private void actionClickStick(Canvas c,String name) {
        PointF p=center(name);float r=25*sizeMultiplier;
        paint.setStyle(Paint.Style.FILL);paint.setColor(actionFill(pressed.contains(name)));
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setColor(ACTION_EDGE);
        paint.setStrokeWidth(2);c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.FILL);paint.setColor(ACTION_GLYPH);
        paint.setTypeface(Typeface.DEFAULT_BOLD);paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(16*sizeMultiplier);
        c.drawText(name.toUpperCase(),p.x,p.y-(paint.ascent()+paint.descent())/2f,paint);
    }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c); c.save(); c.translate(offsetX(),offsetY()); c.scale(scale(),scale());
        if (unchartedStyle) {
            actionButton(c,"l1",46,"L1");actionButton(c,"l2",46,"L2");
            actionButton(c,"r1",46,"R1");actionButton(c,"r2",46,"R2");
            actionButton(c,"touchpad",34,"");actionButton(c,"options",34,"");
            actionDpad(c);
            actionButton(c,"triangle",46,"△");actionButton(c,"square",46,"□");
            actionButton(c,"circle",46,"○");actionButton(c,"cross",46,"×");
            actionStick(c,"leftstick");actionStick(c,"rightstick");
            // No independent L3/R3 bubbles. Double-tap the corresponding
            // analog in this style, with a dedicated pulse of its PS button.
        } else {
            rectangle(c,"l2",108,"L2");rectangle(c,"l1",108,"L1");
            rectangle(c,"r1",108,"R1");rectangle(c,"r2",108,"R2");
            rectangle(c,"touchpad",126,"TOUCH");rectangle(c,"options",108,"OPTIONS");
            circle(c,"up",39,"▲",0xffc6ad76);circle(c,"left",39,"◀",0xffc6ad76);
            circle(c,"right",39,"▶",0xffc6ad76);circle(c,"down",39,"▼",0xffc6ad76);
            circle(c,"triangle",39,"△",0xff83cdaa);circle(c,"circle",39,"○",0xffe38288);
            circle(c,"cross",39,"×",0xff88aaff);circle(c,"square",39,"□",0xffd6a0ce);
            drawStick(c,"leftstick");drawStick(c,"rightstick");
            circle(c,"l3",29,"L3",0xffa0aec0);circle(c,"r3",29,"R3",0xffa0aec0);
        }
        if (editing) {
            paint.setStyle(Paint.Style.FILL);paint.setColor(0xdd15171d);
            c.drawRoundRect(325,9,540,62,12,12,paint);
            c.drawRoundRect(745,9,945,62,12,12,paint);
            c.drawRoundRect(951,9,1124,62,12,12,paint);
            paint.setColor(0xfff1e7ca);paint.setTypeface(Typeface.DEFAULT_BOLD);
            paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(18);
            c.drawText("- SIZE",379,43,paint);c.drawText("+ SIZE",486,43,paint);
            c.drawText(unchartedStyle?"UNCHARTED / ACOES":"CLASSICO / PS",845,43,paint);
            c.drawText("RESET",1037,43,paint);
            paint.setStyle(Paint.Style.FILL);paint.setColor(0xcc15171d);
            c.drawRoundRect(548,9,732,62,13,13,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2);
            paint.setColor(ACTION_EDGE);c.drawRoundRect(548,9,732,62,13,13,paint);
            paint.setStyle(Paint.Style.FILL);paint.setTextSize(20);
            paint.setTextAlign(Paint.Align.CENTER);
            c.drawText("DONE",640,36-(paint.ascent()+paint.descent())/2f,paint);
        }
        c.restore();
    }
}
