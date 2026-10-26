#include "bbport_hud_text.h"
#include "bbport_fps_meter.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
int main() {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.Fonts->AddFontFromFileTTF("gpu/third_party/fonts/DejaVuSans.ttf",18);
    unsigned char* pixels;int x,y;io.Fonts->GetTexDataAsRGBA32(&pixels,&x,&y);
    unsigned cases=0;
    // Deliberately include narrow portrait transitions, long full-mode text,
    // 300% scaling and FG switching on the very next frame (no auto-size lag).
    for(auto size:{ImVec2(800,480),ImVec2(1280,720),ImVec2(1920,1080),ImVec2(2400,1080),ImVec2(1080,2400),ImVec2(1440,1080)})
    for(float scale:{.7f,1.f,1.6f,3.f})for(int mode:{0,1,2})for(int mult:{1,2,3,4})for(bool right:{false,true}) {
        io.DisplaySize=size;io.DeltaTime=1.f/60;ImGui::NewFrame();
        BbOverlay::FpsMeter meter;meter.real_fps=240;meter.output_fps=240*mult;meter.generated_fps=mult>1?240*(mult-1):0;
        char fps[96];meter.Format(fps,sizeof(fps),mult);
        BbHudText::Lines lines={{{fps,ImVec4(1,1,1,1)}}};
        if(mode>1)lines[0].push_back({" · 1000.0 ms",ImVec4(1,1,1,1)});
        if(mode>0)lines.push_back({{"GPU 100% · CPU 100%",ImVec4(1,1,1,1)}});
        if(mode>1)lines.push_back({{"FSR Desempenho · RAM 15.99 GB",ImVec4(1,1,1,1)}});
        const float base=std::max(size.y/1080.f,.75f),pad=14*base;
        const ImVec2 padding(12*base,7*base);
        auto layout=BbHudText::Measure(lines,21*base*scale,size.x-2*pad,padding);
        assert(layout.font<=21*base*scale && layout.size.x<=size.x-2*pad);
        ImGui::SetNextWindowPos({right?size.x-pad:pad,pad},ImGuiCond_Always,{right?1.f:0.f,0});
        ImGui::SetNextWindowSize(layout.size,ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,padding);
        ImGui::Begin("hud-test",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushFont(nullptr,layout.font);
        auto* window=ImGui::GetCurrentWindow();const float left=window->Pos.x+padding.x,rightEdge=window->Pos.x+window->Size.x-padding.x;
        const float firstY=ImGui::GetCursorScreenPos().y;
        BbHudText::Draw(lines);
        for(const auto& line:lines) {
            float width=0;for(const auto& span:line)width+=ImGui::CalcTextSize(span.text.c_str()).x;
            assert(left+width<=rightEdge+.01f);
        }
        // No wrapping/clipping: every row consumes exactly one text line.
        const float rowsHeight=ImGui::GetCursorScreenPos().y-firstY;
        assert(rowsHeight<=lines.size()*(ImGui::GetTextLineHeight()+ImGui::GetStyle().ItemSpacing.y)+1);
        assert(window->Pos.x>=0 && window->Pos.x+window->Size.x<=size.x+.01f);
        ImGui::PopFont();ImGui::End();ImGui::PopStyleVar();ImGui::Render();++cases;
    }
    ImGui::DestroyContext();std::printf("HUD text: PASS (%u FG/mode/scale/orientation/anchor cases, no clipping or wrapping)\n",cases);
}
