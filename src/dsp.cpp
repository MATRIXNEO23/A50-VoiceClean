#include "dsp.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace a50 {
namespace {
float clamp(float v, float a, float b) { return std::max(a, std::min(v, b)); }
float db_to_amp(float db) { return std::pow(10.0f, db / 20.0f); }
}
Processor::Processor(unsigned rate, Settings settings) : rate_(rate) {
    if (rate < 8000 || rate > 192000) throw std::invalid_argument("Unsupported sample rate");
    set_settings(settings);
}
void Processor::set_settings(Settings settings) {
    settings.gain_db = clamp(settings.gain_db, -24.0f, 24.0f);
    settings.threshold_dbfs = clamp(settings.threshold_dbfs, -80.0f, -6.0f);
    settings.hold_ms = clamp(settings.hold_ms, 0.0f, 1000.0f);
    settings.release_ms = clamp(settings.release_ms, 10.0f, 2000.0f);
    settings_ = settings;
}
void Processor::reset() {
    previous_input_ = previous_output_ = envelope_ = gate_gain_ = 0.0f;
    hold_samples_left_ = 0;
}
void Processor::process(float* samples, std::size_t count) {
    if (!samples && count) throw std::invalid_argument("Null samples");
    const float gain = db_to_amp(settings_.gain_db);
    const float threshold = db_to_amp(settings_.threshold_dbfs);
    const float dt = 1.0f / static_cast<float>(rate_);
    const float rc = 1.0f / (2.0f * 3.14159265358979323846f * 80.0f);
    const float hp_alpha = rc / (rc + dt);
    const float env_attack = std::exp(-1.0f / (0.004f * rate_));
    const float env_release = std::exp(-1.0f / (0.070f * rate_));
    const float gate_attack = std::exp(-1.0f / (0.003f * rate_));
    const float gate_release = std::exp(-1.0f / (settings_.release_ms * .001f * rate_));
    const auto hold_samples = static_cast<std::size_t>(settings_.hold_ms * .001f * rate_);
    for (std::size_t i = 0; i < count; ++i) {
        const float x = samples[i];
        float y = x;
        if (settings_.highpass_enabled) {
            y = hp_alpha * (previous_output_ + x - previous_input_);
            previous_input_ = x;
            previous_output_ = y;
        }
        const float level = std::fabs(y);
        const float env_alpha = level > envelope_ ? env_attack : env_release;
        envelope_ = env_alpha * envelope_ + (1.0f - env_alpha) * level;
        if (settings_.gate_enabled) {
            if (envelope_ >= threshold) hold_samples_left_ = hold_samples;
            else if (hold_samples_left_ > 0) --hold_samples_left_;
            const bool open = envelope_ >= threshold || hold_samples_left_ > 0;
            const float alpha = open ? gate_attack : gate_release;
            gate_gain_ = alpha * gate_gain_ + (1.0f - alpha) * (open ? 1.0f : 0.0f);
        } else {
            gate_gain_ = 1.0f;
        }
        samples[i] = clamp(y * gain * gate_gain_, -1.0f, 1.0f);
    }
}
}
