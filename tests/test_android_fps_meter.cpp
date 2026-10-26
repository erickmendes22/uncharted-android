#include "bbport_fps_meter.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main() {
    for (unsigned mult : {1u,2u,3u,4u}) {
        BbOverlay::FpsMeter meter;
        meter.Presented(0,false);
        for(unsigned i=1;i<=30*mult;++i)meter.Presented(double(i)/(30*mult),i%mult==0);
        assert(std::abs(meter.real_fps-30)<.01f);
        assert(std::abs(meter.output_fps-30*mult)<.01f);
    }
    // A dropped generated frame lowers measured output; reused images never add game FPS.
    BbOverlay::FpsMeter meter;meter.Presented(0,false);
    for(int i=1;i<=45;++i)meter.Presented(double(i)/45,i<=30);
    assert(meter.real_fps==30 && meter.output_fps==45);
    meter.Presented(8,true);assert(meter.real_fps==0 && meter.output_fps==0);
    meter.Presented(1,true);assert(meter.real_fps==0 && meter.output_fps==0);
}
