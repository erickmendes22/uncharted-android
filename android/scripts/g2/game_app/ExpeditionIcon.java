package dev.unchartedg2.game;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;

/** Small scalable outline icons. Drawn locally, independent of emoji/font support. */
final class ExpeditionIcon extends Drawable {
    private final int kind;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    ExpeditionIcon(int kind,int color){this.kind=kind;paint.setColor(color);paint.setStrokeWidth(1.8f);paint.setStyle(Paint.Style.STROKE);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);}
    @Override public void draw(Canvas c){
        c.save();c.translate(getBounds().left,getBounds().top);c.scale(getBounds().width()/24f,getBounds().height()/24f);
        Path p=new Path();
        switch(kind){
        case 0: // compass
            c.drawCircle(12,12,9,paint);p.moveTo(16,8);p.lineTo(14,14);p.lineTo(8,16);p.lineTo(10,10);p.close();c.drawPath(p,paint);break;
        case 1: // image / video
            c.drawRoundRect(3,4,21,20,2,2,paint);p.moveTo(4,17);p.lineTo(9,12);p.lineTo(13,15);p.lineTo(17,10);p.lineTo(21,15);c.drawPath(p,paint);break;
        case 2: // settings
            for(int i=0;i<3;i++){float y=6+i*6;c.drawLine(3,y,21,y,paint);c.drawCircle(i%2==0?9:16,y,2,paint);}break;
        case 3: // info
            c.drawCircle(12,12,9,paint);c.drawLine(12,11,12,17,paint);c.drawPoint(12,7,paint);break;
        case 4: // folder
            p.moveTo(3,7);p.lineTo(3,20);p.lineTo(21,20);p.lineTo(21,7);p.lineTo(12,7);p.lineTo(10,4);p.lineTo(3,4);p.close();c.drawPath(p,paint);break;
        case 5: // gamepad
            c.drawRoundRect(2,6,22,19,4,4,paint);c.drawLine(6,10,6,15,paint);c.drawLine(3.5f,12.5f,8.5f,12.5f,paint);c.drawCircle(16,11,1,paint);c.drawCircle(19,14,1,paint);break;
        case 6: // play
            p.moveTo(8,4);p.lineTo(20,12);p.lineTo(8,20);p.close();c.drawPath(p,paint);break;
        default: // layers
            for(int i=0;i<3;i++)c.drawRoundRect(4+i*2,4+i*3,18+i*2,10+i*3,1,1,paint);
        }
        c.restore();
    }
    @Override public void setAlpha(int a){paint.setAlpha(a);invalidateSelf();}
    @Override public void setColorFilter(ColorFilter f){paint.setColorFilter(f);invalidateSelf();}
    @Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
}
