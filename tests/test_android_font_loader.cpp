#include "bbport_font_loader.h"
#include "imgui.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
static unsigned u16(const unsigned char* p){return unsigned(p[0])<<8|p[1];}
static void set32(unsigned char* p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(){
    std::ifstream f("gpu/third_party/fonts/DejaVuSans.ttf",std::ios::binary);
    std::vector<unsigned char> valid{std::istreambuf_iterator<char>(f),{}};assert(!valid.empty());
    assert(BbFontLoader::Compatible(valid.data(),valid.size()));
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;auto& atlas=*ImGui::GetIO().Fonts;
    assert(BbFontLoader::AddMemory(atlas,valid.data(),valid.size(),18));
    const int fonts=atlas.Fonts.Size,sources=atlas.Sources.Size;
    auto reject=[&](const std::vector<unsigned char>& bytes){
        assert(!BbFontLoader::Compatible(bytes.data(),bytes.size()));
        assert(!BbFontLoader::AddMemory(atlas,bytes.data(),bytes.size(),18,true));
        assert(atlas.Fonts.Size==fonts && atlas.Sources.Size==sources);
    };
    reject({});reject(std::vector<unsigned char>(120,0));
    auto invalid=valid;invalid.resize(valid.size()/2);reject(invalid);
    invalid=valid;set32(invalid.data()+12+8,0xfffffff0);reject(invalid);
    invalid=valid;
    for(unsigned n=0;n<u16(invalid.data()+4);++n) {
        auto* entry=invalid.data()+12+n*16;
        if(!std::memcmp(entry,"glyf",4)){std::memcpy(entry,"CFF2",4);break;}
    }
    reject(invalid); // Android variable-outline fallback must not reach ImGui's assert path.
    // A valid face inside a collection still works; SFNT table offsets are absolute.
    std::vector<unsigned char> ttc(valid.size()+16);std::memcpy(ttc.data(),"ttcf",4);
    set32(ttc.data()+4,0x10000);set32(ttc.data()+8,1);set32(ttc.data()+12,16);
    std::memcpy(ttc.data()+16,valid.data(),valid.size());
    for(unsigned n=0;n<u16(ttc.data()+20);++n){auto* p=ttc.data()+28+n*16+8;unsigned at=(unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3];set32(p,at+16);}
    assert(BbFontLoader::Compatible(ttc.data(),ttc.size()));
    assert(BbFontLoader::AddMemory(atlas,ttc.data(),ttc.size(),18,true));
    unsigned char* pixels;int w,h;atlas.GetTexDataAsRGBA32(&pixels,&w,&h);assert(pixels && w>0 && h>0);
    ImGui::GetIO().DisplaySize={1280,720};ImGui::NewFrame();ImGui::Begin("test");ImGui::TextUnformatted("Português / English / FPS 30→60 LSFG 2x");ImGui::End();ImGui::Render();
    ImGui::DestroyContext();std::puts("Font loading: PASS (invalid/CFF2 rejected before atlas mutation, TTF/TTC and fallback drawing intact)");
}
