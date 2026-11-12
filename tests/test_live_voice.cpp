#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include "dsp.h"
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    constexpr unsigned rate=48000;
    std::vector<float> original(rate);
    for(std::size_t i=0;i<original.size();++i)
        original[i]=0.2f*std::sin(2.f*3.14159265f*180.f*i/rate);
    // Bypass mode must preserve the original signal.
    a50::Settings bypass;
    bypass.gate_enabled=false;
    bypass.highpass_enabled=false;
    a50::Processor plain(rate,bypass);
    auto plain_data=original;
    for(std::size_t i=0;i<plain_data.size();i+=480)
        plain.process(plain_data.data()+i,480);
    for(std::size_t i=0;i<original.size();++i)
        assert(std::fabs(plain_data[i]-original[i])<0.00001f);

    // Live settings must work frame-by-frame without a WAV tempo transformation.
    a50::Settings s=bypass;
    s.voice_enabled=true;
    s.pitch_semitones=5;
    s.resonance=0.4f;
    s.bass_db=-3.f;
    s.brightness_db=2.f;
    s.effect_mix=1.f;
    a50::Processor voice(rate,s);
    auto modified=original;
    for(std::size_t i=0;i<modified.size();i+=480)
        voice.process(modified.data()+i,480);
    float max_sample=0, difference=0;
    for(std::size_t i=0;i<modified.size();++i) {
        assert(std::isfinite(modified[i]));
        assert(std::fabs(modified[i])<=1.f);
        max_sample=std::max(max_sample,std::fabs(modified[i]));
        if(i>rate/2) difference=std::max(difference,std::fabs(modified[i]-original[i]));
    }
    assert(max_sample>0.02f);
    assert(difference>0.03f);
    std::cout<<"Live 48 kHz voice DSP tests passed\n";
}
