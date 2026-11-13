#ifdef NDEBUG
#undef NDEBUG
#endif
#include "voice_clarity.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    a50::VoiceClarity clarity;
    std::array<float,480> original{}, filtered{}, output{};
    // Disabled is exact bypass, not a second hidden gate.
    clarity.set_enabled(false);
    for(int i=0;i<480;++i) {original[i]=0.02f;filtered[i]=0.01f;}
    clarity.process(original,filtered,0.95f,output);
    for(float s:output) assert(s==0.01f);
    clarity.set_enabled(true);
    clarity.set_denoise_strength(0.8f);
    // Sustained low-volume speech is lifted, and consonants aren't gated.
    float average=0.f;
    for(int frame=0;frame<100;++frame) {
        for(int i=0;i<480;++i) {
            const float s=0.006f*std::sin((frame*480+i)*0.047f);
            original[i]=s;filtered[i]=s*0.8f;
        }
        clarity.process(original,filtered,0.80f,output);
        if(frame>80) for(float s:output) {
            assert(std::isfinite(s) && std::fabs(s)<=0.901f);
            average+=std::fabs(s);
        }
    }
    assert(average/(19.f*480.f)>0.009f); // louder than original
    // After speech stops, silence stays silent: no AGC floor noise.
    original.fill(0.f);filtered.fill(0.f);
    for(int frame=0;frame<100;++frame) {
        clarity.process(original,filtered,0.f,output);
        for(float s:output) assert(std::fabs(s)<0.00001f);
    }
    // Ambient noise with low speech probability is not boosted by AGC.
    clarity.reset();
    original.fill(0.004f);filtered.fill(0.001f);
    for(int frame=0;frame<80;++frame) clarity.process(original,filtered,0.01f,output);
    for(float s:output) assert(std::fabs(s)<0.004f);
    // Strong samples never exceed the limiter.
    original.fill(0.9f);filtered.fill(0.9f);
    for(int frame=0;frame<100;++frame) clarity.process(original,filtered,0.95f,output);
    for(float s:output) assert(std::isfinite(s) && std::fabs(s)<=0.901f);
    // Entirely invalid input cannot produce NaNs.
    original.fill(NAN);filtered.fill(NAN);
    clarity.process(original,filtered,NAN,output);
    for(float s:output) assert(std::isfinite(s));
    std::cout<<"Quiet-speech clarity, bypass, silence and peak limiting passed\n";
}
