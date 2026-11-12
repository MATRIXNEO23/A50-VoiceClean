#include "transform.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
    constexpr unsigned sr=16000;
    a50::Settings s; s.gate_enabled=false;s.highpass_enabled=false;s.gain_db=6.0206f;
    a50::Processor p(sr,s);
    std::vector<float> x(160,0.2f);p.process(x.data(),x.size());
    assert(std::fabs(x.back()-0.4f)<0.003f);
    s.gain_db=0;s.gate_enabled=true;s.threshold_dbfs=-20;
    p.set_settings(s);p.reset();
    x.assign(1600,0.01f);p.process(x.data(),x.size());
    assert(std::fabs(x.back())<0.001f);
    x.assign(1600,0.5f);p.process(x.data(),x.size());
    assert(x.back()>0.40f);
    a50::Settings dry;dry.gate_enabled=dry.highpass_enabled=false;
    a50::Processor d(sr,dry);
    x.assign(1000,0.25f);d.process(x.data(),x.size());
    assert(std::fabs(x.back()-0.25f)<1e-6f);
    a50::Settings fx=dry;fx.voice_enabled=true;fx.pitch_semitones=5.f;
    fx.resonance=.4f;fx.bass_db=-3.f;fx.brightness_db=3.f;
    a50::Processor wet(sr,fx);
    std::vector<float> tone(sr);
    for(std::size_t i=0;i<tone.size();++i)
        tone[i]=.3f*std::sin(2.0*3.141592653589793*210.0*i/sr);
    auto original=tone;
    wet.process(tone.data(),tone.size());
    float diff=0;
    for(std::size_t i=0;i<tone.size();++i){
        assert(std::isfinite(tone[i]));
        assert(tone[i]>=-1.f && tone[i]<=1.f);
        diff+=std::fabs(tone[i]-original[i]);
    }
    assert(diff>100.f);
    a50::Wave w;w.sample_rate=sr;w.samples=original;
    assert(a50::transform(w,dry,.8f).samples.size()>w.samples.size());
    assert(a50::transform(w,dry,1.2f).samples.size()<w.samples.size());
    bool rejected=false;
    try{(void)a50::transform(w,dry,.5f);}
    catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"DSP and voice-effect tests passed\n";
}
