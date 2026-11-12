#pragma once
#include <filesystem>
#include <vector>
namespace a50 {
struct Wave {
    unsigned sample_rate = 0;
    std::vector<float> samples;
};
Wave read_pcm16_mono_wave(const std::filesystem::path& path);
void write_pcm16_mono_wave(const std::filesystem::path& path, const Wave& wave);
}
