#pragma once
#include <string>
#include <vector>
namespace a50 {
struct Wave {
    unsigned sample_rate = 0;
    std::vector<float> samples;
};
Wave read_pcm16_mono_wave(const std::string& path);
void write_pcm16_mono_wave(const std::string& path, const Wave& wave);
}
