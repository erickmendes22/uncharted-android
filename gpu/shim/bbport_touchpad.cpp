// SPDX-License-Identifier: GPL-2.0-or-later
// g2-arm64: the on-screen gamepad (bbport_touchpad.h). Layout in units of the display height
// (1080 at 1080p), anchored to the screen's edges so phones of any aspect ratio get the same
// controls: the face buttons bottom right, L1/L2 and R1/R2 in the top corners, the D-pad on the
// left, L3/R3 at the bottom centre, the touch pad button and Options at the top. A finger that
// lands on no control drives the left stick on the left half (the stick appears under it) or
// turns the camera on the right half (its movement becomes right stick deflection for that
// frame, as mobile shooters do). Fingers are followed by id, so several work at once.
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>

#include "imgui.h"

#include "bbport_settings.h"
#include "bbport_touchpad.h"
#include "bbport_touch_layout.h"

extern "C" void runtime_touch_pad(uint32_t buttons, const int sticks[4], uint8_t l2, uint8_t r2);
extern "C" int runtime_gamepad_connected(void);

namespace BbOverlay {

namespace {

// OrbisPadData button bits (src/runtime_pad.c).
enum : uint32_t {
    BtnL3 = 0x2, BtnR3 = 0x4, BtnOptions = 0x8, BtnUp = 0x10, BtnRight = 0x20, BtnDown = 0x40,
    BtnLeft = 0x80, BtnL2 = 0x100, BtnR2 = 0x200, BtnL1 = 0x400, BtnR1 = 0x800,
    BtnTriangle = 0x1000, BtnCircle = 0x2000, BtnCross = 0x4000, BtnSquare = 0x8000,
    BtnTouchpad = 0x100000,
};

/// PlayStation labels and button bits; all geometry lives in bbport_touch_layout.h.
enum class Glyph { Text, Cross, Circle, Square, Triangle };
struct Control {const char* label;uint32_t bit;Glyph glyph;};
constexpr Control Controls[]={
    {"",BtnTriangle,Glyph::Triangle},{"",BtnCircle,Glyph::Circle},
    {"",BtnCross,Glyph::Cross},{"",BtnSquare,Glyph::Square},
    {"L2",BtnL2,Glyph::Text},{"L1",BtnL1,Glyph::Text},
    {"R2",BtnR2,Glyph::Text},{"R1",BtnR1,Glyph::Text},
    {"PAD",BtnTouchpad,Glyph::Text},{"II",BtnOptions,Glyph::Text},
    {"L3",BtnL3,Glyph::Text},{"R3",BtnR3,Glyph::Text},
};
constexpr int ControlCount=int(sizeof(Controls)/sizeof(Controls[0]));

// The D-pad: a plus on the left, above the stick's usual place.
// The shared geometry solver expands spacing with button size and reserves the HUD.
constexpr float StickRadius = 0.082f, KnobRadius = 0.041f;
// The camera drag: full right stick deflection at this finger speed (display heights per second)
// times the sensitivity setting; slower drags turn proportionally slower. Independent of the
// frame rate (the deflection is the finger's speed over the time since the last frame).
constexpr float LookFullSpeed = 0.5f;
constexpr float EdgeZone = 0.03f;   // the menu's swipe-in strip (AndroidTouch)
constexpr double MinHold = 0.09;    // seconds a tapped button stays pressed (a tap can be shorter than a frame)

enum class Kind { None, Button, Dpad, Stick, Look };

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Finger {
    SDL_FingerID id = 0;
    bool down = false;
    bool lifted = false;            // released before MinHold: still pressed until then
    double down_time = 0.0;
    Kind kind = Kind::None;
    int control = -1;               // Controls index for Button, 12..15 for Dpad
    float x0 = 0, y0 = 0;           // where it landed (normalized)
    float x = 0, y = 0;             // where it is
    float look_dx = 0, look_dy = 0; // normalized travel since the last frame (Kind::Look)
    float look_vx = 0, look_vy = 0; // the smoothed deflection (Kind::Look)
};

std::mutex fingers_mutex;
Finger fingers[10];
bool was_active = false;
double last_push = 0.0;

float Width = 1920.0f, Height = 1080.0f;
float InputWidth = 1920.0f, InputHeight = 1080.0f;
unsigned OverlayTurns = 0;

BbTouchLayout::Layout layout;
BbTouchLayout::Rect hud_bounds;
ImVec2 Centre(const Control& c) {
    const auto& slot=layout.slots[&c-Controls];return {slot.x,slot.y};
}
bool Hits(const Control& c,float px,float py,float) {
    const auto& slot=layout.slots[&c-Controls];
    const float dx=px-slot.x,dy=py-slot.y;
    if(slot.circle)return dx*dx+dy*dy<=slot.w*slot.w*.25f;
    return std::abs(dx)<=slot.w*.5f && std::abs(dy)<=slot.h*.5f;
}
uint32_t DpadBits(float px,float py,float) {
    const uint32_t bits[]={BtnUp,BtnDown,BtnLeft,BtnRight};
    for(int i=0;i<4;++i){const auto& slot=layout.slots[12+i];float dx=px-slot.x,dy=py-slot.y;
        if(dx*dx+dy*dy<=slot.w*slot.w*.25f)return bits[i];}
    return 0;
}

// The rendered circle/rectangle is the primary hit target. On a phone a
// finger can land just beyond its border; never turn that into a camera/left
// stick swipe or choose an arbitrary first button. Pick the nearest single
// PlayStation control within a small, non-overlapping tolerance.
int NearControl(float px, float py) {
    const float allowance = 0.020f * Height;
    float best = 1.0e30f;
    int chosen = -1;
    for (int i = 0; i < 16; ++i) {
        const auto& slot = layout.slots[i];
        const float dx = std::abs(px - slot.x), dy = std::abs(py - slot.y);
        const float outside = slot.circle
            ? std::max(0.0f, std::sqrt(dx * dx + dy * dy) - slot.w * 0.5f)
            : std::hypot(std::max(0.0f, dx - slot.w * 0.5f),
                         std::max(0.0f, dy - slot.h * 0.5f));
        if (outside > std::min(allowance, slot.w * 0.22f)) continue;
        const float distance = dx * dx + dy * dy;
        if (distance < best) { best = distance; chosen = i; }
    }
    return chosen;
}

Finger* Find(SDL_FingerID id) {
    for (auto& f : fingers) {
        if (f.down && !f.lifted && f.id == id) return &f;
    }
    return nullptr;
}

Finger* Free() {
    for (auto& f : fingers) {
        if (!f.down) return &f;
    }
    return nullptr;
}

/// The pad state of this frame to the runtime (fingers_mutex held).
void Push(bool neutral) {
    uint32_t buttons = 0;
    int sticks[4] = {-1, -1, -1, -1};
    uint8_t l2 = 0, r2 = 0;
    const double now = Now();
    const float dt = float(std::clamp(now - last_push, 0.004, 0.1));
    last_push = now;
    if (!neutral) {
        const float scale = layout.scale;
        const float sensitivity = std::clamp(BbSettings::Get().touch_pad_look.load(), 0.25f, 4.0f);
        for (auto& f : fingers) {
            if (f.down && f.lifted && now - f.down_time >= MinHold) {
                f.down = f.lifted = false; // a short tap held long enough
            }
            if (!f.down) continue;
            switch (f.kind) {
            case Kind::Button:
                buttons |= Controls[f.control].bit;
                if (Controls[f.control].bit == BtnL2) l2 = 255;
                if (Controls[f.control].bit == BtnR2) r2 = 255;
                break;
            case Kind::Dpad: {
                // Lock the directional button at FINGER_DOWN. A small finger
                // movement must never turn an Up press into Right/Down.
                constexpr uint32_t bits[] = {BtnUp, BtnDown, BtnLeft, BtnRight};
                if (f.control >= 12 && f.control < 16)
                    buttons |= bits[f.control - 12];
                break;
            }
            case Kind::Stick: {
                const float r = StickRadius * Height * scale;
                float dx = (f.x - f.x0) * Width / r, dy = (f.y - f.y0) * Height / r;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1.0f) { dx /= len; dy /= len; }
                sticks[0] = int(std::lround(128.0f + dx * 127.0f));
                sticks[1] = int(std::lround(128.0f + dy * 127.0f));
                break;
            }
            case Kind::Look: {
                // the finger's speed since the last frame: the camera turns while it moves
                const float full = LookFullSpeed * Height / sensitivity; // pixels per second
                // finger events and frames tick at different rates: smooth over ~2 frames
                f.look_vx = 0.5f * f.look_vx + 0.5f * (f.look_dx * Width / dt / full);
                f.look_vy = 0.5f * f.look_vy + 0.5f * (f.look_dy * Height / dt / full);
                float dx = std::clamp(f.look_vx, -1.0f, 1.0f);
                float dy = std::clamp(f.look_vy, -1.0f, 1.0f);
                if(!BbSettings::Get().touch_mobile) {
                    const float radius=StickRadius*Height*scale/sensitivity;
                    dx=(f.x-f.x0)*Width/radius;dy=(f.y-f.y0)*Height/radius;
                    const float length=std::sqrt(dx*dx+dy*dy);
                    if(length>1){dx/=length;dy/=length;}
                }
                f.look_dx = f.look_dy = 0.0f;
                sticks[2] = int(std::lround(128.0f + dx * 127.0f));
                sticks[3] = int(std::lround(128.0f + dy * 127.0f));
                break;
            }
            case Kind::None:
                break;
            }
        }
        for (int& s : sticks) {
            if (s >= 0) s = std::clamp(s, 0, 255);
        }
    }
    runtime_touch_pad(buttons, sticks, l2, r2);
}

void DrawGlyph(ImDrawList* dl, Glyph glyph, ImVec2 c, float r, ImU32 colour, float thick) {
    switch (glyph) {
    case Glyph::Cross:
        dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), colour, thick);
        dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), colour, thick);
        break;
    case Glyph::Circle:
        dl->AddCircle(c, r, colour, 32, thick);
        break;
    case Glyph::Square:
        dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), colour, 0.0f, 0, thick);
        break;
    case Glyph::Triangle:
        dl->AddTriangle(ImVec2(c.x, c.y - r * 1.1f), ImVec2(c.x + r * 1.05f, c.y + r * 0.8f),
                        ImVec2(c.x - r * 1.05f, c.y + r * 0.8f), colour, thick);
        break;
    case Glyph::Text:
        break;
    }
}

void Draw(float alpha, float scale) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float H = Height;
    const auto col = [alpha](int a) { return IM_COL32(232, 229, 216, int(alpha * float(a))); };
    const auto fill = [alpha](bool on) { return on ? IM_COL32(187,145,68,int(alpha*170)) : IM_COL32(8,15,13,int(alpha*125)); };
    const float text_size = 0.033f * H * scale;
    const auto shadow = [alpha](int a) { return IM_COL32(0, 0, 0, int(alpha * float(a))); };
    const auto label = [&](const char* text, ImVec2 at, float size) {
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        const ImVec2 pos(at.x - ts.x * 0.5f, at.y - ts.y * 0.5f);
        const float o = std::max(1.0f, size * 0.05f); // a dark shadow: white text over a bright scene
        dl->AddText(font, size, ImVec2(pos.x + o, pos.y + o), shadow(200), text);
        dl->AddText(font, size, pos, col(250), text);
        dl->AddText(font, size, ImVec2(pos.x+.45f*scale,pos.y), col(250), text);
    };
    uint32_t held = 0;
    {
        std::scoped_lock lock{fingers_mutex};
        for (const auto& f : fingers) {
            if (!f.down) continue;
            if (f.kind == Kind::Button) held |= Controls[f.control].bit;
            if (f.kind == Kind::Dpad && f.control >= 12 && f.control < 16) {
                constexpr uint32_t bits[] = {BtnUp, BtnDown, BtnLeft, BtnRight};
                held |= bits[f.control - 12];
            }

        }
    }
    const float line = 3.0f * scale * H/1080.0f;
    for (const auto& c : Controls) {
        const ImVec2 centre = Centre(c);
        const bool on = held & c.bit;
        if (layout.slots[&c-Controls].circle) {
            const float r = layout.slots[&c-Controls].w*.5f;
            const auto tint=[&](int a) {
                const int opacity=int(a*(.45f+.55f*alpha));
                switch(c.glyph){
                case Glyph::Triangle:return IM_COL32(133,211,181,opacity);
                case Glyph::Circle:return IM_COL32(235,128,145,opacity);
                case Glyph::Cross:return IM_COL32(142,168,255,opacity);
                case Glyph::Square:return IM_COL32(210,153,214,opacity);
                default:return col(a);
                }
            };
            dl->AddCircleFilled(centre,r,on?tint(100):IM_COL32(12,17,15,int(160*alpha)),48);
            dl->AddCircle(centre,r,tint(on?255:220),48,line);
            if(c.glyph!=Glyph::Text)DrawGlyph(dl,c.glyph,centre,r*.30f,tint(255),line);

        } else {
            const auto box=layout.slots[&c-Controls].Bounds();
            const ImVec2 a(box.left,box.top);
            const ImVec2 b(box.right,box.bottom);
            const float rounding = layout.slots[&c-Controls].h*.18f;
            dl->AddRectFilled(a,b,on?IM_COL32(151,117,58,int(170*alpha)):IM_COL32(11,17,16,int(200*alpha)),rounding);
            dl->AddRect(a,b,IM_COL32(199,174,121,int((on?250:185)*(.5f+.5f*alpha))),rounding,0,line);
            // Short brass rule echoes an expedition journal without obscuring the game.
            dl->AddLine(ImVec2(a.x+8*scale,b.y-5*scale),ImVec2(a.x+24*scale,b.y-5*scale),col(220),line);
        }
        if (c.glyph == Glyph::Text) {
            label(c.label, centre, layout.slots[&c-Controls].circle ? text_size*.72f : text_size * 0.8f);
        }
    }
    // the D-pad
    {
        const float r=layout.slots[12].w*.5f;
        const struct { uint32_t bit;float x,y; } keys[] = {
            {BtnUp,0,-1},{BtnDown,0,1},{BtnLeft,-1,0},{BtnRight,1,0}};
        int key_index=12;
        for(const auto& key:keys) {
            const bool on=held&key.bit;
            const auto& slot=layout.slots[key_index++];
            const ImVec2 centre(slot.x,slot.y);
            dl->AddCircleFilled(centre,r,fill(on),48);
            dl->AddCircle(centre,r,col(on?255:210),48,line);
            const float size=r*.24f;
            ImVec2 tip,b1,b2;
            if(key.bit==BtnUp){tip={centre.x,centre.y-size};b1={centre.x-size,centre.y+size};b2={centre.x+size,centre.y+size};}
            else if(key.bit==BtnDown){tip={centre.x,centre.y+size};b1={centre.x-size,centre.y-size};b2={centre.x+size,centre.y-size};}
            else if(key.bit==BtnLeft){tip={centre.x-size,centre.y};b1={centre.x+size,centre.y-size};b2={centre.x+size,centre.y+size};}
            else {tip={centre.x+size,centre.y};b1={centre.x-size,centre.y-size};b2={centre.x-size,centre.y+size};}
            dl->AddTriangleFilled(tip,b1,b2,col(245));
        }
    }

    // Twin analogs like the original console layout. No game-specific action names:
    // all three games and their menu prompts keep their real PlayStation mappings.
    for(int side=0;side<2;++side) {
        const float radius=StickRadius*H*scale, knob=KnobRadius*H*scale;
        ImVec2 centre(layout.slots[16+side].x,layout.slots[16+side].y), point=centre;
        bool down=false;
        {
            std::scoped_lock lock{fingers_mutex};
            for(const auto& f:fingers) {
                if(!f.down || f.kind!=(side?Kind::Look:Kind::Stick))continue;
                down=true;centre=ImVec2(f.x0*Width,f.y0*H);
                float dx=f.x*Width-centre.x,dy=f.y*H-centre.y;
                const float length=std::sqrt(dx*dx+dy*dy);
                if(length>radius){dx*=radius/length;dy*=radius/length;}
                point=ImVec2(centre.x+dx,centre.y+dy);
            }
        }
        dl->AddCircleFilled(centre,radius,IM_COL32(18,23,22,int(180*alpha)),64);
        dl->AddCircle(centre,radius,col(down?200:100),64,line);
        dl->AddCircleFilled(point,knob,fill(down),48);
        dl->AddCircle(point,knob,col(down?230:140),48,line);
        if(!down)label(side?"R":"L",point,text_size*.62f);
    }
}

} // namespace

bool TouchPadActive() {
    const int mode = BbSettings::Get().touch_pad.load();
    return mode == 1 || (mode == 2 && !runtime_gamepad_connected());
}

void TouchPadInputSize(float width, float height) {
    if (width <= 0 || height <= 0) return;
    std::scoped_lock lock{fingers_mutex};
    InputWidth = width;
    InputHeight = height;
}

void TouchPadPresentTurn(unsigned quarter_turns) {
    std::scoped_lock lock{fingers_mutex};
    OverlayTurns = quarter_turns & 3u;
}

void TouchPadTransformFinger(SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: break;
    default: return;
    }
    std::scoped_lock lock{fingers_mutex};
    // SDL3 normalized finger coordinates normally match the logical landscape
    // overlay. Some Android drivers report them in the portrait native-window
    // orientation (e.g. 1216x2688 while Vulkan draws at 2688x1216).
    // Only rotate when the window and overlay aspect ratios are reversed.
    const float input_aspect = InputWidth / InputHeight;
    const float overlay_aspect = Width / Height;
    if (input_aspect < 1.0f && overlay_aspect > 1.0f &&
        std::abs(input_aspect * overlay_aspect - 1.0f) < 0.20f) {
        const float x = event.tfinger.x, y = event.tfinger.y;
        if ((OverlayTurns & 3u) == 1u) {
            event.tfinger.x = y;
            event.tfinger.y = 1.0f - x;
        } else if ((OverlayTurns & 3u) == 3u) {
            event.tfinger.x = 1.0f - y;
            event.tfinger.y = x;
        }
    }
}

bool TouchPadEvent(const SDL_Event& event) {
    // Handle raw SDL touch coordinates here instead of editing the
    // release-0.2.5 protected overlay UI. The menu remains untouched.
    SDL_Event mapped = event;
    TouchPadTransformFinger(mapped);
    std::scoped_lock lock{fingers_mutex};
    const float scale = layout.scale;
    switch (mapped.type) {
    case SDL_EVENT_FINGER_DOWN: {
        const float px = mapped.tfinger.x * Width, py = mapped.tfinger.y * Height;
        int control = -1;
        for (int i = 0; i < ControlCount; ++i) {
            if (Hits(Controls[i], px, py, scale)) {
                control = i;
                break;
            }
        }
        uint32_t dpad = control < 0 ? DpadBits(px, py, scale) : 0;
        if (control < 0 && dpad == 0) {
            const int near = NearControl(px, py);
            if (near >= 0 && near < ControlCount) control = near;
            else if (near >= 12 && near < 16) {
                constexpr uint32_t bits[] = {BtnUp, BtnDown, BtnLeft, BtnRight};
                dpad = bits[near - 12];
            }
        }
        // the menu's swipe-in strip, unless the finger is on a control there
        if (control < 0 && !dpad && mapped.tfinger.x >= 1.0f - EdgeZone) return false;
        // SDL reuses a finger ID for successive taps. Old short taps remain
        // pressed for MinHold and used to coexist with the *new* button;
        // pressing Cross immediately after Circle thus pressed both.
        for (auto& previous : fingers) {
            if (previous.down && previous.id == mapped.tfinger.fingerID) {
                previous = Finger{};
            }
        }
        Finger* f = Free();
        if (!f) return false;
        *f = Finger{};
        f->id = mapped.tfinger.fingerID;
        f->down = true;
        f->down_time = Now();
        f->x = f->x0 = mapped.tfinger.x;
        f->y = f->y0 = mapped.tfinger.y;
        if (control >= 0) {
            f->kind = Kind::Button;
            f->control = control;
        } else if (dpad) {
            f->kind = Kind::Dpad;
            f->control = dpad == BtnUp ? 12 : dpad == BtnDown ? 13 :
                         dpad == BtnLeft ? 14 : 15;
        } else {
            f->kind = mapped.tfinger.x < 0.5f ? Kind::Stick : Kind::Look;
        }
        return true;
    }
    case SDL_EVENT_FINGER_MOTION: {
        Finger* f = Find(mapped.tfinger.fingerID);
        if (!f) return false;
        if (f->kind == Kind::Look) {
            f->look_dx += mapped.tfinger.x - f->x;
            f->look_dy += mapped.tfinger.y - f->y;
        }
        f->x = mapped.tfinger.x;
        f->y = mapped.tfinger.y;
        return true;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        Finger* f = Find(mapped.tfinger.fingerID);
        if (!f) return false;
        if ((f->kind == Kind::Button || f->kind == Kind::Dpad) && Now() - f->down_time < MinHold &&
            mapped.type == SDL_EVENT_FINGER_UP) {
            f->lifted = true; // pressed until MinHold (Push)
        } else {
            f->down = false;
        }
        return true;
    }
    default:
        return false;
    }
}

void TouchPadFrame(float width, float height, unsigned quarter_turns) {
    (void)quarter_turns; // Vulkan presenter publishes orientation separately.
    const bool active = TouchPadActive();
    if (!active) {
        if (was_active) TouchPadReset();
        was_active = false;
        return;
    }
    was_active = true;
    const auto& s = BbSettings::Get();
    const float alpha = std::clamp(s.touch_pad_alpha.load(), 0.1f, 1.0f);
    const auto next_layout = BbTouchLayout::Build(
        width, height, s.touch_pad_scale.load(), hud_bounds);
    // SDL finger events arrive on the window thread. Previously the renderer
    // updated the dimensions and 19-slot layout without the mutex held by the
    // hit-test thread. A half-updated layout could hit a different PS button,
    // particularly after orientation, fullscreen or HUD changes.
    {
        std::scoped_lock lock{fingers_mutex};
        Width = width;
        Height = height;
        layout = next_layout;
        Push(false);
    }
    Draw(alpha, next_layout.scale);
}

BbTouchLayout::Rect TouchPadMenuRect(float width,float height) {
    return BbTouchLayout::Build(width,height,BbSettings::Get().touch_pad_scale.load(),hud_bounds).slots[18].Bounds();
}

void TouchPadReserveHud(float left,float top,float right,float bottom) {
    hud_bounds={left,top,right,bottom};
}

void TouchPadReset() {
    std::scoped_lock lock{fingers_mutex};
    for (auto& f : fingers) f.down = f.lifted = false;
    Push(true);
}

} // namespace BbOverlay
