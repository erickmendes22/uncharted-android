#include "bbport_fps_meter.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <cstring>
int main() {
    for (unsigned mult : {1u,2u,3u,4u}) {
        BbOverlay::FpsMeter meter;
        meter.Presented(0,false);
        for(unsigned i=1;i<=30*mult;++i)meter.Presented(double(i)/(30*mult),i%mult==0,i%mult!=0);
        assert(std::abs(meter.real_fps-30)<.01f);
        assert(std::abs(meter.output_fps-30*mult)<.01f);
        char text[96],expected[96];meter.Format(text,sizeof(text),int(mult));
        if(mult==1)std::snprintf(expected,sizeof(expected),"FPS 30");
        else std::snprintf(expected,sizeof(expected),"FPS 30→%u LSFG %ux",30*mult,mult);
        assert(std::strcmp(text,expected)==0);
    }
    // A dropped generated frame lowers measured output; reused images never add game FPS.
    BbOverlay::FpsMeter meter;meter.Presented(0,false);
    for(int i=1;i<=45;++i)meter.Presented(double(i)/45,i<=30);
    assert(meter.real_fps==30 && meter.output_fps==45);
    char text[96];meter.Format(text,sizeof(text),2);
    assert(std::strcmp(text,"FPS 30")==0); // repeated images are not generated frames
    meter.generated_fps=15;meter.Format(text,sizeof(text),2);
    assert(std::strcmp(text,"FPS 30→45 LSFG 2x")==0); // measured, not 30*2
    meter.Format(text,sizeof(text),1);assert(std::strcmp(text,"FPS 30")==0);
    meter.Presented(8,true);assert(meter.real_fps==0 && meter.output_fps==0);
    meter.Presented(1,true);assert(meter.real_fps==0 && meter.output_fps==0);
}
