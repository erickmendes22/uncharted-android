// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace BbHudText {
struct Span { std::string text; ImVec4 color; };
using Line = std::vector<Span>;
using Lines = std::vector<Line>;
struct Layout { float font; ImVec2 size; };
// Measure the exact strings/font used for drawing, including FG and translated
// mode names. Explicit dimensions prevent AlwaysAutoResize's one-frame delay
// from clipping the first frame after FG, locale or orientation changes.
inline Layout Measure(const Lines& lines,float desired,float max_width,ImVec2 padding) {
    const float available=std::max(1.f,max_width-2*padding.x-2.f);
    float font=desired,width=0,height=0;
    for(int pass=0;pass<4;++pass) {
        ImGui::PushFont(nullptr,font);
        width=0;
        for(const auto& line:lines) {
            float w=0;for(const auto& span:line)w+=ImGui::CalcTextSize(span.text.c_str()).x;
            width=std::max(width,w);
        }
        height=lines.size()*ImGui::GetTextLineHeight()+std::max(0,int(lines.size())-1)*ImGui::GetStyle().ItemSpacing.y;
        ImGui::PopFont();
        if(width<=available)break;
        font*=available/std::max(width,1.f)*.98f;
    }
    return {font,{std::min(max_width,std::ceil(width+2*padding.x+2)),std::ceil(height+2*padding.y+2)}};
}
inline void Draw(const Lines& lines) {
    for(const auto& line:lines) {
        bool first=true;
        for(const auto& span:line) {
            if(!first)ImGui::SameLine(0,0);
            ImGui::TextColored(span.color,"%s",span.text.c_str());first=false;
        }
    }
}
}
