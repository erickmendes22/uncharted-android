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
#include <cstdlib>
#include <cstring>
#include <string>

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
constexpr float PanelShare = 0.53f;  // of the screen width

std::atomic<float> menu_x0{.52f}, menu_x1{.58f}, menu_y0{.02f}, menu_y1{.09f};
std::atomic<float> panel_left_norm{0.53f}; // the panel's left edge, 0..1 of the screen width
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
    // The native Android controller now owns opening the panel via a swipe
    // from the left edge. Suppress the legacy ImGui three-bars icon entirely.
    if (const char* java_touch = std::getenv("BB_JAVA_TOUCH");
        java_touch && java_touch[0] == '1') return;
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


struct MobilePatch {
    const char* key;
    const char* label;
};
constexpr MobilePatch MobilePatches[]={
    {"patch_Disable water reflections","Water reflections OFF"},
    {"patch_Disable SSAO","SSAO OFF"},
    {"patch_Disable fog","Fog OFF"},
    {"patch_Disable volumetric lights","Volumetric lights OFF"},
    {"patch_Disable dynamic lights","Dynamic lights OFF"},
    {"patch_Disable sunlight ambient occlusion","Sunlight occlusion OFF"},
    {"patch_Disable motion blur","Motion blur OFF"},
    {"patch_Disable depth of field","Depth of field OFF"},
    {"patch_Disable bloom","Bloom OFF"},
    {"patch_Disable anti-aliasing","Anti-aliasing OFF"},
    {"patch_Disable anisotropic filtering","Anisotropic filtering OFF"},
    {"patch_Draw distance: 40 m (extreme)","Draw distance: 40 m"},
    {"patch_Skip startup logos","Skip opening logos"}
};
constexpr int MobilePatchCount=int(sizeof(MobilePatches)/sizeof(MobilePatches[0]));
struct MobileDraft {
    bool loaded=false;
    bool changes=false;
    bool patch[MobilePatchCount]{};
    int upscale=0; // 0 Off, 1..4 FSR, 5..8 Snapdragon GSR2
    int shadows=0; // 0 original, 1 two cascades, 2 one cascade
    bool small_readbacks=false;
};
MobileDraft draft;
void LoadDraft() {
    draft=MobileDraft{};
    draft.loaded=true;
    if(const char* filename=std::getenv("BB_PATCH_STATE")){
        if(FILE* f=std::fopen(filename,"rb")){
            char line[256];
            while(std::fgets(line,sizeof(line),f)){
                const char* equals=std::strchr(line,'=');
                if(!equals)continue;
                std::string key(line,equals-line),value(equals+1);
                const int n=std::atoi(value.c_str());
                if(key=="upscale" && n>=0 && n<=8)draft.upscale=n;
                if(key=="shadow_cascades" && n>=0 && n<=2)draft.shadows=n;
                if(key=="gpu_small_readbacks")draft.small_readbacks=n!=0;
                for(int i=0;i<MobilePatchCount;++i)
                    if(key==MobilePatches[i].key)draft.patch[i]=n!=0;
            }
            std::fclose(f);
        }
    }
}
bool RestartWithDraft(bool apply_draft) {
    const char* request=std::getenv("BB_GAME_CONFIG_REQUEST");
    if(!request)return false;
    FILE* file=std::fopen(request,"wb");
    if(!file)return false;
    const char* game=std::getenv("BB_GAME_ID");
    std::fprintf(file,"game=%s\n",game && std::strlen(game)==2?game:"u1");
    if(apply_draft){
        std::fprintf(file,"upscale=%d\nshadow_cascades=%d\ngpu_small_readbacks=%d\n",
                     draft.upscale,draft.shadows,int(draft.small_readbacks));
        for(int i=0;i<MobilePatchCount;++i)
            std::fprintf(file,"%s=%d\n",MobilePatches[i].key,int(draft.patch[i]));
    }
    const bool ok=std::fclose(file)==0;
    if(!ok)return false;
    BbSettings::Save();
    runtime_restart(); // Restart helper opens Launcher to reapply verified guest recipes.
    return true;
}
bool NavItem(int index,const char* label,int active,float width) {
    ImGui::PushID(index);
    if(index==active){
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.79f,.64f,.37f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.9f,.73f,.45f,1));
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.09f,.13f,.17f,1));
    }
    const bool clicked=ImGui::Button(BbLocale::Text(label),ImVec2(width,0));
    if(index==active)ImGui::PopStyleColor(3);
    ImGui::PopID();
    return clicked;
}

void AndroidMenu(float base_scale,float frame_ms,bool& keep_open) {
    auto& s=BbSettings::Get();
    const auto* viewport=ImGui::GetMainViewport();
    const float scale=base_scale*TouchScale;
    const float width=std::floor(viewport->WorkSize.x*PanelShare);
    panel_left_norm.store(width/viewport->WorkSize.x);
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize({width,viewport->WorkSize.y});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(13*scale,12*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(8*scale,7*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(6*scale,8*scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,7*scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,15*scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,10*scale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.043f,.075f,.108f,.985f));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.105f,.164f,.208f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.25f,.32f,.33f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.42f,.36f,.22f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.115f,.17f,.21f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(.79f,.64f,.37f,1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(.95f,.8f,.5f,1));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.94f,.91f,.83f,1));
    ImGui::PushFont(nullptr,17*scale);
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
                     ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings;
    ImGui::GetIO().MouseDrawCursor=false;
    if(!draft.loaded)LoadDraft();
    if(ImGui::Begin("##uncharted-mobile-menu",nullptr,flags)){
        auto* bg=ImGui::GetWindowDrawList();
        const ImVec2 pos=ImGui::GetWindowPos(),extent=ImGui::GetWindowSize();
        bg->AddRectFilledMultiColor(pos,{pos.x+extent.x,pos.y+extent.y},
            IM_COL32(8,24,39,255),IM_COL32(19,36,49,255),
            IM_COL32(10,21,33,255),IM_COL32(7,20,30,255));
        bg->AddRectFilled({pos.x,pos.y},{pos.x+3*scale,pos.y+extent.y},IM_COL32(201,162,94,255));
        const float resume_width=std::max(ImGui::CalcTextSize(BbLocale::Text("Resume")).x+16*scale,width*.24f);
        ImGui::TextColored(ImVec4(.87f,.72f,.45f,1),"UNCHARTED");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(.77f,.66f,.47f,1),"DAEDALUS");
        ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-resume_width);
        if(ImGui::Button(BbLocale::Text("Resume"),{resume_width,0}))keep_open=false;
        ImGui::Separator();
        static int tab=0;
        const char* tabs[]={"Video","Performance","Patches","HUD","Controls","System"};
        const float side=width*.26f;
        const float area=ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeightWithSpacing()*1.6f;
        ImGui::BeginChild("##left-menu",{side,area},false);
        for(int i=0;i<6;++i){
            if(NavItem(i,tabs[i],tab,ImGui::GetContentRegionAvail().x)){tab=i;ImGui::SetScrollY(0);}
        }
        ImGui::Separator();
        const char* id=std::getenv("BB_GAME_ID");
        ImGui::TextDisabled("UNCHARTED %s",id && std::strlen(id)==2?id+1:"1");
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##settings-panel",{0,area},false);
        if(const float travel=pending_scroll.exchange(0.f);travel!=0.f)
            ImGui::SetScrollY(ImGui::GetScrollY()-travel*viewport->WorkSize.y);
        const float full=ImGui::GetContentRegionAvail().x;
        if(tab==0) {
            Section("Video");
            Toggle("Fullscreen",s.fullscreen);
            Note(s.fullscreen?"Fill the entire screen":"Keep the original aspect ratio");
            Section("Upscaling");
            const char* providers[]={"Off","FSR 3.1","GSR 2"};
            int provider=draft.upscale==0?0:draft.upscale>=5?2:1;
            if(Segmented("provider",provider,providers,3)){
                const int quality=draft.upscale==0?2:(draft.upscale>=5?draft.upscale-5:draft.upscale-1);
                draft.upscale=provider==0?0:(provider==1?1:5)+std::clamp(quality,0,3);
                draft.changes=true;
            }
            if(draft.upscale!=0){
                Section("Quality mode");
                const char* qualities[]={"Native AA","Quality","Balanced","Performance"};
                int quality=draft.upscale>=5?draft.upscale-5:draft.upscale-1;
                if(Segmented("quality",quality,qualities,4)){
                    draft.upscale=(draft.upscale>=5?5:1)+quality;
                    draft.changes=true;
                }
            } else {
                Note("Manual internal resolution: configure for this game in Launcher patches.");
            }
            if(draft.changes){
                ImGui::TextColored(ImVec4(.95f,.77f,.48f,1),"%s",
                    BbLocale::Text("Changes to upscaling or patches require restarting the game."));
                if(ImGui::Button(BbLocale::Text("Save and restart game"),{-1,0}))RestartWithDraft(true);
            } else if(s.upscaler!=BbSettings::UpscalerOff){
                Note(s.temporal_running?"Temporal reconstruction: running":
                                        "Temporal reconstruction: waiting for a compatible scene");
            }
            Slider("Sharpening",s.sharpness,0.f,2.f);
            Toggle("Sharpen (RCAS)",s.sharpen);
        }else if(tab==1){
            Section("Performance");
            Section("Frame rate limit");
            const char* limits[]={"30","60","120","Off"};
            const int values[]={30,60,120,0};
            int limit=3;
            for(int i=0;i<4;++i)if(values[i]==int(EmulatorSettings.GetFrameLimit()))limit=i;
            if(Segmented("fps-limit",limit,limits,4))Store(s.fps_limit,values[limit]);
            Section("Frame generation");
            const char* modes[]={"Off","LSFG 2x","LSFG 3x","LSFG 4x"};
            int fg=s.frame_gen==BbSettings::FrameGenLsfg?
                    std::clamp(s.frame_gen_multiplier.load()-1,1,3):0;
            const bool available[]={true,!s.lsfg_dll.empty(),!s.lsfg_dll.empty(),!s.lsfg_dll.empty()};
            if(Segmented("frame-gen",fg,modes,4,available)){
                Store(s.frame_gen,fg?BbSettings::FrameGenLsfg:BbSettings::FrameGenOff);
                if(fg)Store(s.frame_gen_multiplier,fg+1);
            }
            Note("LSFG requires your own Lossless.dll.");
            Section("GPU readback");
            bool small=draft.small_readbacks;
            if(ImGui::Checkbox(BbLocale::Text("Small GPU result readbacks (U2/U3)"),&small)){
                draft.small_readbacks=small;draft.changes=true;
            }
            Note("Optional: small GPU readbacks may reduce FPS. Enable only if necessary.");
            const char* driver=std::getenv("BB_VK_DRIVER");
            ImGui::Text("%s: %s",BbLocale::Text("Vulkan driver"),driver?driver:BbLocale::Text("Automatic"));
            if(draft.changes && ImGui::Button(BbLocale::Text("Save and restart game"),{-1,0}))
                RestartWithDraft(true);
        }else if(tab==2){
            Section("Patches");
            Note("Patches apply only when the executable signature matches. Restart required.");
            const char* shadows[]={"Original","Reduced · 2 cascades","Minimum · 1 cascade"};
            if(Segmented("shadows",draft.shadows,shadows,3))draft.changes=true;
            ImGui::Separator();
            for(int i=0;i<MobilePatchCount;++i){
                ImGui::PushID(i);
                bool enabled=draft.patch[i];
                if(ImGui::Checkbox(BbLocale::Text(MobilePatches[i].label),&enabled)){
                    draft.patch[i]=enabled;draft.changes=true;
                }
                ImGui::PopID();
            }
            if(draft.upscale!=0)Note("Upscaling controls native AA; anti-aliasing patches are verified on restart.");
            if(draft.changes && ImGui::Button(BbLocale::Text("Apply patches and restart"),{-1,0}))
                RestartWithDraft(true);
        }else if(tab==3){
            Section("Performance HUD");
            Toggle("Show performance HUD",s.show_fps);
            const char* modes[]={"Minimal","Compact","Full"};
            int mode=s.hud_mode;
            if(Segmented("hud",mode,modes,3))Store(s.hud_mode,mode);
            Section("HUD position");
            const char* positions[]={"Top L","Top R","Bottom L","Bottom R"};
            int corner=s.hud_position;
            if(Segmented("hud-pos",corner,positions,4))Store(s.hud_position,corner);
            Slider("HUD size",s.hud_scale,.7f,3.f,true);
        }else if(tab==4){
            Section("On-screen gamepad");
            const char* pads[]={"Off","Auto","On"};
            const int pad_values[]={0,2,1};
            int pad=s.touch_pad==0?0:s.touch_pad==1?2:1;
            if(Segmented("pad",pad,pads,3))Store(s.touch_pad,pad_values[pad]);
            Note("Auto: show touch controls when no controller is connected.");
            ImGui::BeginDisabled(s.touch_pad==0);
            Slider("Opacity",s.touch_pad_alpha,.1f,1.f,true);
            Slider("Size",s.touch_pad_scale,.6f,1.6f,true);
            Slider("Camera drag sensitivity",s.touch_pad_look,.25f,4.f);
            Section("Android touch layout");
            bool action=true;
            if(const char* path=std::getenv("BB_TOUCH_STYLE_STATE")){
                if(FILE* file=std::fopen(path,"rb")){
                    char text[16]={};
                    if(std::fgets(text,sizeof(text),file)&&text[0]=='c')action=false;
                    std::fclose(file);
                }
            }
            Note(action?"Current: Uncharted (GoW3 action icons)":"Current: Classic PlayStation");
            if(ImGui::Button(BbLocale::Text(action?"Switch to Classic PlayStation":
                                               "Switch to Uncharted action icons"),{-1,0})){
                if(const char* request=std::getenv("BB_TOUCH_STYLE_REQUEST"))
                    if(FILE* f=std::fopen(request,"wb")){std::fputc('1',f);std::fclose(f);}
                keep_open=false;
            }
            if(ImGui::Button(BbLocale::Text("Edit touch layout (move, resize, icons)"),{-1,0})){
                if(const char* request=std::getenv("BB_TOUCH_EDIT_REQUEST"))
                    if(FILE* f=std::fopen(request,"wb")){std::fputc('1',f);std::fclose(f);}
                keep_open=false;
            }
            ImGui::EndDisabled();
        }else {
            Section("System");
            Note("Settings for each Uncharted game are independent in the launcher.");
            if(ImGui::Button(BbLocale::Text("Return to launcher"),{-1,0})){
                if(const char* request=std::getenv("BB_GAME_CONFIG_REQUEST")){
                    // Restart helper selects Launcher rather than GameActivity.
                    std::string path(request);
                    const auto slash=path.find_last_of('/');
                    if(slash!=std::string::npos){
                        path.resize(slash+1);path+="return-to-launcher";
                        if(FILE* f=std::fopen(path.c_str(),"wb")){std::fputc('1',f);std::fclose(f);}
                    }
                }
                BbSettings::Save();runtime_restart();
            }
            if(ImGui::Button(BbLocale::Text("Restart game"),{-1,0})){
                BbSettings::Save();runtime_restart();
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f;
        if(ImGui::Button(BbLocale::Text("Export log"),{half,0}))runtime_mobile_action(0);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.35f,.12f,.11f,1));
        if(ImGui::Button(BbLocale::Text("Exit game"),{half,0})){BbSettings::Save();runtime_mobile_action(1);}
        ImGui::PopStyleColor();
    }
    ImGui::End();ImGui::PopFont();ImGui::PopStyleColor(8);ImGui::PopStyleVar(8);
    if(dirty && (!keep_open || ImGui::GetTime()-last_change>1)){
        BbSettings::Save();dirty=false;
    }
}

TouchAction AndroidTouch(const SDL_Event& event, bool open) {
    // Java accepts left-edge swipes across the phone; SDL handles only
    // closing and panel scrolling when Java's native overlay is active.
    const char* java_touch = std::getenv("BB_JAVA_TOUCH");
    if (java_touch && java_touch[0] == '1' && !open) return TouchAction::None;
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
        if (!open && event.tfinger.x <= EdgeZone) {
            edge_finger = event.tfinger.fingerID;
            edge_tracking = true;
            edge_start_x = event.tfinger.x;
        }
        down_x = event.tfinger.x;
        down_y = last_y = event.tfinger.y;
        down_outside = open && event.tfinger.x > panel_left_norm.load();
        down_inside = open && !down_outside;
        return TouchAction::None;
    case SDL_EVENT_FINGER_MOTION:
        if (edge_tracking && event.tfinger.fingerID == edge_finger &&
            event.tfinger.x >= edge_start_x + SwipeLength) {
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
