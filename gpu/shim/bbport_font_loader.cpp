// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_font_loader.h"
#include "imgui.h"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

namespace BbFontLoader {
namespace {
constexpr size_t MaxFontBytes=32*1024*1024;
uint16_t U16(const unsigned char* p){return uint16_t(p[0])<<8|p[1];}
uint32_t U32(const unsigned char* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
bool Range(size_t at,size_t length,size_t size){return at<=size && length<=size-at;}
}
bool Compatible(const void* data,size_t size) {
    if(!data || size<=100 || size>MaxFontBytes || size>INT_MAX)return false;
    const auto* bytes=static_cast<const unsigned char*>(data);
    size_t face=0;
    if(!std::memcmp(bytes,"ttcf",4)) {
        if(!Range(0,16,size) || !U32(bytes+8))return false;
        const uint32_t count=U32(bytes+8);
        if(count>(size-12)/4)return false;
        face=U32(bytes+12);
    }
    if(!Range(face,12,size))return false;
    const uint32_t signature=U32(bytes+face);
    if(signature!=0x00010000 && signature!=0x4f54544f && signature!=0x74727565)return false;
    const size_t count=U16(bytes+face+4),directory=face+12;
    if(!count || count>(size-directory)/16)return false;
    bool cmap=false,head=false,hhea=false,hmtx=false,maxp=false,outlines=false,loca=false;
    size_t cmap_at=0,cmap_size=0;
    for(size_t n=0;n<count;++n) {
        const auto* entry=bytes+directory+16*n;
        const size_t at=U32(entry+8),length=U32(entry+12);
        if(!Range(at,length,size))return false;
        // stb_truetype doesn't support CFF2 variable outlines. Never add these
        // sources to ImGui: failure inside AddFont mutates its atlas first.
        if(!std::memcmp(entry,"CFF2",4))return false;
        if(!std::memcmp(entry,"cmap",4)){cmap=length>=4;cmap_at=at;cmap_size=length;}
        if(!std::memcmp(entry,"head",4))head=length>=54;
        if(!std::memcmp(entry,"hhea",4))hhea=length>=36;
        if(!std::memcmp(entry,"hmtx",4))hmtx=length>=4;
        if(!std::memcmp(entry,"maxp",4))maxp=length>=6;
        if(!std::memcmp(entry,"glyf",4))outlines=length>0;
        if(!std::memcmp(entry,"loca",4))loca=length>=2;
        if(!std::memcmp(entry,"CFF ",4)){outlines=length>=4;loca=true;}
    }
    if(!cmap || !head || !hhea || !hmtx || !maxp || !outlines || !loca)return false;
    const size_t records=U16(bytes+cmap_at+2);
    if(records>(cmap_size-4)/8)return false;
    for(size_t n=0;n<records;++n) {
        const size_t sub=U32(bytes+cmap_at+4+n*8+4);
        if(sub>=cmap_size || cmap_size-sub<2)return false;
    }
    stbtt_fontinfo info{};
    return stbtt_InitFont(&info,bytes,int(face))!=0;
}
ImFont* AddMemory(ImFontAtlas& atlas,const void* data,size_t size,float pixels,bool merge,bool owned) {
    if(!Compatible(data,size) || (merge && atlas.Fonts.empty())) {
        if(owned)IM_FREE(const_cast<void*>(data));
        return nullptr;
    }
    ImFontConfig config;config.MergeMode=merge;config.FontDataOwnedByAtlas=owned;
    return atlas.AddFontFromMemoryTTF(const_cast<void*>(data),int(size),pixels,&config);
}
ImFont* AddFile(ImFontAtlas& atlas,const char* path,float pixels,bool merge) {
    FILE* file=std::fopen(path,"rb");if(!file)return nullptr;
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);return nullptr;}
    const long length=std::ftell(file);
    if(length<=100 || size_t(length)>MaxFontBytes || std::fseek(file,0,SEEK_SET)!=0){std::fclose(file);return nullptr;}
    void* data=IM_ALLOC(size_t(length));
    if(!data){std::fclose(file);return nullptr;}
    const bool complete=std::fread(data,1,size_t(length),file)==size_t(length);std::fclose(file);
    if(!complete){IM_FREE(data);return nullptr;}
    ImFont* font=AddMemory(atlas,data,size_t(length),pixels,merge,true);
    if(!font)std::printf("Overlay: skipped unsupported font %s; keeping the existing font\n",path);
    return font;
}
}
