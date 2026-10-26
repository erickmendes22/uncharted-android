#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "bbport_settings.h"
unsigned BbDisplayRefreshHz(){return 120;}
int main(){
 const char* path=std::getenv("BB_CONFIG");assert(path);
 {auto* f=std::fopen(path,"w");assert(f);std::fputs("hud_scale=3.0\nframe_gen=fsr3\n",f);std::fclose(f);}
 BbSettings::Load();auto& s=BbSettings::Get();assert(s.hud_scale==3.0f);assert(s.frame_gen==BbSettings::FrameGenOff);
 s.frame_gen=BbSettings::FrameGenLsfg;BbSettings::Save();s.hud_scale=1;BbSettings::Load();
 assert(s.hud_scale==3.0f);assert(s.frame_gen==BbSettings::FrameGenLsfg);
 std::remove(path);std::puts("Android UI settings: PASS (300% HUD persists, FSR frame generation disabled, LSFG retained)");
}
