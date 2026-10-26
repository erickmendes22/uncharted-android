// SPDX-License-Identifier: GPL-2.0-or-later
// g2-arm64: the in-game menu for touch screens. The PC menu (bbport_overlay.cpp) is a floating
// window of combo boxes sized for a mouse; here the same BbSettings values are edited in a panel
// on the right edge of the screen with rows a finger can hit, choices as rows of buttons, and
// only what applies on Android (no DLSS, FSR 4 or XeSS). Opened with a swipe in from the right
// edge, L3+R3 or Insert; closed with a tap outside it, the B button or its Close button.

#include "bbport_locale.h"
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <type_traits>
#include <cmath>
#include <cstdio>

#include "imgui.h"

#include "bbport_overlay_android.h"
#include "bbport_settings.h"
#include "bbport_touchpad.h"
#include "bbport_touch_layout.h"
#include "core/emulator_settings.h"

extern "C" void runtime_restart(void);
extern "C" void runtime_mobile_action(int);

namespace BbOverlay {

namespace {

constexpr float TouchScale = 1.35f;   // on top of the display scale: 32 px text at 1080p
constexpr float PanelShare = 0.48f;  // of the screen width

std::atomic<float> menu_x0{.52f}, menu_x1{.58f}, menu_y0{.02f}, menu_y1{.09f};
std::atomic<float> panel_left_norm{1.0f}; // the panel's left edge, 0..1 of the screen width
std::atomic<float> pending_scroll{0.0f};  // a finger's travel inside the panel, 0..1 of the height

// A change is written to bbport.ini a second after the last one, or when the panel closes
// (BbSettings::Save on every tap would rewrite the file while a slider is dragged).
bool dirty = false;
double last_change = 0.0;

void Changed() {
    dirty = true;
    last_change = ImGui::GetTime();
}

template <typename T>
void Store(std::atomic<T>& target, std::type_identity_t<T> value) {
    if (target.load() != value) {
        target.store(value);
        Changed();
    }
}

/// A row of equal buttons, the chosen one lit. Returns true when the choice changed.
bool Segmented(const char* id, int& value, const char* const* labels, int count,
               const bool* enabled = nullptr) {
    bool changed = false;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width =
        (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * float(count - 1)) / float(count);
    ImGui::PushID(id);
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
    for (int i = 0; i < count; ++i) {
        if (i) {
            ImGui::SameLine();
        }
        const bool selected = value == i;
        const bool usable = !enabled || enabled[i];
        ImGui::BeginDisabled(!usable);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.77f,.64f,.39f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.88f,.75f,.47f,1));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.08f,.11f,.08f,1));
        }
        if (ImGui::Button(BbLocale::Text(labels[i]), ImVec2(width, 0.0f)) && !selected) {
            value = i;
            changed = true;
        }
        if (selected) {
            ImGui::PopStyleColor(3);
        }
        ImGui::EndDisabled();
    }
    ImGui::PopFont();
    ImGui::PopID();
    return changed;
}

void Section(const char* title) {
    ImGui::Spacing();
    ImGui::SeparatorText(BbLocale::Text(title));
}

void Toggle(const char* label, std::atomic<bool>& value) {
    ImGui::PushID(label);
    const float font=ImGui::GetFontSize(),padding=font*.55f;
    const char* state=BbLocale::Text(value?"On":"Off");
    const float pill=std::max(font*2.4f,ImGui::CalcTextSize(state).x+padding*2);
    const float width=ImGui::GetContentRegionAvail().x;
    const float wrap=std::max(font*3,width-pill-padding*3);
    const auto text_size=ImGui::CalcTextSize(BbLocale::Text(label),nullptr,false,wrap);
    const float height=std::max(font*2.0f,text_size.y+padding*2);
    const auto start=ImGui::GetCursorScreenPos();
    if(ImGui::Button("##toggle",ImVec2(width,height))){value=!value;Changed();}
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddText(nullptr,font,ImVec2(start.x+padding,start.y+(height-text_size.y)*.5f),IM_COL32(240,235,221,255),BbLocale::Text(label),nullptr,wrap);
    const ImVec2 a(start.x+width-pill-padding,start.y+(height-font*1.4f)*.5f),b(a.x+pill,a.y+font*1.4f);
    dl->AddRectFilled(a,b,value?IM_COL32(199,164,99,255):IM_COL32(48,57,49,255),font*.5f);
    const auto ts=ImGui::CalcTextSize(state);
    dl->AddText(ImVec2(a.x+(pill-ts.x)*.5f,a.y+(b.y-a.y-ts.y)*.5f),value?IM_COL32(24,30,24,255):IM_COL32(210,209,195,255),state);
    ImGui::PopID();
}

void Slider(const char* label, std::atomic<float>& value, float lo, float hi,bool percent=false) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(BbLocale::Text(label));
    float v=value.load()*(percent?100.f:1.f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::SliderFloat("##value",&v,lo*(percent?100.f:1.f),hi*(percent?100.f:1.f),percent?"%.0f%%":"%.2f")) {
        value.store(v/(percent?100.f:1.f));Changed();
    }
    ImGui::PopID();
}

void Note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", BbLocale::Text(text));
    ImGui::PopStyleColor();
}

} // namespace

void AndroidMenuButton(float base_scale) {
    const auto size=ImGui::GetIO().DisplaySize;
    const float h=size.y;
    const auto box=TouchPadMenuRect(size.x,size.y);
    const ImVec2 a(box.left,box.top),b(box.right,box.bottom);
    const float x=(a.x+b.x)*.5f,y=(a.y+b.y)*.5f;
    menu_x0=a.x/size.x;menu_x1=b.x/size.x;menu_y0=a.y/h;menu_y1=b.y/h;
    auto* dl=ImGui::GetForegroundDrawList();
    dl->AddRectFilled(a,b,IM_COL32(12,20,18,190),h*.012f);
    dl->AddRect(a,b,IM_COL32(206,176,110,180),h*.012f,0,base_scale*1.4f);
    for(int i=-1;i<=1;++i)dl->AddLine(ImVec2(x-h*.018f,y+i*h*.01f),ImVec2(x+h*.018f,y+i*h*.01f),IM_COL32(245,235,210,240),base_scale*2);
}

void AndroidMenu(float base_scale, float frame_ms, bool& keep_open) {
    auto& s=BbSettings::Get();
    const auto* viewport=ImGui::GetMainViewport();
    const float scale=base_scale*TouchScale;
    const float width=std::floor(viewport->WorkSize.x*PanelShare);
    panel_left_norm.store((viewport->WorkSize.x-width)/viewport->WorkSize.x);
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x-width,viewport->WorkPos.y});
    ImGui::SetNextWindowSize({width,viewport->WorkSize.y});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(18*scale,14*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10*scale,9*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(8*scale,10*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,9*scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,18*scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,12*scale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.055f,.08f,.06f,.98f));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.14f,.19f,.145f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.25f,.30f,.20f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.42f,.37f,.23f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.11f,.16f,.12f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(.78f,.66f,.42f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(.95f,.81f,.53f,1));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.94f,.92f,.86f,1));
    ImGui::PushFont(nullptr,18*scale);
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings;
    ImGui::GetIO().MouseDrawCursor=false;
    if(ImGui::Begin("##android_menu",nullptr,flags)) {
        const float resume_width=std::max(ImGui::CalcTextSize(BbLocale::Text("Resume")).x+24*scale,ImGui::GetContentRegionAvail().x*.34f);
        ImGui::TextColored(ImVec4(.85f,.73f,.48f,1),"UNCHARTED DAEDALUS");
        ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-resume_width);
        if(ImGui::Button(BbLocale::Text("Resume"),{resume_width,0}))keep_open=false;

        ImGui::Separator();
        static int tab=0;
        const char* tabs[]={"Video","HUD","Controls"};
        const bool switched=Segmented("tabs",tab,tabs,3);
        const float footer=ImGui::GetFrameHeightWithSpacing()+12*scale;
        ImGui::BeginChild("settings-scroll",{0,-footer},false);
        if(switched)ImGui::SetScrollY(0);
        if(const float travel=pending_scroll.exchange(0.f);travel!=0.f)ImGui::SetScrollY(ImGui::GetScrollY()-travel*viewport->WorkSize.y);
        if(tab==0) {
            Section("Display");Toggle("Fullscreen",s.fullscreen);
            Note(s.fullscreen?"Fill the entire screen":"Keep the original aspect ratio");
            Section("Upscaling");
            const char* upscalers[]={"Off","FSR 3.1","TAA"};const int providers[]={BbSettings::UpscalerOff,BbSettings::UpscalerFsr3,BbSettings::UpscalerTaa};
            int up=0;for(int i=0;i<3;++i)if(providers[i]==s.upscaler)up=i;
            if(Segmented("upscaler",up,upscalers,3))Store(s.upscaler,providers[up]);
            Note("FSR mode and resolution apply on the next game start.");
            if(s.upscaler!=BbSettings::UpscalerOff)Note(s.temporal_running?"Temporal reconstruction: running":"Temporal reconstruction: waiting for a compatible scene");
            Slider("Sharpening",s.sharpness,0.f,2.f);Toggle("Sharpen (RCAS)",s.sharpen);
            Section("Frame rate limit");
            const char* limits[]={"30","60","120","Off"};const int values[]={30,60,120,0};
            int limit=-1;for(int i=0;i<4;++i)if(values[i]==int(EmulatorSettings.GetFrameLimit()))limit=i;
            if(Segmented("fps-limit",limit,limits,4))Store(s.fps_limit,values[limit]);
            Note("Unlimited may affect game speed above 120 FPS.");
            Section("Frame generation");
            const char* modes[]={"Off","LSFG 2x","LSFG 3x","LSFG 4x"};
            int fg=s.frame_gen==BbSettings::FrameGenLsfg?std::clamp(s.frame_gen_multiplier.load()-1,1,3):0;
            const bool enabled[]={true,!s.lsfg_dll.empty(),!s.lsfg_dll.empty(),!s.lsfg_dll.empty()};
            if(Segmented("lsfg",fg,modes,4,enabled)){Store(s.frame_gen,fg?BbSettings::FrameGenLsfg:BbSettings::FrameGenOff);if(fg)Store(s.frame_gen_multiplier,fg+1);}
            Note("LSFG requires your own Lossless.dll.");
        } else if(tab==1) {
            Section("Performance HUD");Toggle("Show performance HUD",s.show_fps);
            const char* modes[]={"Minimal","Compact","Full"};int mode=s.hud_mode;
            if(Segmented("hud",mode,modes,3))Store(s.hud_mode,mode);
            Section("HUD position");
            const char* positions[]={"Top L","Top R","Bottom L","Bottom R"};int corner=s.hud_position;
            if(Segmented("position",corner,positions,4))Store(s.hud_position,corner);
            Slider("HUD size",s.hud_scale,.7f,3.f,true);
        } else if(tab==2) {
            Section("On-screen gamepad");
            const char* pads[]={"Off","Auto","On"};const int pad_values[]={0,2,1};int pad=s.touch_pad==0?0:s.touch_pad==1?2:1;
            if(Segmented("pad",pad,pads,3))Store(s.touch_pad,pad_values[pad]);
            Note("Auto: show touch controls when no controller is connected.");
            ImGui::BeginDisabled(s.touch_pad==0);
            Slider("Opacity",s.touch_pad_alpha,.1f,1.f,true);
            Slider("Size",s.touch_pad_scale,.6f,1.6f,true);
            Toggle("Swipe camera (instead of analog)",s.touch_mobile);
            Slider("Camera drag sensitivity",s.touch_pad_look,.25f,4.f);
            Note("Left: movement. Right: camera.");
            ImGui::EndDisabled();
        }
        ImGui::EndChild();ImGui::Separator();
        const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f;
        if(ImGui::Button(BbLocale::Text("Export log"),{half,0}))runtime_mobile_action(0);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.29f,.12f,.09f,1));
        if(ImGui::Button(BbLocale::Text("Exit game"),{half,0})){BbSettings::Save();runtime_mobile_action(1);}
        ImGui::PopStyleColor();
    }
    ImGui::End();ImGui::PopFont();ImGui::PopStyleColor(8);ImGui::PopStyleVar(8);
    if(dirty && (!keep_open || ImGui::GetTime()-last_change>1)){BbSettings::Save();dirty=false;}
}

TouchAction AndroidTouch(const SDL_Event& event, bool open) {
    static SDL_FingerID edge_finger = 0;
    static bool edge_tracking = false;
    static float edge_start_x = 0.0f;
    static float down_x = 0.0f, down_y = 0.0f;
    static float last_y = 0.0f;
    static bool down_outside = false, down_inside = false;
    constexpr float EdgeZone = 0.03f;   // of the screen width
    constexpr float SwipeLength = 0.08f;
    constexpr float TapSlop = 0.02f;
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
        if(!open && event.tfinger.x>=menu_x0 && event.tfinger.x<=menu_x1 && event.tfinger.y>=menu_y0 && event.tfinger.y<=menu_y1)
            return TouchAction::Open;
        if (!open && event.tfinger.x >= 1.0f - EdgeZone) {
            edge_finger = event.tfinger.fingerID;
            edge_tracking = true;
            edge_start_x = event.tfinger.x;
        }
        down_x = event.tfinger.x;
        down_y = last_y = event.tfinger.y;
        down_outside = open && event.tfinger.x < panel_left_norm.load();
        down_inside = open && !down_outside;
        return TouchAction::None;
    case SDL_EVENT_FINGER_MOTION:
        if (edge_tracking && event.tfinger.fingerID == edge_finger &&
            event.tfinger.x <= edge_start_x - SwipeLength) {
            edge_tracking = false;
            return TouchAction::Open;
        }
        if (down_inside) {
            // Scroll by the finger's travel once it is clearly a drag, not a tap.
            if (std::abs(event.tfinger.y - down_y) >= TapSlop) {
                pending_scroll.fetch_add(event.tfinger.y - last_y);
            }
            last_y = event.tfinger.y;
        }
        return TouchAction::None;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (edge_tracking && event.tfinger.fingerID == edge_finger) {
            edge_tracking = false;
        }
        down_inside = false;
        if (open && down_outside && event.type == SDL_EVENT_FINGER_UP &&
            std::abs(event.tfinger.x - down_x) < TapSlop &&
            std::abs(event.tfinger.y - down_y) < TapSlop) {
            down_outside = false;
            return TouchAction::Close;
        }
        return TouchAction::None;
    default:
        return TouchAction::None;
    }
}

} // namespace BbOverlay
