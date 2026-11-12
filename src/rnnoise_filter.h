#pragma once
#include <array>
#include <cstddef>

struct DenoiseState;

namespace a50 {
// RNNoise operates on exactly 480 samples of 48 kHz mono PCM per frame.
// Public interface uses normalized float [-1,1] to match the WASAPI pipeline.
class RNNoiseFilter {
    DenoiseState* state_ = nullptr;
    std::array<float,480> input_{};
    std::array<float,480> denoised_{};
    std::size_t filled_ = 0;
    float voice_probability_ = 0.f;
public:
    RNNoiseFilter();
    ~RNNoiseFilter();
    RNNoiseFilter(const RNNoiseFilter&)=delete;
    RNNoiseFilter& operator=(const RNNoiseFilter&)=delete;
    // Returns true if a complete 10-ms frame is emitted into result.
    bool feed(float sample,std::array<float,480>& result);
    float last_voice_probability() const noexcept {return voice_probability_;}
};
}
