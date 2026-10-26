#include <cassert>
#include <cstdio>
#include "bbport_touch_layout.h"
int main(){for(float aspect:{1.6f,16.f/9,2.21f,2.4f})for(float s:{.6f,1.f,1.25f,1.6f})for(int corner=0;corner<5;++corner){
 float h=1080,w=aspect*h;BbTouchLayout::Rect hud{};
 if(corner==1)hud={10,10,850,240};if(corner==2)hud={w-850,10,w-10,240};
 if(corner==3)hud={10,h-240,850,h-10};if(corner==4)hud={w-850,h-240,w-10,h-10};
 auto layout=BbTouchLayout::Build(w,h,s,hud);
 if(!BbTouchLayout::Valid(layout,w,h,hud)){std::printf("FAIL %.2f %.2f corner %d effective %.2f\n",aspect,s,corner,layout.scale);return 1;}
 }std::puts("Touch layout: PASS (80 aspect/scale/corner combinations, no overlap)");}
