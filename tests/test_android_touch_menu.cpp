// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise actual touch routing and held-stick behavior without an Android device/GPU.
#include <cassert>
#include <cstdio>
#include "imgui.h"
#include "bbport_settings.h"
#include "bbport_touchpad.h"
#include "bbport_overlay_android.h"

static int last_sticks[4];
static uint32_t last_buttons;
extern "C" void runtime_touch_pad(uint32_t buttons,const int sticks[4],uint8_t,uint8_t) {
    last_buttons=buttons;
    for(int i=0;i<4;++i)last_sticks[i]=sticks[i];
}
extern "C" int runtime_gamepad_connected(){return 0;}
extern "C" void runtime_restart(){}
extern "C" void runtime_mobile_action(int){}
unsigned BbDisplayRefreshHz(){return 120;}

static void frame(float w=1920,float h=1080){ImGui::GetIO().DisplaySize=ImVec2(w,h);ImGui::NewFrame();BbOverlay::TouchPadFrame(w,h);BbOverlay::AndroidMenuButton(1);ImGui::Render();}
static SDL_Event finger(Uint32 type,float x,float y){
    SDL_Event e{};e.type=type;e.tfinger.fingerID=42;e.tfinger.x=x;e.tfinger.y=y;return e;
}
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1920,1080);io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto& settings=BbSettings::Get();settings.touch_pad=0;frame();
    // The dedicated menu must remain usable when gamepad controls are hidden.
    auto e=finger(SDL_EVENT_FINGER_DOWN,(BbOverlay::TouchPadMenuRect(1920,1080).left+BbOverlay::TouchPadMenuRect(1920,1080).right)*.5f/1920,.055f);
    assert(BbOverlay::AndroidTouch(e,false)==BbOverlay::TouchAction::Open);
    settings.touch_pad=1;settings.touch_mobile=false;settings.touch_pad_look=1;frame();
    // Unlabelled right-side space is an analog: holding still must keep turning.
    e=finger(SDL_EVENT_FINGER_DOWN,.64f,.65f);assert(BbOverlay::TouchPadEvent(e));
    e=finger(SDL_EVENT_FINGER_MOTION,.68f,.65f);assert(BbOverlay::TouchPadEvent(e));frame();
    assert(last_sticks[2]>128 && last_sticks[3]==128);const int held=last_sticks[2];
    frame();assert(last_sticks[2]==held);
    // Opening the menu releases the virtual pad (same reset as SetOpen).
    BbOverlay::TouchPadReset();assert(last_sticks[2]==-1);
    // Left and right analogs must stay independent.
    e=finger(SDL_EVENT_FINGER_DOWN,.35f,.72f);BbOverlay::TouchPadEvent(e);
    e=finger(SDL_EVENT_FINGER_MOTION,.38f,.72f);BbOverlay::TouchPadEvent(e);frame();
    assert(last_sticks[0]>128 && last_sticks[2]==-1);
    BbOverlay::TouchPadReset();
    // Re-layout at phone/tablet landscape sizes: hit targets follow what is drawn.
    settings.touch_pad_scale=1;
    for(const auto size:{ImVec2(1920,1080),ImVec2(2400,1080),ImVec2(1920,1200)}) {
        frame(size.x,size.y);
        const auto layout=BbTouchLayout::Build(size.x,size.y,settings.touch_pad_scale.load());
        e=finger(SDL_EVENT_FINGER_DOWN,layout.slots[7].x/size.x,layout.slots[7].y/size.y);
        BbOverlay::TouchPadEvent(e);frame(size.x,size.y);assert(last_buttons&0x800); // R1
        BbOverlay::TouchPadReset();
        e=finger(SDL_EVENT_FINGER_DOWN,layout.slots[12].x/size.x,layout.slots[12].y/size.y);
        BbOverlay::TouchPadEvent(e);frame(size.x,size.y);assert(last_buttons&0x10); // D-pad up
        BbOverlay::TouchPadReset();
        frame(size.x,size.y);
        bool green=false,blue=false,red=false,purple=false;
        for(const auto* list:ImGui::GetDrawData()->CmdLists)for(const auto& v:list->VtxBuffer) {
            const auto rgb=v.col&0xFFFFFF;
            green|=rgb==(IM_COL32(133,211,181,255)&0xFFFFFF);
            blue|=rgb==(IM_COL32(142,168,255,255)&0xFFFFFF);
            red|=rgb==(IM_COL32(235,128,145,255)&0xFFFFFF);
            purple|=rgb==(IM_COL32(210,153,214,255)&0xFFFFFF);
        }
        assert(green && blue && red && purple);
    }
    frame();
    // Draw the actual Android panel with each HUD mode, including its child/footer.
    for(int mode=0;mode<3;++mode){settings.hud_mode=mode;bool open=true;ImGui::NewFrame();BbOverlay::AndroidMenu(1,16.6f,open);ImGui::Render();}
    ImGui::DestroyContext();std::puts("Android touch/menu: PASS (menu hidden-pad access, held analog, independent sticks, panel layout)");
}
