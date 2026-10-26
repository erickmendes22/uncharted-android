// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

namespace BbOverlay {
// One observation per successfully presented image. Original game frames and
// generated/reused images share a time window; no configured multiplier is used.
struct FpsMeter {
    double start = -1;
    unsigned originals = 0, output = 0;
    float real_fps = 0, output_fps = 0;
    void Presented(double now, bool original) {
        if (start < 0 || now-start > 3.0 || now < start) {
            start=now; originals=output=0; real_fps=output_fps=0;
            return;
        }
        ++output;
        if (original) ++originals;
        const double elapsed=now-start;
        if (elapsed >= 1.0) {
            real_fps=float(originals/elapsed);
            output_fps=float(output/elapsed);
            start=now; originals=output=0;
        }
    }
};
}
