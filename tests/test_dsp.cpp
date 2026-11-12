#include "dsp.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main() {
    constexpr unsigned sr = 16000;
    a50::Settings s; s.gate_enabled = false; s.highpass_enabled = false;
    s.gain_db = 6.0206f;
    a50::Processor p(sr, s);
    std::vector<float> samples(160, 0.2f);
    p.process(samples.data(), samples.size());
    assert(std::fabs(samples.back() - 0.4f) < 0.003f);
    s.gain_db = 0; s.gate_enabled = true; s.highpass_enabled = false;
    s.threshold_dbfs = -20;
    p.set_settings(s); p.reset();
    std::vector<float> silence(1600, 0.01f);
    p.process(silence.data(), silence.size());
    assert(std::fabs(silence.back()) < 0.001f);
    std::vector<float> voice(1600, 0.5f);
    p.process(voice.data(), voice.size());
    assert(voice.back() > 0.40f);
    std::cout << "DSP tests passed\n";
}
