#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include "rnnoise_filter.h"
#include <array>
#include <cmath>
#include <iostream>

int main() {
    a50::RNNoiseFilter filter;
    std::array<float,480> result{};
    // Confirm exact 10-ms frame grouping without needing an audio device.
    for(int i=0;i<479;++i) assert(!filter.feed(0.f,result));
    assert(filter.feed(0.f,result));
    for(float x:result) assert(std::isfinite(x) && std::fabs(x)<=1.f);
    for(int frame=0;frame<20;++frame) {
        int completed=0;
        for(int i=0;i<480;++i) {
            const float signal=0.18f*std::sin((frame*480+i)*0.03f);
            if(filter.feed(signal,result)) ++completed;
        }
        assert(completed==1);
        assert(filter.last_voice_probability()>=0.f &&
               filter.last_voice_probability()<=1.f);
        for(float x:result) assert(std::isfinite(x) && std::fabs(x)<=1.f);
    }
    std::cout << "RNNoise 48 kHz frames passed\n";
}
