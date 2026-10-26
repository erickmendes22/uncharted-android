// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace BbTouchLayout {
struct Rect {float left=0,top=0,right=0,bottom=0;};
struct Slot {
    float x=0,y=0,w=0,h=0;bool circle=false;
    Rect Bounds() const{return {x-w*.5f,y-h*.5f,x+w*.5f,y+h*.5f};}
};
inline bool Intersects(Rect a,Rect b){return a.right>b.left && b.right>a.left && a.bottom>b.top && b.bottom>a.top;}
inline bool Overlaps(const Slot& a,const Slot& b) {
    if(!Intersects(a.Bounds(),b.Bounds()))return false;
    if(a.circle && b.circle){float dx=a.x-b.x,dy=a.y-b.y,r=(a.w+b.w)*.5f;return dx*dx+dy*dy<r*r;}
    if(a.circle || b.circle){const auto& c=a.circle?a:b;const auto r=(a.circle?b:a).Bounds();
        float dx=c.x-std::clamp(c.x,r.left,r.right),dy=c.y-std::clamp(c.y,r.top,r.bottom);return dx*dx+dy*dy<c.w*c.w*.25f;}
    return true;
}
// Indices 0..11 match PS controls; 12..15 D-pad up/down/left/right;
// 16/17 floating analog hints; 18 is the dedicated app-menu button.
struct Layout {std::array<Slot,19> slots{};float scale=1;};
inline bool Valid(const Layout& l,float w,float h,Rect hud={}) {
    const bool reserved=hud.right>hud.left && hud.bottom>hud.top;
    for(size_t i=0;i<l.slots.size();++i){const auto b=l.slots[i].Bounds();
        if(b.left<0 || b.top<0 || b.right>w || b.bottom>h)return false;
        if(reserved && Overlaps(l.slots[i],{(hud.left+hud.right)*.5f,(hud.top+hud.bottom)*.5f,hud.right-hud.left,hud.bottom-hud.top,false}))return false;
        for(size_t j=0;j<i;++j)if(Overlaps(l.slots[i],l.slots[j]))return false;
    }
    return true;
}
inline Layout Build(float width,float height,float requested,Rect hud={}) {
    const float aspect=width/height;
    Rect r{hud.left/height,hud.top/height,hud.right/height,hud.bottom/height};
    Layout result;
    for(float s=std::clamp(requested,.6f,1.6f);s>=.39f;s-=.025f){
        Layout l;l.scale=s;
        auto& a=l.slots;
        const float stick=.64f+.10f*std::max(s-1.f,0.f),y=.635f,arm=.105f*s;
        auto circle=[&](int i,float x,float yy,float diameter){a[i]={x,yy,diameter*s,diameter*s,true};};
        circle(0,aspect-.42f,y-arm,.108f);circle(1,aspect-.42f+arm,y,.108f);
        circle(2,aspect-.42f,y+arm,.108f);circle(3,aspect-.42f-arm,y,.108f);
        const float shoulder=.15f+.035f*std::max(s-1.f,0.f);
        a[4]={.325f,shoulder,.15f*s,.066f*s,false};a[5]={.325f+.17f*s,shoulder,.15f*s,.066f*s,false};
        a[6]={aspect-.325f,shoulder,.15f*s,.066f*s,false};a[7]={aspect-.325f-.17f*s,shoulder,.15f*s,.066f*s,false};
        a[8]={aspect*.5f-.20f*s,.055f,.17f*s,.066f*s,false};
        a[9]={aspect*.5f,.055f,.085f*s,.066f*s,false};
        a[18]={aspect*.5f+.16f*s,.055f,.11f*s,.06f*s,false};
        circle(10,stick+.1195f*s+.022f,.84f,.075f);circle(11,aspect-stick-.1195f*s-.022f,.84f,.075f);
        circle(12,.42f,y-arm,.104f);circle(13,.42f,y+arm,.104f);
        circle(14,.42f-arm,y,.104f);circle(15,.42f+arm,y,.104f);
        circle(16,stick,.84f,.164f);circle(17,aspect-stick,.84f,.164f);
        if(r.right>r.left && r.bottom>r.top) {
            Slot obstacle{(r.left+r.right)*.5f,(r.top+r.bottom)*.5f,r.right-r.left,r.bottom-r.top,false};
            if(r.top<.25f) {
                // Keep the central actions in their own row, moving the whole row
                // horizontally when a large corner HUD reaches it.
                bool central=false;for(int i:{8,9,18})central|=Overlaps(a[i],obstacle);
                if(central){const float dx=(r.left+r.right)<aspect ? r.right+.018f-a[8].Bounds().left : r.left-.018f-a[18].Bounds().right;
                    for(int i:{8,9,18})a[i].x+=dx;}
                for(int i:{4,5,6,7})if(Overlaps(a[i],obstacle))a[i].y=r.bottom+.018f+a[i].h*.5f;
            } else if(r.bottom>.75f) {
                for(int side=0;side<2;++side){
                    const int left[]={10,12,13,14,15,16},right[]={0,1,2,3,11,17};const int* group=side?right:left;
                    bool hit=false;float bottom=0;for(int j=0;j<6;++j){hit|=Overlaps(a[group[j]],obstacle);bottom=std::max(bottom,a[group[j]].Bounds().bottom);}
                    if(hit)for(int j=0;j<6;++j)a[group[j]].y-=bottom-r.top+.018f;
                }
            }
        }
        for(auto& slot:a){slot.x*=height;slot.y*=height;slot.w*=height;slot.h*=height;}
        result=l;
        if(Valid(l,width,height,hud))return l;
    }
    return result;
}
} // namespace BbTouchLayout
