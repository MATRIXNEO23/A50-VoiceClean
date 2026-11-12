#pragma once
#include <cstddef>
#include <vector>
namespace a50 {
struct Settings {
    float gain_db = 0.0f;
    float threshold_dbfs = -38.0f;
    float hold_ms = 120.0f;
    float release_ms = 140.0f;
    bool gate_enabled = true;
    bool highpass_enabled = true;
    bool voice_enabled = false;
    float pitch_semitones = 0.0f;
    float resonance = 0.0f; // -1..1; tonal color, not true formant correction
    float bass_db = 0.0f;
    float brightness_db = 0.0f;
    float effect_mix = 1.0f; // 0..1
};
class Processor {
public:
    explicit Processor(unsigned sample_rate, Settings settings = {});
    void set_settings(Settings settings);
    void process(float* samples, std::size_t count);
    void reset();
private:
    float shift_pitch(float x, float ratio);
    unsigned rate_;
    Settings settings_;
    float previous_input_ = 0.0f;
    float previous_output_ = 0.0f;
    float envelope_ = 0.0f;
    float gate_gain_ = 0.0f;
    std::size_t hold_samples_left_ = 0;
    std::vector<float> ring_;
    std::size_t write_index_ = 0;
    float grain_phase_ = 0.0f;
    float low_bass_ = 0.0f, low_tone_ = 0.0f, low_bright_ = 0.0f;
};
}
