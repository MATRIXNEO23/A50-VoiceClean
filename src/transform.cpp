#include "transform.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace a50 {
Wave transform(Wave input, Settings settings, float speed) {
    if (speed < 0.70f || speed > 1.30f || !std::isfinite(speed))
        throw std::invalid_argument("Speed must be 0.70 to 1.30");
    if (input.samples.empty()) return input;
    if (std::fabs(speed - 1.0f) > 0.0001f) {
        Wave changed;
        changed.sample_rate = input.sample_rate;
        const std::size_t count = static_cast<std::size_t>(input.samples.size() / speed);
        changed.samples.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            const double p = i * static_cast<double>(speed);
            const auto j = static_cast<std::size_t>(p);
            const auto j2 = std::min(j + 1, input.samples.size() - 1);
            const float t = static_cast<float>(p - j);
            changed.samples[i] = input.samples[j] * (1.0f - t) + input.samples[j2] * t;
        }
        input = std::move(changed);
        // Approximate pitch compensation for the resampling tempo change.
        settings.voice_enabled = true;
        settings.pitch_semitones -= 12.0f * std::log2(speed);
    }
    Processor dsp(input.sample_rate, settings);
    constexpr std::size_t block = 160;
    for (std::size_t i=0; i<input.samples.size(); i += block)
        dsp.process(input.samples.data()+i, std::min(block,input.samples.size()-i));
    return input;
}
}
