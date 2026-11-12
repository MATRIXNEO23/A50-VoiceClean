#pragma once
#include <cstddef>
namespace a50 {
struct Settings {
    float gain_db = 0.0f;
    float threshold_dbfs = -38.0f;
    float hold_ms = 120.0f;
    float release_ms = 140.0f;
    bool gate_enabled = true;
    bool highpass_enabled = true;
};
class Processor {
public:
    explicit Processor(unsigned sample_rate, Settings settings = {});
    void set_settings(Settings settings);
    void process(float* samples, std::size_t count); // mono [-1,1] in-place
    void reset();
private:
    unsigned rate_;
    Settings settings_;
    float previous_input_ = 0.0f;
    float previous_output_ = 0.0f;
    float envelope_ = 0.0f;
    float gate_gain_ = 0.0f;
    std::size_t hold_samples_left_ = 0;
};
}
