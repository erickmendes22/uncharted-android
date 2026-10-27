// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
struct ImFont;
struct ImFontAtlas;
namespace BbFontLoader {
bool Compatible(const void* data, size_t size);
ImFont* AddMemory(ImFontAtlas& atlas,const void* data,size_t size,float pixels,bool merge=false,bool owned=false);
ImFont* AddFile(ImFontAtlas& atlas,const char* path,float pixels,bool merge=false);
}
