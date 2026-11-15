#ifdef NDEBUG
#undef NDEBUG
#endif
#include "formant_voice.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // Timbre must be a distinct adjustment, never derived from pitch.
    assert(std::fabs(a50::formant_semitones_from_timbre(0.35f)-2.1f)<0.0001f);
    assert(a50::formant_semitones_from_timbre(0.f)==0.f);
    assert(a50::formant_semitones_from_timbre(2.f)==6.f);
    assert(a50::formant_semitones_from_timbre(-2.f)==-6.f);
    a50::FormantVoice voice;
    // 48kHz streaming requires bounded latency. This is not an audio-quality test.
    assert(voice.latency_samples()>0 && voice.latency_samples()<48000);
    voice.set_settings(4.f,4.f,1.f);
    std::array<float,480> frame{};
    double energy=0.0;
    for(int f=0;f<120;++f) {
        for(std::size_t i=0;i<frame.size();++i) {
            const int n=f*480+static_cast<int>(i);
            frame[i]=0.08f*std::sin(2.0*3.141592653589793*185*n/48000.0);
        }
        voice.process(frame);
        for(float s:frame) {
            assert(std::isfinite(s) && std::fabs(s)<=0.9801f);
            if(f>=60) energy+=s*s;
        }
    }
    assert(energy>0.001);
    voice.set_settings(8.f,5.f,0.5f);
    for(int f=0;f<60;++f) {
        frame.fill(0.02f);
        voice.process(frame);
        for(float s:frame) assert(std::isfinite(s) && std::fabs(s)<=0.9801f);
    }
    // Rapid slider movement must never produce NaNs or out-of-range samples.
    voice.set_settings(-10.f,-5.f,0.f);
    for(int f=0;f<50;++f) {
        frame.fill(0.02f);
        voice.process(frame);
        for(float sample:frame)
            assert(std::isfinite(sample) && std::fabs(sample)<=0.9801f);
    }
    voice.reset();
    frame.fill(0.f);
    for(int f=0;f<40;++f) voice.process(frame);
    for(float s:frame) assert(std::isfinite(s));
    std::cout<<"Spectral voice finite output, latency, settings and reset passed\n";
}
