#include "dsp.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace a50 {
namespace {
constexpr float pi = 3.14159265358979323846f;
float clamp(float v, float a, float b) { return std::max(a, std::min(v, b)); }
float db_to_amp(float db) { return std::pow(10.0f, db / 20.0f); }
float one_pole(unsigned rate, float cutoff) { return 1.0f - std::exp(-2.0f * pi * cutoff / rate); }
}
Processor::Processor(unsigned rate, Settings settings) : rate_(rate) {
    if (rate < 8000 || rate > 192000) throw std::invalid_argument("Unsupported sample rate");
    ring_.resize(static_cast<std::size_t>(rate * 0.16f) + 32, 0.0f);
    set_settings(settings);
}
void Processor::set_settings(Settings settings) {
    settings.gain_db = clamp(settings.gain_db, -24.0f, 24.0f);
    settings.threshold_dbfs = clamp(settings.threshold_dbfs, -80.0f, -6.0f);
    settings.hold_ms = clamp(settings.hold_ms, 0.0f, 1000.0f);
    settings.release_ms = clamp(settings.release_ms, 10.0f, 2000.0f);
    settings.pitch_semitones = clamp(settings.pitch_semitones, -12.0f, 12.0f);
    settings.resonance = clamp(settings.resonance, -1.0f, 1.0f);
    settings.bass_db = clamp(settings.bass_db, -18.0f, 18.0f);
    settings.brightness_db = clamp(settings.brightness_db, -18.0f, 18.0f);
    settings.effect_mix = clamp(settings.effect_mix, 0.0f, 1.0f);
    settings_ = settings;
}
void Processor::reset() {
    previous_input_ = previous_output_ = envelope_ = gate_gain_ = 0.0f;
    low_bass_ = low_tone_ = low_bright_ = 0.0f;
    hold_samples_left_ = 0;
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    write_index_ = 0;
    grain_phase_ = 0.0f;
}
float Processor::shift_pitch(float x, float ratio) {
    // Two Hann-windowed delay taps. No model files or AVX2 required.
    // The dual-grain method adds about 1-2 grains of latency.
    ring_[write_index_] = x;
    const float grain = rate_ * 0.028f;
    const float base = grain + 2.0f;
    auto tap = [&](float phase) {
        const float delay = base + grain * (1.0f - phase);
        float pos = static_cast<float>(write_index_) - delay;
        const float size = static_cast<float>(ring_.size());
        while (pos < 0) pos += size;
        const auto a = static_cast<std::size_t>(pos);
        const auto b = (a + 1) % ring_.size();
        const float t = pos - static_cast<float>(a);
        return ring_[a] * (1.0f - t) + ring_[b] * t;
    };
    const float phase2 = std::fmod(grain_phase_ + 0.5f, 1.0f);
    const float w = 0.5f - 0.5f * std::cos(2.0f * pi * grain_phase_);
    const float out = w * tap(grain_phase_) + (1.0f - w) * tap(phase2);
    grain_phase_ += (ratio - 1.0f) / grain;
    grain_phase_ -= std::floor(grain_phase_);
    write_index_ = (write_index_ + 1) % ring_.size();
    return out;
}
void Processor::process(float* samples, std::size_t count) {
    if (!samples && count) throw std::invalid_argument("Null samples");
    const float gain = db_to_amp(settings_.gain_db);
    const float threshold = db_to_amp(settings_.threshold_dbfs);
    const float dt = 1.0f / rate_;
    const float rc = 1.0f / (2.0f * pi * 80.0f);
    const float hp_alpha = rc / (rc + dt);
    const float env_attack = std::exp(-1.0f / (0.004f * rate_));
    const float env_release = std::exp(-1.0f / (0.070f * rate_));
    const float gate_attack = std::exp(-1.0f / (0.003f * rate_));
    const float gate_release = std::exp(-1.0f / (settings_.release_ms * .001f * rate_));
    const auto hold_samples = static_cast<std::size_t>(settings_.hold_ms * .001f * rate_);
    const float ratio = std::pow(2.0f, settings_.pitch_semitones / 12.0f);
    const float bass_gain = db_to_amp(settings_.bass_db);
    const float bright_gain = db_to_amp(settings_.brightness_db);
    const float lp_bass = one_pole(rate_, 190.0f);
    const float lp_tone = one_pole(rate_, 1100.0f + settings_.resonance * 450.0f);
    const float lp_bright = one_pole(rate_, 2900.0f);
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
        } else gate_gain_ = 1.0f;
        y *= gain * gate_gain_;
        if (settings_.voice_enabled) {
            const float dry = y;
            float wet = shift_pitch(y, ratio);
            low_bass_ += lp_bass * (wet - low_bass_);
            low_tone_ += lp_tone * (wet - low_tone_);
            low_bright_ += lp_bright * (wet - low_bright_);
            wet += (bass_gain - 1.0f) * low_bass_;
            wet += 0.5f * settings_.resonance * (low_tone_ - low_bass_);
            wet += (bright_gain - 1.0f) * (wet - low_bright_);
            y = dry * (1.0f - settings_.effect_mix) + wet * settings_.effect_mix;
        }
        samples[i] = clamp(y, -1.0f, 1.0f);
    }
}
}
