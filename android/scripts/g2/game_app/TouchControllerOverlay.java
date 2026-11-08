package dev.unchartedg2.game;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
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
        {83,100},{230,100},{615,100},{790,100},{1050,100},{1197,100},
        {160,370},{78,452},{242,452},{160,534},
        {1120,370},{1202,452},{1120,534},{1038,452},
        {318,620},{445,620},{835,620},{964,620}
    };
    private final PadBridge pad;
    private final SharedPreferences preferences;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private float sizeMultiplier;
    private float surfaceOpacity;
    private boolean actionIcons;
    private final Map<String,PointF> centers=new LinkedHashMap<>();
    private final SparseArray<String> controls=new SparseArray<>();
    private final SparseArray<PointF> positions=new SparseArray<>();
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
        sizeMultiplier=Math.max(.8f,Math.min(1.35f,preferences.getFloat("overlay_size",1.20f)));
        surfaceOpacity=Math.max(.20f,Math.min(.80f,preferences.getFloat("overlay_opacity",.40f)));
        actionIcons=preferences.getBoolean("overlay_action_icons",true);
        for (int i=0;i<NAMES.length;i++) {
            String name=NAMES[i];
            centers.put(name,new PointF(preferences.getFloat("overlay_x_"+name,DEFAULT[i][0]),
                                        preferences.getFloat("overlay_y_"+name,DEFAULT[i][1])));
        }
        setContentDescription("On-screen PlayStation controller, GoW III / inFAMOUS layout");
    }
    boolean isEditing() { return editing; }
    float buttonSize() { return sizeMultiplier; }
    boolean usesActionIcons() { return actionIcons; }
    void setSize(float size) {
        sizeMultiplier=Math.max(.8f,Math.min(1.35f,size));
        preferences.edit().putFloat("overlay_size",sizeMultiplier).apply();
        release();invalidate();
    }
    void setActionIcons(boolean on) {
        actionIcons=on;
        preferences.edit().putBoolean("overlay_action_icons",on).apply();
        invalidate();
    }
    // Surface opacity affects only the button backgrounds, never the labels
    // or the hitboxes. Default is the same translucent gamepad as GoW 3.
    private int backgroundColor(boolean down) {
        final float opacity=Math.min(1f,surfaceOpacity+(down?.25f:0f));
        return (Math.round(opacity*255f)<<24)|0x0030394c;
    }
    private PointF center(String name) { return centers.get(name); }
    // The 1280x720 layout drawn at one scale in the centred 16:9 area, as the game (MainActivity):
    // round on any screen shape. Controls may be dragged into the bars around it.
    private float scale() { return Math.max(1e-3f,Math.min(getWidth()/1280f,getHeight()/720f)); }
    private float offsetX() { return (getWidth()-1280f*scale())/2f; }
    private float offsetY() { return (getHeight()-720f*scale())/2f; }
    private float layoutX(MotionEvent e,int index) { return (e.getX(index)-offsetX())/scale(); }
    private float layoutY(MotionEvent e,int index) { return (e.getY(index)-offsetY())/scale(); }
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
    private String hit(float x,float y) {
        PointF dpad=dpadCenter();
        if (!editing && distance(x,y,dpad)<112*sizeMultiplier && distance(center("up").x,center("up").y,dpad)<115*sizeMultiplier &&
            distance(center("left").x,center("left").y,dpad)<115*sizeMultiplier &&
            distance(center("right").x,center("right").y,dpad)<115*sizeMultiplier &&
            distance(center("down").x,center("down").y,dpad)<115*sizeMultiplier) return "dpad";
        String winner=null; float best=Float.MAX_VALUE;
        for (String name:NAMES) {
            PointF p=center(name);
            float dx=Math.abs(x-p.x),dy=Math.abs(y-p.y);
            boolean inside=rectangle(name) ? dx<(name.equals("touchpad") ? 72 : 62)*sizeMultiplier && dy<38*sizeMultiplier
                    : distance(x,y,p)<(stick(name) ? 70 : name.equals("l3") || name.equals("r3") ? 39 : 48)*sizeMultiplier;
            if (inside && distance(x,y,p)<best) { best=distance(x,y,p); winner=name; }
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
                if (p.x<c.x-22) pressed.add("left");
                if (p.x>c.x+22) pressed.add("right");
                if (p.y<c.y-22) pressed.add("up");
                if (p.y>c.y+22) pressed.add("down");
            } else if (control.equals("leftstick")) {
                left=true; PointF c=center(control);
                lx=axis(Math.max(-1f,Math.min(1f,(p.x-c.x)/(56f*sizeMultiplier))));
                ly=axis(Math.max(-1f,Math.min(1f,(p.y-c.y)/(56f*sizeMultiplier))));
            } else if (control.equals("rightstick")) {
                right=true; PointF c=center(control);
                rx=axis(Math.max(-1f,Math.min(1f,(p.x-c.x)/(56f*sizeMultiplier))));
                ry=axis(Math.max(-1f,Math.min(1f,(p.y-c.y)/(56f*sizeMultiplier))));
            } else if (control.equals("l2")) { l2=255; pressed.add(control); }
            else if (control.equals("r2")) { r2=255; pressed.add(control); }
            else if (control.equals("touchpad")) pressed.add("touchpad_right"); /* 右侧点击:打开游戏暂停菜单,进而可进存档列表 */
            else pressed.add(control);
        }
        pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2);
        invalidate();
    }
    void release() { menuSwipePointer=-1; controls.clear(); positions.clear(); publish(); }
    void edit(Runnable done) { release(); editing=true; onEditDone=done; invalidate(); }
    private void finishEdit() {
        editPointer=-1; dragged=null; editing=false; invalidate();
        if (onEditDone!=null) onEditDone.run();
        onEditDone=null;
    }
    void resetLayout() {
        SharedPreferences.Editor editor=preferences.edit();
        for (int i=0;i<NAMES.length;i++) {
            String name=NAMES[i]; centers.get(name).set(DEFAULT[i][0],DEFAULT[i][1]);
            editor.remove("overlay_x_"+name).remove("overlay_y_"+name);
        }
        editor.apply(); release(); invalidate();
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int action=e.getActionMasked(),at=e.getActionIndex(),id=e.getPointerId(at);
        if (editing) {
            if (action==MotionEvent.ACTION_CANCEL) { editPointer=-1; dragged=null; return true; }
            if (action==MotionEvent.ACTION_DOWN) {
                float x=layoutX(e,at),y=layoutY(e,at);
                if (y<75f) {
                    if (x>548 && x<732) { finishEdit(); return true; }
                    if (x>325 && x<430) { setSize(sizeMultiplier-.10f); return true; }
                    if (x>432 && x<540) { setSize(sizeMultiplier+.10f); return true; }
                    if (x>745 && x<945) { setActionIcons(!actionIcons); return true; }
                    if (x>951 && x<1124) { resetLayout(); return true; }
                }
                dragged=hit(x,y); editPointer=dragged==null ? -1 : id;
            } else if (action==MotionEvent.ACTION_MOVE && editPointer>=0 && dragged!=null) {
                int index=e.findPointerIndex(editPointer);
                float barX=offsetX()/scale(),barY=offsetY()/scale();
                if (index>=0) center(dragged).set(
                    Math.max(36-barX,Math.min(1244+barX,layoutX(e,index))),
                    Math.max(36-barY,Math.min(684+barY,layoutY(e,index))));
                invalidate();
            } else if ((action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP)
                       && id==editPointer && dragged!=null) {
                PointF p=center(dragged);
                preferences.edit().putFloat("overlay_x_"+dragged,p.x)
                    .putFloat("overlay_y_"+dragged,p.y).apply();
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
            if (menuSwipePointer<0 && x<=40f && hit(x,y)==null) {
                menuSwipePointer=id;
                menuSwipeStartX=x;
                menuSwipeStartY=y;
                return true;
            }
            String name=hit(x,y);
            if (name==null) return true;
            controls.put(id,name);
            positions.put(id,new PointF(x,y));
        } else if (action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
            if (id==menuSwipePointer) menuSwipePointer=-1;
            controls.remove(id); positions.remove(id);
        }
        if (action==MotionEvent.ACTION_MOVE) {
            if (menuSwipePointer>=0) {
                int pointer=e.findPointerIndex(menuSwipePointer);
                if (pointer>=0) {
                    float x=layoutX(e,pointer),y=layoutY(e,pointer);
                    if (Math.abs(y-menuSwipeStartY)>65f && x-menuSwipeStartX<140f)
                        menuSwipePointer=-1;
                    else if (x-menuSwipeStartX>=140f &&
                             Math.abs(y-menuSwipeStartY)<=65f) {
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
    // Used only to decide whether to draw a small auxiliary badge.
    // It must NEVER replace the main PlayStation symbols like X, O, square, triangle.
    private static String actionLabel(String name) {
        switch (name) {
            case "triangle": return "interact";
            case "square": return "melee";
            case "cross": return "jump";
            case "circle": return "cover";
            case "l2": return "aim";
            case "r2": return "fire";
            case "l1": return "grenade";
            case "r1": return "reload";
            default: return null;
        }
    }
    private void drawActionGlyph(Canvas c,String name,float x,float y,float size) {
        c.save(); c.translate(x,y); c.scale(size,size);
        paint.setColor(0xfff2ead9);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(.12f);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        android.graphics.Path glyph=new android.graphics.Path();
        switch (name) {
            case "cross": // climb/jump, not a replacement for the X button
                c.drawLine(0,.61f,0,-.65f,paint);
                c.drawLine(-.48f,-.20f,0,-.68f,paint);
                c.drawLine(.48f,-.20f,0,-.68f,paint);
                break;
            case "square": // melee fist
                c.drawRoundRect(-.58f,-.25f,.58f,.45f,.16f,.16f,paint);
                for(int i=-1;i<=1;i++)c.drawLine(i*.28f,-.25f,i*.28f,.07f,paint);
                break;
            case "triangle": // interact hand
                c.drawRoundRect(-.39f,-.12f,.39f,.56f,.16f,.16f,paint);
                c.drawLine(-.24f,-.10f,-.24f,-.56f,paint);
                c.drawLine(0,-.10f,0,-.68f,paint);
                c.drawLine(.24f,-.10f,.24f,-.54f,paint);
                break;
            case "circle": // take cover shield
                glyph.moveTo(0,-.73f);
                glyph.lineTo(.59f,-.38f);
                glyph.lineTo(.45f,.35f);
                glyph.lineTo(0,.73f);
                glyph.lineTo(-.45f,.35f);
                glyph.lineTo(-.59f,-.38f);
                glyph.close();
                c.drawPath(glyph,paint);
                break;
            case "l2": // sight / aim reticle
                c.drawCircle(0,0,.40f,paint);
                c.drawLine(-.76f,0,-.46f,0,paint);
                c.drawLine(.46f,0,.76f,0,paint);
                c.drawLine(0,-.76f,0,-.46f,paint);
                c.drawLine(0,.46f,0,.76f,paint);
                break;
            case "r2": // fire muzzle
                c.drawCircle(0,0,.20f,paint);
                for(int i=0;i<8;i++){
                    double a=i*Math.PI/4;
                    c.drawLine((float)(.36f*Math.cos(a)),(float)(.36f*Math.sin(a)),
                               (float)(.77f*Math.cos(a)),(float)(.77f*Math.sin(a)),paint);
                }
                break;
            case "l1": // grenade + fuse pin
                c.drawOval(-.45f,-.22f,.45f,.65f,paint);
                c.drawLine(0,-.22f,.20f,-.63f,paint);
                c.drawCircle(.33f,-.65f,.15f,paint);
                break;
            case "r1": // reload arrow
                c.drawArc(-.60f,-.60f,.60f,.60f,35f,278f,false,paint);
                glyph.moveTo(.56f,-.05f);
                glyph.lineTo(.65f,.42f);
                glyph.lineTo(.19f,.25f);
                c.drawPath(glyph,paint);
                break;
            default: break;
        }
        c.restore();
        paint.setStyle(Paint.Style.FILL);
        paint.setStrokeCap(Paint.Cap.BUTT);
        paint.setStrokeJoin(Paint.Join.MITER);
    }

    private void circle(Canvas c,String name,float r,String label,int accent) {
        PointF p=center(name);
        r*=sizeMultiplier;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(name.equals(dragged) ? 0xffe8dfc9 : backgroundColor(pressed.contains(name)));
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(3); paint.setColor(accent);
        c.drawCircle(p.x,p.y,r,paint);
        paint.setStyle(Paint.Style.FILL); paint.setTypeface(Typeface.DEFAULT_BOLD);
        paint.setTextAlign(Paint.Align.CENTER);
        // Exact GoW3/ISS pad layout: large PlayStation legend is always central.
        paint.setColor(accent);
        paint.setTextSize((label.length()>2 ? 19 : 33)*sizeMultiplier);
        c.drawText(label,p.x,p.y-(paint.ascent()+paint.descent())/2,paint);
        if (actionIcons && actionLabel(name)!=null) {
            // Small, backgrounded secondary pictogram; never use text/emoji
            // inside the already crowded face-button cluster.
            final float bx=p.x+r*.76f, by=p.y+r*.70f;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0xdd14202b);
            c.drawCircle(bx,by,12*sizeMultiplier,paint);
            drawActionGlyph(c,name,bx,by,9*sizeMultiplier);
        }
    }
    private void rectangle(Canvas c,String name,float width,String label) {
        PointF p=center(name);
        width*=sizeMultiplier;
        // The touchpad button presses "touchpad_right" (right-side click, opens the pause menu),
        // not "touchpad", so highlight it from that signal too.
        boolean lit=pressed.contains(name) ||
            (name.equals("touchpad") && pressed.contains("touchpad_right"));
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(name.equals(dragged) ? 0xffe8dfc9 : backgroundColor(lit));
        c.drawRoundRect(p.x-width/2,p.y-25*sizeMultiplier,p.x+width/2,p.y+25*sizeMultiplier,14,14,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setColor(0xbbc6ad76); paint.setStrokeWidth(2);
        c.drawRoundRect(p.x-width/2,p.y-25*sizeMultiplier,p.x+width/2,p.y+25*sizeMultiplier,14,14,paint);
        paint.setStyle(Paint.Style.FILL); paint.setTypeface(Typeface.DEFAULT_BOLD);
        paint.setTextSize(19*sizeMultiplier); paint.setTextAlign(Paint.Align.CENTER);
        // Both shoulder legends and TOUCH/OPTIONS remain clean and readable.
        final boolean badge=actionIcons && actionLabel(name)!=null;
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setColor(0xfff2ead9);
        c.drawText(label,p.x-(badge?width*.18f:0f),
                   p.y-(paint.ascent()+paint.descent())/2f,paint);
        if (badge) {
            final float bx=p.x+width*.29f, by=p.y;
            paint.setColor(0xdd14202b);
            c.drawCircle(bx,by,12*sizeMultiplier,paint);
            drawActionGlyph(c,name,bx,by,9*sizeMultiplier);
        }
    }
    private void drawStick(Canvas c,String name) {
        PointF center=center(name);
        paint.setStyle(Paint.Style.FILL); paint.setColor(name.equals(dragged) ? 0xffe8dfc9 : backgroundColor(false));
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
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c); c.save(); c.translate(offsetX(),offsetY()); c.scale(scale(),scale());
        rectangle(c,"l2",108,"L2"); rectangle(c,"l1",108,"L1");
        rectangle(c,"r1",108,"R1"); rectangle(c,"r2",108,"R2");
        rectangle(c,"touchpad",126,"TOUCH"); rectangle(c,"options",108,"OPTIONS");
        circle(c,"up",39,"▲",0xffc6ad76); circle(c,"left",39,"◀",0xffc6ad76);
        circle(c,"right",39,"▶",0xffc6ad76); circle(c,"down",39,"▼",0xffc6ad76);
        circle(c,"triangle",39,"△",0xff83cdaa); circle(c,"circle",39,"○",0xffe38288);
        circle(c,"cross",39,"×",0xff88aaff); circle(c,"square",39,"□",0xffd6a0ce);
        drawStick(c,"leftstick"); drawStick(c,"rightstick");
        circle(c,"l3",29,"L3",0xffa0aec0); circle(c,"r3",29,"R3",0xffa0aec0);
        if (editing) {
            paint.setStyle(Paint.Style.FILL); paint.setColor(0xdd15171d);
            c.drawRoundRect(325,9,540,62,12,12,paint);
            c.drawRoundRect(745,9,945,62,12,12,paint);
            c.drawRoundRect(951,9,1124,62,12,12,paint);
            paint.setColor(0xfff1e7ca); paint.setTypeface(Typeface.DEFAULT_BOLD);
            paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(18);
            c.drawText("- SIZE",379,43,paint);c.drawText("+ SIZE",486,43,paint);
            c.drawText(actionIcons?"SIMBOLOS + ACOES":"PLAYSTATION",845,43,paint);
            c.drawText("RESET",1037,43,paint);
            paint.setStyle(Paint.Style.FILL); paint.setColor(0xcc15171d);
            c.drawRoundRect(548,9,732,62,13,13,paint);
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(2); paint.setColor(0xffc6ad76);
            c.drawRoundRect(548,9,732,62,13,13,paint);
            paint.setStyle(Paint.Style.FILL); paint.setTextSize(20);
            paint.setTextAlign(Paint.Align.CENTER);
            c.drawText("DONE",640,36-(paint.ascent()+paint.descent())/2,paint);
        }
        c.restore();
    }
}
