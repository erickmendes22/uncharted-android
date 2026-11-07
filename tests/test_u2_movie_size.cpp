// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_vdec_content.h"
#include <cassert>
int main() {
    using namespace BbVdecContent;
    const Size frame{1920,1080};
    const auto s360=Choose(Parse("640x360"),frame);
    assert(s360.width==640 && s360.height==360);
    const auto s720=Choose(Parse("1280x720"),frame);
    assert(s720.width==1280 && s720.height==720);
    assert(Choose(Parse("1920x1080"),frame).width==1920);
    assert(Choose(Parse("3840x2160"),frame).width==0);
    assert(Parse("641x360").width==0);
    assert(Parse("").width==0);
    assert(Parse(nullptr).width==0);
    return 0;
}
