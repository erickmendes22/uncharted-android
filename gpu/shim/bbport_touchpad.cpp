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

enum class Shape { Circle, Pill };
/// What is drawn inside: a text label, or a PlayStation glyph made of lines (fonts lack them).
enum class Glyph { Text, Cross, Circle, Square, Triangle };

/// A fixed control: its centre from an edge (ax 0 = the left edge, 1 = the right, 0.5 the
/// middle; ay the same for the top and bottom), its size, all in units of the display height.
struct Control {
    const char* label;
    uint32_t bit;
    float ax, ox, ay, oy; // centre = (ax * W + ox * H, ay * H + oy * H)
    float w, h;           // a pill's extent; a circle's diameter in w
    Shape shape;
    Glyph glyph;
};

constexpr Control ClassicControls[] = {
    {"", BtnTriangle,1,-.24f,0,.54f,.108f,.108f,Shape::Circle,Glyph::Triangle},
    {"", BtnCircle,1,-.135f,0,.645f,.108f,.108f,Shape::Circle,Glyph::Circle},
    {"", BtnCross,1,-.24f,0,.75f,.108f,.108f,Shape::Circle,Glyph::Cross},
    {"", BtnSquare,1,-.345f,0,.645f,.108f,.108f,Shape::Circle,Glyph::Square},
    {"L2",BtnL2,0,.17f,0,.175f,.23f,.077f,Shape::Pill,Glyph::Text},
    {"L1",BtnL1,0,.17f,0,.075f,.23f,.077f,Shape::Pill,Glyph::Text},
    {"R2",BtnR2,1,-.17f,0,.175f,.23f,.077f,Shape::Pill,Glyph::Text},
    {"R1",BtnR1,1,-.17f,0,.075f,.23f,.077f,Shape::Pill,Glyph::Text},
    {"PAD",BtnTouchpad,.5f,-.17f,0,.065f,.17f,.066f,Shape::Pill,Glyph::Text},
    {"II",BtnOptions,.5f,0,0,.065f,.085f,.066f,Shape::Pill,Glyph::Text},
    {"L3",BtnL3,0,.17f,0,.275f,.087f,.066f,Shape::Pill,Glyph::Text},
    {"R3",BtnR3,1,-.17f,0,.275f,.087f,.066f,Shape::Pill,Glyph::Text},
};
constexpr int ControlCount = int(sizeof(ClassicControls) / sizeof(ClassicControls[0]));
#define Controls ClassicControls

// The D-pad: a plus on the left, above the stick's usual place.
constexpr float DpadX = 0.22f, DpadY = 0.60f; // centre, in H (x from the left edge)
constexpr float DpadArm = 0.105f, DpadWidth = 0.070f, DpadDead = 0.025f;

// The floating left stick and the camera drag.
constexpr float StickRadius = 0.082f, KnobRadius = 0.041f; // in H
constexpr float StickHintX = 0.40f, StickHintY = 0.82f;  // the idle hint, in H
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
    int control = -1;               // Controls index for Kind::Button
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

ImVec2 Centre(const Control& c) {
    return ImVec2(c.ax * Width + c.ox * Height, c.ay * Height + c.oy * Height);
}

bool Hits(const Control& c, float px, float py, float scale) {
    const ImVec2 centre = Centre(c);
    const float hw = c.w * Height * scale * 0.5f, hh = c.h * Height * scale * 0.5f;
    if (c.shape == Shape::Circle) {
        const float dx = px - centre.x, dy = py - centre.y;
        return dx * dx + dy * dy <= hw * hw * 1.25f; // a little margin for fingers
    }
    return std::abs(px - centre.x) <= hw * 1.15f && std::abs(py - centre.y) <= hh * 1.3f;
}

/// D-pad bits for a point, 0 outside the plus.
uint32_t DpadBits(float px, float py, float scale) {
    const float cx = DpadX * Height, cy = DpadY * Height;
    const float dx = (px - cx) / (Height * scale), dy = (py - cy) / (Height * scale);
    const float ax = std::abs(dx), ay = std::abs(dy);
    if (ax > DpadArm + 0.02f || ay > DpadArm + 0.02f) return 0;
    if (ax > DpadWidth * 0.5f + 0.02f && ay > DpadWidth * 0.5f + 0.02f) {
        // a corner: both arms when the finger is clearly diagonal, nothing far out
        if (ax < DpadArm * 0.9f && ay < DpadArm * 0.9f) {
            return (dx < 0 ? BtnLeft : BtnRight) | (dy < 0 ? BtnUp : BtnDown);
        }
        return 0;
    }
    if (ax < DpadDead && ay < DpadDead) return 0;
    if (ax >= ay) return dx < 0 ? BtnLeft : BtnRight;
    return dy < 0 ? BtnUp : BtnDown;
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
        const float scale = BbSettings::Get().touch_pad_scale.load();
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
            case Kind::Dpad:
                buttons |= DpadBits(f.x * Width, f.y * Height, scale);
                break;
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
            if (f.kind == Kind::Dpad) held |= DpadBits(f.x * Width, f.y * H, scale);

        }
    }
    const float line = 3.0f * scale * H/1080.0f;
    for (const auto& c : Controls) {
        const ImVec2 centre = Centre(c);
        const bool on = held & c.bit;
        if (c.shape == Shape::Circle) {
            const float r = c.w * H * scale * 0.5f;
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
            dl->AddCircleFilled(centre,r,tint(on?135:65),48);
            dl->AddCircle(centre,r,tint(on?255:220),48,line);
            if(c.glyph!=Glyph::Text)DrawGlyph(dl,c.glyph,centre,r*.43f,tint(255),line);

        } else {
            const ImVec2 a(centre.x - c.w * H * scale * 0.5f, centre.y - c.h * H * scale * 0.5f);
            const ImVec2 b(centre.x + c.w * H * scale * 0.5f, centre.y + c.h * H * scale * 0.5f);
            const float rounding = c.h * H * scale * 0.18f;
            dl->AddRectFilled(a,b,on?IM_COL32(151,117,58,int(170*alpha)):IM_COL32(11,17,16,int(200*alpha)),rounding);
            dl->AddRect(a,b,IM_COL32(199,174,121,int((on?250:185)*(.5f+.5f*alpha))),rounding,0,line);
        }
        if (c.glyph == Glyph::Text) {
            label(c.label, centre, c.shape == Shape::Circle ? text_size*.72f : text_size * 0.8f);
        }
    }
    // the D-pad
    {
        const ImVec2 c(DpadX * H, DpadY * H);
        const float arm = DpadArm * H * scale, w = DpadWidth * H * scale * 0.5f;
        const struct { uint32_t bit; ImVec2 a, b; } arms[] = {
            {BtnUp, ImVec2(c.x - w, c.y - arm), ImVec2(c.x + w, c.y - w * 0.4f)},
            {BtnDown, ImVec2(c.x - w, c.y + w * 0.4f), ImVec2(c.x + w, c.y + arm)},
            {BtnLeft, ImVec2(c.x - arm, c.y - w), ImVec2(c.x - w * 0.4f, c.y + w)},
            {BtnRight, ImVec2(c.x + w * 0.4f, c.y - w), ImVec2(c.x + arm, c.y + w)},
        };
        for (const auto& a : arms) {
            const bool on = held & a.bit;
            dl->AddRectFilled(a.a,a.b,fill(on),w*.30f);
            dl->AddRect(a.a,a.b,col(on?250:190),w*.30f,0,line);
            const ImVec2 centre((a.a.x+a.b.x)*.5f,(a.a.y+a.b.y)*.5f);
            const float size=w*.36f;
            ImVec2 tip,b1,b2;
            if(a.bit==BtnUp){tip={centre.x,centre.y-size};b1={centre.x-size,centre.y+size};b2={centre.x+size,centre.y+size};}
            else if(a.bit==BtnDown){tip={centre.x,centre.y+size};b1={centre.x-size,centre.y-size};b2={centre.x+size,centre.y-size};}
            else if(a.bit==BtnLeft){tip={centre.x-size,centre.y};b1={centre.x+size,centre.y-size};b2={centre.x+size,centre.y+size};}
            else {tip={centre.x+size,centre.y};b1={centre.x-size,centre.y-size};b2={centre.x-size,centre.y+size};}
            dl->AddTriangleFilled(tip,b1,b2,col(240));
        }
    }
    // Twin analogs like the original console layout. No game-specific action names:
    // all three games and their menu prompts keep their real PlayStation mappings.
    for(int side=0;side<2;++side) {
        const float radius=StickRadius*H*scale, knob=KnobRadius*H*scale;
        ImVec2 centre(side ? Width-.52f*H : StickHintX*H, StickHintY*H), point=centre;
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

bool TouchPadEvent(const SDL_Event& event) {
    std::scoped_lock lock{fingers_mutex};
    const float scale = BbSettings::Get().touch_pad_scale.load();
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN: {
        const float px = event.tfinger.x * Width, py = event.tfinger.y * Height;
        int control = -1;
        for (int i = 0; i < ControlCount; ++i) {
            if (Hits(Controls[i], px, py, scale)) {
                control = i;
                break;
            }
        }
        // the menu's swipe-in strip, unless the finger is on a control there (R1, R2)
        if (control < 0 && event.tfinger.x >= 1.0f - EdgeZone) return false;
        Finger* f = Free();
        if (!f) return false;
        *f = Finger{};
        f->id = event.tfinger.fingerID;
        f->down = true;
        f->down_time = Now();
        f->x = f->x0 = event.tfinger.x;
        f->y = f->y0 = event.tfinger.y;
        if (control >= 0) {
            f->kind = Kind::Button;
            f->control = control;
        } else if (DpadBits(px, py, scale)) {
            f->kind = Kind::Dpad;
        } else {
            f->kind = event.tfinger.x < 0.5f ? Kind::Stick : Kind::Look;
        }
        return true;
    }
    case SDL_EVENT_FINGER_MOTION: {
        Finger* f = Find(event.tfinger.fingerID);
        if (!f) return false;
        if (f->kind == Kind::Look) {
            f->look_dx += event.tfinger.x - f->x;
            f->look_dy += event.tfinger.y - f->y;
        }
        f->x = event.tfinger.x;
        f->y = event.tfinger.y;
        return true;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        Finger* f = Find(event.tfinger.fingerID);
        if (!f) return false;
        if ((f->kind == Kind::Button || f->kind == Kind::Dpad) && Now() - f->down_time < MinHold &&
            event.type == SDL_EVENT_FINGER_UP) {
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

void TouchPadFrame(float width, float height) {
    Width = width;
    Height = height;
    const bool active = TouchPadActive();
    if (!active) {
        if (was_active) TouchPadReset();
        was_active = false;
        return;
    }
    was_active = true;
    const auto& s = BbSettings::Get();
    const float alpha = std::clamp(s.touch_pad_alpha.load(), 0.1f, 1.0f);
    const float scale = std::clamp(s.touch_pad_scale.load(), 0.6f, 1.6f);
    {
        std::scoped_lock lock{fingers_mutex};
        Push(false);
    }
    Draw(alpha, scale);
}

void TouchPadReset() {
    std::scoped_lock lock{fingers_mutex};
    for (auto& f : fingers) f.down = f.lifted = false;
    Push(true);
}

} // namespace BbOverlay
