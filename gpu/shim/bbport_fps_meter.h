// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>

namespace BbOverlay {
// One observation per successfully presented image. Original game frames and
// generated/reused images share a time window; no configured multiplier is used.
struct FpsMeter {
    double start = -1;
    unsigned originals = 0, output = 0, generated = 0;
    float real_fps = 0, output_fps = 0, generated_fps = 0;
    void Presented(double now, bool original, bool interpolated = false) {
        if (start < 0 || now-start > 3.0 || now < start) {
            start=now; originals=output=generated=0; real_fps=output_fps=generated_fps=0;
            return;
        }
        ++output;
        if (original) ++originals;
        if (interpolated) ++generated;
        const double elapsed=now-start;
        if (elapsed >= 1.0) {
            real_fps=float(originals/elapsed);
            output_fps=float(output/elapsed);
            generated_fps=float(generated/elapsed);
            start=now; originals=output=generated=0;
        }
    }
    void Format(char* text, size_t size, int multiplier) const {
        const int source=int(std::lround(real_fps)), presented=int(std::lround(output_fps));
        if(multiplier>=2 && source>0 && generated_fps>0 && presented>0 && presented!=source)
            std::snprintf(text,size,"FPS %d→%d LSFG %dx",source,presented,multiplier);
        else std::snprintf(text,size,"FPS %d",source);
    }
};
}
