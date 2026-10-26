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
#include "core/emulator_settings.h"

extern "C" void runtime_restart(void);
extern "C" void runtime_mobile_action(int);

namespace BbOverlay {

namespace {

constexpr float TouchScale = 1.8f;   // on top of the display scale: 32 px text at 1080p
constexpr float PanelShare = 0.45f;  // of the screen width

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
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_ButtonActive]);
        }
        if (ImGui::Button(BbLocale::Text(labels[i]), ImVec2(width, 0.0f)) && !selected) {
            value = i;
            changed = true;
        }
        if (selected) {
            ImGui::PopStyleColor(2);
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
    bool v = value.load();
    if (ImGui::Checkbox(BbLocale::Text(label), &v)) {
        value.store(v);
        Changed();
    }
}

void Slider(const char* label, std::atomic<float>& value, float lo, float hi) {
    float v = value.load();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat(BbLocale::Text(label), &v, lo, hi, "%.2f")) {
        value.store(v);
        Changed();
    }
}

void Note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", BbLocale::Text(text));
    ImGui::PopStyleColor();
}

} // namespace

void AndroidMenuButton(float base_scale) {
    const auto size=ImGui::GetIO().DisplaySize;
    const float h=size.y, x=size.x*.5f+h*.16f, y=h*.055f;
    const ImVec2 a(x-h*.055f,y-h*.03f), b(x+h*.055f,y+h*.03f);
    menu_x0=a.x/size.x;menu_x1=b.x/size.x;menu_y0=a.y/h;menu_y1=b.y/h;
    auto* dl=ImGui::GetForegroundDrawList();
    dl->AddRectFilled(a,b,IM_COL32(12,20,18,190),h*.012f);
    dl->AddRect(a,b,IM_COL32(206,176,110,180),h*.012f,0,base_scale*1.4f);
    for(int i=-1;i<=1;++i)dl->AddLine(ImVec2(x-h*.018f,y+i*h*.01f),ImVec2(x+h*.018f,y+i*h*.01f),IM_COL32(245,235,210,240),base_scale*2);
}

void AndroidMenu(float base_scale, float frame_ms, bool& keep_open) {
    auto& s = BbSettings::Get();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = base_scale * TouchScale;
    const float width = std::floor(viewport->WorkSize.x * PanelShare);
    panel_left_norm.store((viewport->WorkSize.x - width) / viewport->WorkSize.x);

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - width,
                                   viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(width, viewport->WorkSize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale, 10.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * scale, 11.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * scale, 8.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 16.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 0.94f));
    ImGui::PushFont(nullptr, 18.0f * scale);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    ImGui::GetIO().MouseDrawCursor = false;
    if (ImGui::Begin("##android_menu", nullptr, flags)) {
        ImGui::TextColored(ImVec4(.88f,.77f,.53f,1),"UNCHARTED | DAEDALUS");
        const float action_width=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*2)/3;
        if(ImGui::Button(BbLocale::Text("Resume"),ImVec2(action_width,0)))keep_open=false;
        ImGui::SameLine();
        if(ImGui::Button(BbLocale::Text("Export log"),ImVec2(action_width,0)))runtime_mobile_action(0);
        ImGui::SameLine();
        if(ImGui::Button(BbLocale::Text("Exit game"),ImVec2(action_width,0))){BbSettings::Save();runtime_mobile_action(1);}
        ImGui::Separator();
        ImGui::BeginChild("settings-scroll",ImVec2(0,0),false);
        if (const float travel = pending_scroll.exchange(0.0f); travel != 0.0f)
            ImGui::SetScrollY(ImGui::GetScrollY() - travel * viewport->WorkSize.y);
        Section("Display");
        Toggle("Fullscreen",s.fullscreen);
        Note(s.fullscreen ? "Fill the entire screen" : "Keep the original aspect ratio");
        Toggle("Show performance HUD",s.show_fps);
        static const char* hud_names[]={"Minimal","Compact","Full"};
        int hud=s.hud_mode;if(Segmented("hud",hud,hud_names,3))Store(s.hud_mode,hud);
        static const char* hud_corners[]={"Top L","Top R","Bottom L","Bottom R"};
        int corner=s.hud_position;if(Segmented("hud-pos",corner,hud_corners,4))Store(s.hud_position,corner);
        Slider("HUD size",s.hud_scale,.7f,1.6f);
        // Actual render and output dimensions, including startup-patched FSR sessions.
        int scene_w = s.active_render_width.load(), scene_h = s.active_render_height.load();
        const bool seen = scene_w > 0; // the upscaler has met the scene
        if (!seen) {
            scene_w = s.game_display_width.load();
            scene_h = s.game_display_height.load();
        }
        const int out_w = BbSettings::OutputWidths[s.output_res];
        const int out_h = BbSettings::OutputHeights[s.output_res];
        const bool startup_scaled = std::getenv("BB_RENDER_RES") && std::getenv("BB_RENDER_RES")[0];
        ImGui::TextDisabled(BbLocale::Text("Game rendered at %d x %d%s"), scene_w, scene_h,
                            seen ? "" : " (its display size; no 3D scene met yet)");
        ImGui::TextDisabled(BbLocale::Text("Shown at %d x %d"), startup_scaled ? 1280 : out_w, startup_scaled ? 720 : out_h);

        Section("Upscaling");
        static const char* upscalers[] = {"Off", "FSR 3.1", "TAA"};
        static const int upscaler_values[] = {BbSettings::UpscalerOff, BbSettings::UpscalerFsr3,
                                              BbSettings::UpscalerTaa};
        int upscaler = 0;
        for (int i = 0; i < 3; ++i) {
            if (upscaler_values[i] == s.upscaler.load()) {
                upscaler = i;
            }
        }
        if (Segmented("upscaler", upscaler, upscalers, 3)) {
            Store(s.upscaler, upscaler_values[upscaler]);
        }
        if (s.upscaler == BbSettings::UpscalerOff) {
            Note("Off: the game's own anti-aliasing");
        } else {
            if(startup_scaled)ImGui::TextDisabled(BbLocale::Text("FSR 3.1: %d x %d -> 1280 x 720"),scene_w,scene_h);
            else ImGui::TextDisabled(BbLocale::Text("Temporal AA at %d x %d"),scene_w,scene_h);
        }
        if (const char* problem = s.upscaler_problem.load();
            problem && s.upscaler == BbSettings::UpscalerFsr3) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", problem);
            ImGui::PopStyleColor();
        }
        Note("Resolution and FSR quality are selected in the launcher for the next start.");

        if(s.upscaler!=BbSettings::UpscalerOff)
            Note(s.temporal_running ? "Temporal reconstruction: running" : "Temporal reconstruction: waiting for a compatible scene");

        Section("Sharpening");
        Toggle("Sharpen (RCAS)", s.sharpen);
        ImGui::BeginDisabled(!s.sharpen);
        Slider("##sharpness", s.sharpness, 0.0f, 2.0f);
        ImGui::EndDisabled();

        Section("Frame generation");
        static const char* frame_gens[] = {"Off", "FSR 3"};
        int frame_gen = s.frame_gen == BbSettings::FrameGenFsr3 ? 1 : 0;
        if (Segmented("framegen", frame_gen, frame_gens, 2)) {
            Store(s.frame_gen, frame_gen ? BbSettings::FrameGenFsr3 : BbSettings::FrameGenOff);
        }
        if (const char* problem = s.frame_gen_problem.load()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", problem);
            ImGui::PopStyleColor();
        } else if (frame_gen) {
            ImGui::TextDisabled("%s",BbLocale::Text("One generated frame between two of the game's"));
        }

        Section("Frame rate limit");
        static const char* limits[] = {"30", "60", "120", "Off"};
        static const int limit_values[] = {30, 60, 120, 0};
        const int effective = int(EmulatorSettings.GetFrameLimit());
        int limit = -1;
        for (int i = 0; i < 4; ++i) {
            if (limit_values[i] == effective) {
                limit = i;
            }
        }
        if (Segmented("fps_limit", limit, limits, 4)) {
            Store(s.fps_limit, limit_values[limit]);
        }
        if (effective == 0) {
            ImGui::TextDisabled(BbLocale::Text("No limit; above 120 FPS the game's movement timing breaks (screen: %u Hz)"), BbDisplayRefreshHz());
        } else {
            ImGui::TextDisabled(BbLocale::Text("%d FPS at most (screen: %u Hz)"), effective, BbDisplayRefreshHz());
        }

        Section("On-screen gamepad");
        static const char* pads[] = {"Off", "Auto", "On"};
        static const int pad_values[] = {0, 2, 1};
        int pad = s.touch_pad == 0 ? 0 : s.touch_pad == 1 ? 2 : 1;
        if (Segmented("touch_pad", pad, pads, 3)) {
            Store(s.touch_pad, pad_values[pad]);
        }
        Toggle("Swipe camera (instead of analog)",s.touch_mobile);
        Note("Auto: shown while no gamepad is connected. Left half: a stick under the finger; "
             "right half: drag to look around; the buttons at the edges.");
        ImGui::BeginDisabled(s.touch_pad == 0);
        ImGui::TextDisabled("%s",BbLocale::Text("Opacity"));
        Slider("##touch_alpha", s.touch_pad_alpha, 0.1f, 1.0f);
        ImGui::TextDisabled("%s",BbLocale::Text("Size"));
        Slider("##touch_scale", s.touch_pad_scale, 0.6f, 1.6f);
        ImGui::TextDisabled("%s",BbLocale::Text("Camera drag sensitivity"));
        Slider("##touch_look", s.touch_pad_look, 0.25f, 4.0f);
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(BbLocale::Text("Advanced"))) {
            Toggle("Sub-pixel jitter", s.jitter);
            Toggle("Character motion vectors", s.object_motion);
            Toggle("Reactive mask", s.reactive);
            ImGui::BeginDisabled(!s.reactive);
            ImGui::TextDisabled("%s",BbLocale::Text("Scale"));
            Slider("##reactive_scale", s.reactive_scale, 0.0f, 4.0f);
            ImGui::TextDisabled("%s",BbLocale::Text("Threshold"));
            Slider("##reactive_threshold", s.reactive_threshold, 0.0f, 1.0f);
            ImGui::TextDisabled("%s",BbLocale::Text("Maximum"));
            Slider("##reactive_max", s.reactive_max, 0.0f, 1.0f);
            ImGui::EndDisabled();
        }

        const bool restart = s.object_motion != s.startup_object_motion ||
                             BbSettings::ResolutionNeedsRestart();
        if (restart) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.3f, 1.0f));
            ImGui::TextWrapped("Some of these take effect after a restart.");
            ImGui::PopStyleColor();
            if (ImGui::Button(BbLocale::Text("Apply and restart the game"), ImVec2(-FLT_MIN, 0.0f))) {
                BbSettings::Save();
                runtime_restart();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button(BbLocale::Text("Close##bottom"), ImVec2(-FLT_MIN, 0.0f))) {
            keep_open = false;
        }
        ImGui::TextDisabled("%s",BbLocale::Text("Menu button, right-edge swipe or L3+R3"));
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(8);
    if (dirty && (!keep_open || ImGui::GetTime() - last_change > 1.0)) {
        BbSettings::Save();
        dirty = false;
    }
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
