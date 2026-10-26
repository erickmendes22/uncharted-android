#include "bbport_fps_meter.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_overlay.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <cstring>
#include <thread>
#include <ctime>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <vector>
#if defined(__linux__)
#include <pthread.h>
#endif

#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include "bbport_overlay_android.h" // g2-arm64: the touch menu
#include "bbport_touchpad.h"          // g2-arm64: the on-screen gamepad
#endif
#include "bbport_settings.h"
#include "bbport_locale.h"
#include "bbport_toggles.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include <vk_mem_alloc.h>

// DejaVu Sans (Cyrillic), embedded (third_party/fonts, Bitstream Vera license).
#ifdef _WIN32
asm(".section .rdata,\"dr\"\n"
    ".balign 16\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".text\n");
#else
asm(".section .rodata\n"
    ".balign 16\n"
    ".hidden bb_font_ttf\n"
    ".global bb_font_ttf\n"
    "bb_font_ttf:\n"
    ".incbin \"" BB_FONT_PATH "\"\n"
    ".hidden bb_font_ttf_end\n"
    ".global bb_font_ttf_end\n"
    "bb_font_ttf_end:\n"
    ".previous\n");
#endif
extern "C" const unsigned char bb_font_ttf[];
extern "C" const unsigned char bb_font_ttf_end[];

extern "C" void runtime_mobile_action(int action);
extern "C" void runtime_restart(void); // bb-probe (probe.c)
extern "C" size_t runtime_memory_protect_count(void); // g2-arm64: runtime_memory.c

namespace BbOverlay {

namespace {

std::mutex imgui_mutex; // the ImGui context: window thread (input) and present thread
bool initialized = false;
std::atomic<bool> menu_open{false};
bool l3_down = false, r3_down = false;
bool dirty = false; // settings changed while open: saved on close
// The game's text dialog (SetTextEntry), guarded by imgui_mutex.
bool text_entry_active = false;
std::string text_entry_prompt, text_entry_text;
float base_scale = 1.0f;

// Present rate for the FPS counter.
std::chrono::steady_clock::time_point last_present{};
float frame_ms_avg = 0.0f, last_present_ms = 0.0f;
FpsMeter presented_fps;

void SetOpen(bool value) {
    if (menu_open.exchange(value) == value) {
        return;
    }
    ImGui::GetIO().MouseDrawCursor = value;
#ifdef __ANDROID__
    if (value) {
        TouchPadReset(); // g2-arm64: the on-screen gamepad lets go while the panel is open
    }
#endif
    if (!value && dirty) {
        dirty = false;
        BbSettings::Save();
    }
}

ImGuiKey KeyFromSdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

ImGuiKey KeyFromGamepad(u8 button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return ImGuiKey_GamepadFaceDown;
    case SDL_GAMEPAD_BUTTON_EAST: return ImGuiKey_GamepadFaceRight;
    case SDL_GAMEPAD_BUTTON_WEST: return ImGuiKey_GamepadFaceLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return ImGuiKey_GamepadFaceUp;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return ImGuiKey_GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ImGuiKey_GamepadL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
    case SDL_GAMEPAD_BUTTON_START: return ImGuiKey_GamepadStart;
    case SDL_GAMEPAD_BUTTON_BACK: return ImGuiKey_GamepadBack;
    default: return ImGuiKey_None;
    }
}

float PixelDensity(SDL_WindowID id) {
    SDL_Window* window = SDL_GetWindowFromID(id);
    const float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    return density > 0.0f ? density : 1.0f;
}

// Marks the settings dirty when a widget changed them.
template <typename T>
void Store(std::atomic<T>& target, T value, bool changed) {
    if (changed) {
        target = value;
        dirty = true;
    }
}

void Checkbox(const char* label, std::atomic<bool>& value) {
    bool v = value;
    Store(value, v, ImGui::Checkbox(label, &v));
}

void Slider(const char* label, std::atomic<float>& value, float lo, float hi) {
    float v = value;
    Store(value, v, ImGui::SliderFloat(label, &v, lo, hi, "%.2f"));
}

void Hint(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Menu() {
    auto& s = BbSettings::Get();
    const auto* viewport = ImGui::GetMainViewport();
    const float pad = 16.0f * base_scale;
    const float width = std::min(570.0f * base_scale, viewport->WorkSize.x - pad * 2);
    const float height = std::min(690.0f * base_scale, viewport->WorkSize.y - pad * 2);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + pad, viewport->WorkPos.y + pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    bool keep_open = true;
    if (!ImGui::Begin("UNCHARTED | Daedalus", &keep_open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::End(); return;
    }
    ImGui::TextColored(ImVec4(0.80f, 0.66f, 0.42f, 1.0f), "THE NATHAN DRAKE COLLECTION");
    ImGui::Text("%.0f FPS  |  %.1f ms", frame_ms_avg > 0 ? 1000.0f / frame_ms_avg : 0, frame_ms_avg);
    ImGui::Separator();
    if (ImGui::BeginTabBar("Expedition settings")) {
        if (ImGui::BeginTabItem("Graphics")) {
            ImGui::Spacing();
            ImGui::Text("Scene: %d x %d", s.active_render_width.load(), s.active_render_height.load());
            static const char* upscalers[] = {"Off", "FSR 3.1", "FSR 4 (INT8)", "FSR 4.1.1 (INT8)", "TAA", "DLSS"};
            int upscaler = s.upscaler;
            if (ImGui::BeginCombo("Anti-aliasing / upscaler", upscalers[upscaler])) {
                for (int i = 0; i < BbSettings::UpscalerCount; ++i) {
                    bool supported = i == BbSettings::UpscalerFsr4 ? s.fsr4_supported.load()
                        : i == BbSettings::UpscalerFsr411 ? s.fsr411_supported.load()
                        : i == BbSettings::UpscalerDlss ? s.dlss_supported.load() : true;
                    ImGui::BeginDisabled(!supported);
                    if (ImGui::Selectable(upscalers[i], i == upscaler)) Store(s.upscaler, i, true);
                    ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
            const bool mobile_mode=std::getenv("BB_MOBILE_UPSCALE") && std::getenv("BB_MOBILE_UPSCALE")[0]=='1';
            if(mobile_mode) {
                int preset = s.preset;
                if (ImGui::BeginCombo("Preset", BbSettings::PresetName(preset))) {
                    for (int i = 0; i < BbSettings::PresetCount; ++i)
                        if (ImGui::Selectable(BbSettings::PresetName(i), i == preset)) Store(s.preset, i, true);
                    ImGui::EndCombo();
                }
            } else ImGui::TextWrapped("Enable temporal upscale in the launcher for Quality / Performance presets (next game start).");
            if (const char* error = s.upscaler_problem.load()) ImGui::TextWrapped("%s", error);
            if (const char* error = s.fsr4_problem.load()) ImGui::TextWrapped("%s", error);
            if (const char* error = s.dlss_problem.load(); error && s.upscaler == BbSettings::UpscalerDlss)
                ImGui::TextWrapped("%s", error);
            Checkbox("Sharpen", s.sharpen);
            Slider("Sharpness", s.sharpness, 0, 2);
            Checkbox("Show FPS", s.show_fps);
            static const char* hud_modes[]={"FPS only","Compact","Detailed"};
            int hud=s.hud_mode;if(ImGui::Combo("HUD style",&hud,hud_modes,3))Store(s.hud_mode,hud,true);
            static const char* corners[]={"Top left","Top right","Bottom left","Bottom right"};
            int corner=s.hud_position;if(ImGui::Combo("HUD position",&corner,corners,4))Store(s.hud_position,corner,true);
            Slider("HUD size",s.hud_scale,.7f,3.f);
            static const int rates[] = {-1, 0, 30, 60, 120};
            static const char* limits[] = {"Automatic", "Unlimited", "30 FPS", "60 FPS", "120 FPS"};
            int selected = 0;
            for (int i = 0; i < 5; ++i) if (s.fps_limit == rates[i]) selected = i;
            if (ImGui::BeginCombo("FPS target", limits[selected])) {
                for (int i = 0; i < 5; ++i)
                    if (ImGui::Selectable(limits[i], i == selected)) Store(s.fps_limit, rates[i], true);
                ImGui::EndCombo();
            }
            ImGui::TextWrapped("Internal resolution patches are in the launcher. FSR and TAA operate on the scene size provided by the game.");
            if (ImGui::CollapsingHeader("Advanced graphics")) {
                Checkbox("Subpixel jitter", s.jitter);
                Checkbox("Object motion vectors", s.object_motion);
                Checkbox("Reactive mask", s.reactive);
                Slider("Reactive scale", s.reactive_scale, 0, 4);
                Slider("Reactive threshold", s.reactive_threshold, 0, 1);
                Slider("Reactive maximum", s.reactive_max, 0, 1);
                Checkbox("FSR 4 automatic exposure", s.fsr4_auto_exposure);
                Checkbox("FSR 4 invert jitter", s.fsr4_invert_jitter);
                static const char* debug[] = {"Off", "Reactive mask", "Motion vectors"};
                int debug_view = s.debug_view;
                if (ImGui::Combo("Debug view", &debug_view, debug, 3)) Store(s.debug_view, debug_view, true);
                if(mobile_mode) ImGui::TextUnformatted("Temporal output: 1920 x 1080 (native UI)");
                static const char* modes[] = {"Automatic", "Off", "On"};
                int live = s.live_resolution + 1;
                if (ImGui::Combo("Live resolution changes", &live, modes, 3)) Store(s.live_resolution, live - 1, true);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Frame generation")) {
            ImGui::Spacing();
            static const char* providers[] = {"Off", "FSR 3.1", "DLSS", "LSFG (Lossless.dll)"};
            int provider = s.frame_gen;
            if (ImGui::BeginCombo("Provider", providers[provider])) {
                for (int i = 0; i < BbSettings::FrameGenCount; ++i) {
                    ImGui::BeginDisabled(i == BbSettings::FrameGenDlss && s.dlssg_max_multiplier <= 1);
                    if (ImGui::Selectable(providers[i], i == provider)) Store(s.frame_gen, i, true);
                    ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
            const int max = s.frame_gen == BbSettings::FrameGenLsfg ? 4
                : s.frame_gen == BbSettings::FrameGenDlss ? s.dlssg_max_multiplier.load() : 2;
            int mult = std::clamp(s.frame_gen_multiplier.load(), 2, std::max(2, max));
            ImGui::BeginDisabled(s.frame_gen == BbSettings::FrameGenOff || max <= 2);
            if (ImGui::SliderInt("Multiplier", &mult, 2, std::max(2, max), "%dx")) Store(s.frame_gen_multiplier, mult, true);
            ImGui::EndDisabled();
            if (s.frame_gen == BbSettings::FrameGenLsfg) {
                ImGui::TextWrapped("Import your own Lossless.dll in the launcher. Higher multipliers add GPU and memory cost.");
                int flow = s.lsfg_flow_scale;
                if (ImGui::SliderInt("Motion resolution", &flow, 25, 100, "%d%%")) Store(s.lsfg_flow_scale, flow, true);
                Checkbox("FP16 when supported", s.lsfg_fp16);
            }
            if (const char* error = s.frame_gen_problem.load()) ImGui::TextWrapped("%s", error);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls")) {
            ImGui::Spacing();
            Checkbox("Swipe camera instead of analog",s.touch_mobile);
            static const char* modes[] = {"Off", "On", "Automatic (no controller)"};
            int mode = s.touch_pad;
            if (ImGui::Combo("Touch controls", &mode, modes, 3)) Store(s.touch_pad, mode, true);
            Slider("Opacity", s.touch_pad_alpha, 0.1f, 1.0f);
            Slider("Button size", s.touch_pad_scale, 0.6f, 1.6f);
            Slider("Camera sensitivity", s.touch_pad_look, 0.25f, 4.0f);
            ImGui::TextWrapped("Drag on the right half to move the camera. Open this menu from the right edge or with L3 + R3 on a controller.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    const bool restart = s.object_motion != s.startup_object_motion ||
                         s.live_resolution != s.startup_live_resolution || BbSettings::ResolutionNeedsRestart();
    if (restart) {
        ImGui::Separator();
        if (ImGui::Button("Save and restart game")) { BbSettings::Save(); runtime_restart(); }
    }
    ImGui::Spacing();
    if (ImGui::Button("Export Uncharted log...",ImVec2(-1,38.0f*base_scale))) runtime_mobile_action(0);
    if (ImGui::Button("Exit game",ImVec2(-1,38.0f*base_scale))) { BbSettings::Save(); runtime_mobile_action(1); }
    if (ImGui::Button("Save and return to game", ImVec2(-1, 42.0f * base_scale))) keep_open = false;
    ImGui::End();
    if (!keep_open) SetOpen(false);
}

// The game's text dialog: what is typed, and how to finish (keyboard or controller).
void TextEntryBox() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.42f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x * 0.32f, 0.0f),
                                        ImVec2(viewport->WorkSize.x * 0.8f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##text_entry", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.45f, 1.0f), "%s", text_entry_prompt.c_str());
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("%s_", text_entry_text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Keyboard: type, Backspace to erase, Enter = OK, Esc = cancel");
    ImGui::TextDisabled("Controller: Cross (A) = OK, Circle (B) = cancel");
    ImGui::End();
}

void FpsCounter() {
    const auto& settings=BbSettings::Get();
    struct Sample { double at=0,cpu=0;float cpu_percent=0,ram_mib=0;long long gpu_busy=0,gpu_total=0;int gpu=-1; };
    static Sample sample;
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(now-sample.at>=1.0) {
#if defined(__linux__)
        struct timespec cpu_time{};clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&cpu_time);
        double cpu=cpu_time.tv_sec+cpu_time.tv_nsec*1e-9;
        if(sample.at>0)sample.cpu_percent=std::clamp(float(100*(cpu-sample.cpu)/(now-sample.at)/std::max(1u,std::thread::hardware_concurrency())),0.f,100.f);
        sample.cpu=cpu;
        long pages=0,resident=0; if(FILE* f=std::fopen("/proc/self/statm","r")){if(std::fscanf(f,"%ld %ld",&pages,&resident)==2)sample.ram_mib=resident*double(sysconf(_SC_PAGESIZE))/(1024*1024);std::fclose(f);}
        if(FILE* f=std::fopen("/sys/class/kgsl/kgsl-3d0/gpubusy","r")){
            long long busy=0,total=0;
            if(std::fscanf(f,"%lld %lld",&busy,&total)==2 && total>0){
                sample.gpu=std::clamp(int(100.0*double(busy)/total),0,100);
                sample.gpu_busy=busy;sample.gpu_total=total;
            }
            std::fclose(f);
        }
#endif
        sample.at=now;
    }
    const auto* viewport=ImGui::GetMainViewport();
    const float pad=14.0f*base_scale;const int pos=settings.hud_position;
    const bool right=pos==1||pos==3,bottom=pos>=2;
    const float gutter=pad,top=pad;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+(right?viewport->WorkSize.x-gutter:gutter),viewport->WorkPos.y+(bottom?viewport->WorkSize.y-pad:top)),ImGuiCond_Always,ImVec2(right?1.f:0.f,bottom?1.f:0.f));
    ImGui::SetNextWindowBgAlpha(.52f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12*base_scale,7*base_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6*base_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("##uncharted-hud",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr,21*base_scale*settings.hud_scale.load());
    // Corner-anchored HUD; controls reserve this rectangle instead of pushing the HUD down.
    if(settings.frame_gen==BbSettings::FrameGenLsfg) {
        ImGui::TextColored(ImVec4(.55f,1.f,.14f,1),"FPS %.0f | %dx %.0f",presented_fps.real_fps,
            settings.frame_gen_multiplier.load(),presented_fps.output_fps);
    } else {
        ImGui::TextColored(ImVec4(.55f,1.f,.14f,1),"FPS %.0f",presented_fps.real_fps);
    }
    if(settings.hud_mode>1){ImGui::SameLine();ImGui::Text(" · %.1f ms",frame_ms_avg);}
    if(settings.hud_mode>0){
        if(sample.gpu>=0)ImGui::TextColored(ImVec4(.88f,.34f,1,1),"GPU %d%%",sample.gpu);
        else ImGui::TextColored(ImVec4(.88f,.34f,1,1),"GPU --");
        ImGui::SameLine();ImGui::TextDisabled("·");ImGui::SameLine();
        ImGui::TextColored(ImVec4(.12f,.86f,1,1),"CPU %.0f%%",sample.cpu_percent);
        if(settings.upscaler!=BbSettings::UpscalerOff) {
            // Report the applied startup mode, not a pending launcher selection or
            // the game's native display allocation. Every temporal preset outputs 720p.
            const char* res=std::getenv("BB_RENDER_RES");
            const char* mode=res && std::strcmp(res,"960x540")==0 ? "Quality"
                :res && std::strcmp(res,"832x468")==0 ? "Balanced"
                :res && std::strcmp(res,"640x360")==0 ? "Performance" : "Native AA";
            if(settings.upscaler==BbSettings::UpscalerFsr3)ImGui::Text("FSR %s",BbLocale::Text(mode));
            else ImGui::Text("TAA");
        }
        if(settings.hud_mode>1) {
            if(settings.upscaler!=BbSettings::UpscalerOff)ImGui::SameLine();
            ImGui::Text(" · RAM %.2f GB",sample.ram_mib/1024.f);
        }
    }

#ifdef __ANDROID__
    const auto hud_pos=ImGui::GetWindowPos(),hud_size=ImGui::GetWindowSize();
    TouchPadReserveHud(hud_pos.x,hud_pos.y,hud_pos.x+hud_size.x,hud_pos.y+hud_size.y);
#endif
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace

// g2-arm64: the memory log's view of the device (set once the presenter has one).
static std::atomic<const Vulkan::Instance*> memory_log_instance{nullptr};

void Init(const Vulkan::Instance& instance, vk::Format format, u32 image_count) {
    memory_log_instance = &instance;
    std::scoped_lock lock{imgui_mutex};
    if (initialized) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // window positions are not kept
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "bbport";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.WindowPadding = ImVec2(18, 16);
    style.FramePadding = ImVec2(12, 10);
    style.ItemSpacing = ImVec2(10, 12);
    style.ScrollbarSize = 22;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.065f, 0.09f, 0.07f, 0.97f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.96f, 0.94f, 0.87f, 1);
    style.Colors[ImGuiCol_Border] = ImVec4(0.40f, 0.35f, 0.24f, 0.8f);
    style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.20f, 0.15f, 1);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.18f, 0.14f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.34f, 0.29f, 0.18f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.48f, 0.39f, 0.23f, 1);
    style.Colors[ImGuiCol_ButtonActive] = style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.48f, 0.27f, 1);
    style.Colors[ImGuiCol_CheckMark] = style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.66f, 0.42f, 1);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.21f, 0.16f, 1);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(0.40f, 0.33f, 0.19f, 1);

    ImFont* app_font=nullptr;
#ifdef __ANDROID__
    // Match the Android launcher typography instead of a desktop diagnostic font.
    for(const char* path:{"/system/fonts/Roboto-Regular.ttf","/system/fonts/NotoSans-Regular.ttf"}) {
        if(FILE* f=std::fopen(path,"rb")){std::fclose(f);app_font=io.Fonts->AddFontFromFileTTF(path,18.0f);if(app_font)break;}
    }
#endif
    if(!app_font){
        ImFontConfig font_config;font_config.FontDataOwnedByAtlas=false;
        io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bb_font_ttf),int(bb_font_ttf_end-bb_font_ttf),18.0f,&font_config);
    }

#ifdef __ANDROID__
    // Dynamic font atlas: merge Android's script fallbacks for all offered UI locales.
    for(const char* path:{"/system/fonts/NotoSansCJK-Regular.ttc","/system/fonts/NotoSansJP-Regular.otf","/system/fonts/NotoSansKR-Regular.otf","/system/fonts/NotoSansSC-Regular.otf"}) {
        if(FILE* f=std::fopen(path,"rb")){std::fclose(f);ImFontConfig config;config.MergeMode=true;io.Fonts->AddFontFromFileTTF(path,18.0f,&config);}
    }
#endif
    const vk::Instance vk_instance = instance.GetInstance();
    ImGui_ImplVulkan_LoadFunctions(
        instance.ApiVersion(),
        [](const char* name, void* user) {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                *static_cast<const vk::Instance*>(user), name);
        },
        const_cast<vk::Instance*>(&vk_instance));

    const VkFormat color_format = static_cast<VkFormat>(format);
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = instance.ApiVersion();
    info.Instance = vk_instance;
    info.PhysicalDevice = instance.GetPhysicalDevice();
    info.Device = instance.GetDevice();
    info.QueueFamily = instance.GetGraphicsQueueFamilyIndex();
    info.Queue = instance.GetGraphicsQueue();
    info.DescriptorPoolSize = 16;
    info.MinImageCount = std::max(image_count, 2u);
    info.ImageCount = std::max(image_count, 2u);
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
    };
    if (!ImGui_ImplVulkan_Init(&info)) {
        std::printf("Overlay: ImGui Vulkan backend init failed\n");
        ImGui::DestroyContext();
        return;
    }
    initialized = true;
    std::printf("Overlay: menu ready (Insert or L3+R3)\n");
}

void UpdateTextInput(SDL_Window* window) {
    bool want = false;
    {
        std::scoped_lock lock{imgui_mutex};
        want = initialized && menu_open && ImGui::GetIO().WantTextInput;
    }
    if (want != SDL_TextInputActive(window)) {
        if (want) {
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
        }
    }
}

bool HandleEvent(const SDL_Event& event) {
    std::scoped_lock lock{imgui_mutex};
    if (!initialized) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    const bool is_open = menu_open;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const bool down = event.type == SDL_EVENT_KEY_DOWN;
        if (down && !event.key.repeat &&
            (event.key.key == SDLK_INSERT || (is_open && event.key.key == SDLK_ESCAPE))) {
            SetOpen(event.key.key == SDLK_INSERT ? !is_open : false);
            return true;
        }
        if (!is_open) {
            return false;
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, (event.key.mod & SDL_KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (event.key.mod & SDL_KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (event.key.mod & SDL_KMOD_ALT) != 0);
        if (const ImGuiKey key = KeyFromSdl(event.key.key); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
            l3_down = down;
        } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
        }
        if (down && l3_down && r3_down) {
            SetOpen(!is_open);
            return true;
        }
        if (!is_open) {
            return false;
        }
#ifdef __ANDROID__
        // g2-arm64: the touch menu has no popups, so B (Circle) closes it.
        if (down && button == SDL_GAMEPAD_BUTTON_EAST) {
            SetOpen(false);
            return true;
        }
#endif
        if (const ImGuiKey key = KeyFromGamepad(button); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_TEXT_INPUT: {
        // Typed characters (Ctrl+click on a slider, a text field): key events alone erase but
        // do not type. SDL sends them while text input is on (UpdateTextInput).
        if (!is_open) {
            return false;
        }
        io.AddInputCharactersUTF8(event.text.text);
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (!is_open) {
            return false;
        }
        const float density = PixelDensity(event.motion.windowID);
        io.AddMousePosEvent(event.motion.x * density, event.motion.y * density);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!is_open) {
            return false;
        }
        const int button = event.button.button == SDL_BUTTON_LEFT    ? 0
                           : event.button.button == SDL_BUTTON_RIGHT  ? 1
                           : event.button.button == SDL_BUTTON_MIDDLE ? 2
                                                                      : -1;
        if (button >= 0) {
            io.AddMouseButtonEvent(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        }
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (!is_open) {
            return false;
        }
        io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
        return true;
#ifdef __ANDROID__
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        // g2-arm64: a swipe in from the right edge opens the menu, a tap outside closes it. The
        // mouse events SDL makes from touches work the panel itself; the game gets neither
        // while the menu is open.
        switch (AndroidTouch(event, is_open)) {
        case TouchAction::Open:
            SetOpen(true);
            return true;
        case TouchAction::Close:
            SetOpen(false);
            return true;
        case TouchAction::None:
            break;
        }
        if (!is_open && TouchPadActive()) {
            TouchPadEvent(event); // g2-arm64: the on-screen gamepad takes fingers off the edge
        }
        return is_open;
#endif
    default:
        return false;
    }
}

bool Visible() {
#ifdef __ANDROID__
    return initialized; // the menu button remains available even with touch controls and HUD hidden
#else
    return initialized && (menu_open || text_entry_active || BbSettings::Get().show_fps);
#endif
}

bool CapturesInput() {
    return menu_open || text_entry_active;
}

void SetTextEntry(bool active, const std::string& prompt, const std::string& text) {
    std::scoped_lock lock{imgui_mutex};
    text_entry_active = active;
    text_entry_prompt = prompt;
    text_entry_text = text;
}

// Called for every presented frame: the interval for the FPS readout and BB_FPS_LOG.
// g2-arm64: with BB_FPS_LOG, every 30 s: the process's mappings (Android's vm.max_map_count is 65530;
// past it mmap and mprotect fail - after 17 min of U2 a malloc returned NULL in
// sceAjmBatchStartBuffer while the GPU library threw bad_alloc), its RSS and swap, and the system's
// available memory. From its own thread: reading /proc/self/maps walks every mapping.
static long ProcField(const char* path, const char* name) {
    long value = -1;
    if (FILE* f = std::fopen(path, "r")) {
        char line[256];
        const size_t len = std::strlen(name);
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, name, len) == 0) {
                value = std::strtol(line + len, nullptr, 10);
                break;
            }
        }
        std::fclose(f);
    }
    return value;
}

// BB_MEM_SMAPS=1: RSS+swap and the number of mappings by kind, from /proc/self/smaps (it walks every
// page table: tens of milliseconds with the address space lock held - diagnostic runs only).
static void PrintMemoryMap() {
    FILE* f = std::fopen("/proc/self/smaps", "r");
    if (!f) {
        return;
    }
    enum { Guest, Jit, Scudo, Kgsl, Anon, Files, Kinds };
    static const char* const names[Kinds] = {"guest", "FEX JIT", "scudo", "kgsl", "other anon", "files"};
    unsigned long long kb[Kinds] = {}, count[Kinds] = {};
    int kind = Anon;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long long lo, hi;
        char perms[8];
        if (std::sscanf(line, "%llx-%llx %7s", &lo, &hi, perms) == 3 && std::strchr(line, '-') < std::strchr(line, ' ')) {
            const char* path = line;
            for (int field = 0; *path && field < 5; ++field) {
                while (*path && *path != ' ') ++path;
                while (*path == ' ') ++path;
            }
            // Guest memory: 64 GiB (USER_MIN) up to 256 GiB; Android maps its own at 448-512 GiB.
            if (lo >= 0x1000000000ull && lo < 0x4000000000ull) {
                kind = Guest;
            } else if (!*path || *path == '\n') {
                kind = Anon;
            } else if (std::strstr(path, "FEXMem")) {
                kind = Jit;
            } else if (std::strstr(path, "scudo")) {
                kind = Scudo;
            } else if (std::strstr(path, "kgsl")) {
                kind = Kgsl;
            } else if (*path == '[') {
                kind = Anon;
            } else {
                kind = Files;
            }
            ++count[kind];
            continue;
        }
        unsigned long long v;
        if (std::sscanf(line, "Rss: %llu", &v) == 1 || std::sscanf(line, "Swap: %llu", &v) == 1) {
            kb[kind] += v;
        }
    }
    std::fclose(f);
    char out[512];
    int n = std::snprintf(out, sizeof(out), "Memory map (RSS+swap, mappings):");
    for (int k = 0; k < Kinds; ++k) {
        n += std::snprintf(out + n, sizeof(out) - n, " %s %llu MiB/%llu%s", names[k], kb[k] >> 10,
                           count[k], k + 1 < Kinds ? "," : "\n");
    }
    std::fputs(out, stdout);
}

static void StartMemoryLog() {
    std::thread([] {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), "BbMemLog");
#endif
        std::vector<char> buf(1 << 20);
        const auto count_maps = [&buf] {
            size_t maps = 0;
            if (FILE* f = std::fopen("/proc/self/maps", "r")) {
                size_t n;
                while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
                    maps += std::count(buf.data(), buf.data() + n, '\n');
                }
                std::fclose(f);
            }
            return maps;
        };
        for (;;) {
            size_t maps = 0;
            for (int tick = 0; tick < 6; ++tick) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                maps = count_maps();
                if (maps > 16000) {
                    std::printf("Memory alarm: %zu mappings\n", maps);
                    PrintMemoryMap();
                }
            }
            static const bool smaps = [] {
                const char* env = std::getenv("BB_MEM_SMAPS");
                return env && env[0] == '1';
            }();
            static unsigned round = 0;
            if (smaps && (++round & 1) == 0) {
                PrintMemoryMap();
            }
            const long rss = ProcField("/proc/self/status", "VmRSS:");
            const long swap = ProcField("/proc/self/status", "VmSwap:");
            const long avail = ProcField("/proc/meminfo", "MemAvailable:");
            // GPU side: the driver's total, all VMA blocks (images, buffers, staging), the texture
            // cache's images among them, and the arenas (own allocations); the rest is the driver's.
            unsigned long long device = 0, vma = 0, vma_used = 0;
            if (const auto* instance = memory_log_instance.load()) {
                device = instance->CanReportMemoryUsage() ? instance->GetDeviceMemoryUsage() : 0;
                VmaTotalStatistics stats{};
                vmaCalculateStatistics(instance->GetAllocator(), &stats);
                vma = stats.total.statistics.blockBytes;
                vma_used = stats.total.statistics.allocationBytes;
                // BB_VMA_DUMP=<file>: every VMA allocation (size, usage, memory type) once, ~2 min in.
                static int rounds = 0;
                static const char* dump = std::getenv("BB_VMA_DUMP");
                if (dump && ++rounds == 4) {
                    char* json = nullptr;
                    vmaBuildStatsString(instance->GetAllocator(), &json, VK_TRUE);
                    if (FILE* f = std::fopen(dump, "w")) {
                        std::fputs(json, f);
                        std::fclose(f);
                    }
                    vmaFreeStatsString(instance->GetAllocator(), json);
                }
            }
            // g2-arm64: the page tracker's host protection changes a second (each an mprotect
            // with TLB shootdowns on every core running the game's threads).
            static size_t last_protects = 0;
            static auto last_protects_time = std::chrono::steady_clock::now();
            const size_t protects = runtime_memory_protect_count();
            const auto now_protects = std::chrono::steady_clock::now();
            const double protect_secs =
                std::chrono::duration<double>(now_protects - last_protects_time).count();
            const double protects_per_s =
                protect_secs > 1.0 ? double(protects - last_protects) / protect_secs : 0.0; // the first report: ~0 s
            last_protects = protects;
            last_protects_time = now_protects;
            std::printf("Memory: %zu mappings, RSS %ld MiB, swap %ld MiB, system available %ld MiB; "
                        "GPU %llu MiB: VMA %llu (used %llu; images %lld in %lld), buffer arenas %llu "
                        "in %llu allocations; host protects %.0f/s\n",
                        maps, rss / 1024, swap / 1024, avail / 1024, device >> 20, vma >> 20,
                        vma_used >> 20,
                        (long long)(BbStats::image_bytes.load() >> 20),
                        (long long)BbStats::image_count.load(),
                        (unsigned long long)(BbStats::arena_bytes.load() >> 20),
                        (unsigned long long)BbStats::arena_allocations.load(), protects_per_s);
        }
    }).detach();
}

void NotePresentedFrame(bool original) {
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    presented_fps.Presented(now,original);
}

void NotePresent() {
    const auto now = std::chrono::steady_clock::now();
    const float ms = std::chrono::duration<float, std::milli>(now - last_present).count();
    last_present = now;
    last_present_ms = ms;
    if (ms > 0.0f && ms < 1000.0f) {
        frame_ms_avg = frame_ms_avg == 0.0f ? ms : frame_ms_avg * 0.95f + ms * 0.05f;
    }
    // BB_FPS_LOG=1: presents per second and the slowest frame, every 5 seconds (test runs).
    static const bool fps_log = [] {
        const char* env = std::getenv("BB_FPS_LOG");
        const bool on = env && env[0] == '1';
        if (on) {
            StartMemoryLog();
        }
        return on;
    }();
    if (fps_log) {
        static auto window_start = now;
        static int frames = 0;
        static float worst = 0.0f;
        ++frames;
        worst = std::max(worst, ms < 1000.0f ? ms : 0.0f);
        const float elapsed = std::chrono::duration<float>(now - window_start).count();
        if (elapsed >= 5.0f) {
            std::printf("Frame rate: %.1f FPS, slowest frame %.1f ms\n", frames / elapsed, worst);
            window_start = now;
            frames = 0;
            worst = 0.0f;
        }
    }
}

// g2-arm64: turns ImGui's draw data (laid out w x h) clockwise by quarter turns, for an image
// in the display panel's orientation (Android pre-rotation).
static void TurnDrawData(ImDrawData* data, unsigned turns, float w, float h) {
    const auto turn = [&](ImVec2 p) {
        switch (turns) {
        case 1:
            return ImVec2(h - p.y, p.x);
        case 2:
            return ImVec2(w - p.x, h - p.y);
        default:
            return ImVec2(p.y, w - p.x);
        }
    };
    for (ImDrawList* list : data->CmdLists) {
        for (ImDrawVert& vertex : list->VtxBuffer) {
            vertex.pos = turn(vertex.pos);
        }
        for (ImDrawCmd& cmd : list->CmdBuffer) {
            const ImVec2 a = turn(ImVec2(cmd.ClipRect.x, cmd.ClipRect.y));
            const ImVec2 b = turn(ImVec2(cmd.ClipRect.z, cmd.ClipRect.w));
            cmd.ClipRect = ImVec4(std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
                                  std::max(a.y, b.y));
        }
    }
    data->DisplayPos = ImVec2(0.0f, 0.0f);
    data->DisplaySize = turns & 1 ? ImVec2(h, w) : ImVec2(w, h);
}

void Render(vk::CommandBuffer cmdbuf, vk::ImageView view, vk::Extent2D extent,
            unsigned quarter_turns) {
    const float ms = last_present_ms;
    if (!Visible()) {
        return;
    }
    std::scoped_lock lock{imgui_mutex};
    ImGuiIO& io = ImGui::GetIO();
    const vk::Extent2D logical =
        quarter_turns & 1 ? vk::Extent2D{extent.height, extent.width} : extent;
    io.DisplaySize = ImVec2(float(logical.width), float(logical.height));
    io.DeltaTime = ms > 0.0f && ms < 1000.0f ? ms / 1000.0f : 1.0f / 60.0f;
    // UI scale follows the display height (1080p = 1).
    const float scale = std::max(float(logical.height) / 1080.0f, 0.75f);
    if (std::abs(scale - base_scale) > 0.01f) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale / base_scale);
        style.FontScaleMain = scale;
        base_scale = scale;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    if (menu_open) {
#ifdef __ANDROID__
        bool keep_open = true; // g2-arm64: the touch panel instead of the PC window
        AndroidMenu(base_scale, frame_ms_avg, keep_open);
        if (!keep_open) {
            SetOpen(false);
        }
#else
        Menu();
#endif
    }
#ifdef __ANDROID__
    TouchPadReserveHud(0,0,0,0);
#endif
    if (BbSettings::Get().show_fps && !menu_open) {
        FpsCounter();
    }
#ifdef __ANDROID__
    if (!menu_open) {
        TouchPadFrame(io.DisplaySize.x, io.DisplaySize.y); // g2-arm64: the on-screen gamepad
        AndroidMenuButton(base_scale);
    }
#endif
    if (text_entry_active) {
        TextEntryBox();
    }
    ImGui::Render();
    if (quarter_turns) {
        TurnDrawData(ImGui::GetDrawData(), quarter_turns, float(logical.width),
                     float(logical.height));
    }

    const vk::RenderingAttachmentInfo attachment{
        .imageView = view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    });
    {
        // Font atlas uploads submit to the graphics queue themselves.
        std::scoped_lock submit_lock{Vulkan::Scheduler::submit_mutex};
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
    }
    cmdbuf.endRendering();
}

} // namespace BbOverlay
