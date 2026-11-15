#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace a50 {
// Lightweight speech-intelligibility stage after RNNoise (48 kHz / 480 samples).
// No model, dynamic allocation or AVX2. Keeps quiet consonants when RNNoise
// attenuates them, reduces excessive low end and adds modest speech presence.
// Speech-aware AGC boosts low speech without amplifying silent room noise.
// The switch is fully reversible: disabled means the original RNNoise output.
class VoiceClarity {
    bool enabled_ = false;
    float denoise_strength_ = 0.80f; // 0=original signal; 1=full RNNoise
    float smoothed_strength_ = 0.80f;
    float gain_ = 1.f;
    int voice_hold_ = 0;
    float low_ = 0.f, presence_low_ = 0.f, presence_high_ = 0.f;
    static constexpr float pi = 3.14159265358979323846f;
    static float pole(float hz) noexcept {
        return 1.f-std::exp(-2.f*pi*hz/48000.f);
    }
public:
    void set_enabled(bool enabled) noexcept {
        if(enabled_!=enabled) reset();
        enabled_=enabled;
    }
    void set_denoise_strength(float value) noexcept {
        if(std::isfinite(value)) denoise_strength_=std::clamp(value,0.f,1.f);
    }
    void reset() noexcept {
        gain_=1.f;
        smoothed_strength_=denoise_strength_;
        voice_hold_=0;
        low_=presence_low_=presence_high_=0.f;
    }
    void process(const std::array<float,480>& original,
                 const std::array<float,480>& cleaned,
                 float speech_probability,
                 std::array<float,480>& output) noexcept {
        // Independent smoothed wet/dry RNNoise control, even with AGC/EQ off.
        std::array<float,480> mixed{};
        for(std::size_t i=0;i<480;++i) {
            const float raw=std::isfinite(original[i])
                ? std::clamp(original[i],-1.f,1.f) : 0.f;
            const float filtered=std::isfinite(cleaned[i])
                ? std::clamp(cleaned[i],-1.f,1.f) : 0.f;
            smoothed_strength_+=0.0025f*(denoise_strength_-smoothed_strength_);
            mixed[i]=filtered*smoothed_strength_+raw*(1.f-smoothed_strength_);
        }
        if(!enabled_) { output=mixed; return; }
        float sum=0.f;
        for(float sample:original) {
            const float s=std::isfinite(sample)?std::clamp(sample,-1.f,1.f):0.f;
            sum+=s*s;
        }
        const float raw_rms=std::sqrt(sum/480.f);
        const float probability=std::isfinite(speech_probability)
            ? std::clamp(speech_probability,0.f,1.f) : 0.f;
        // Small positive floor avoids waking AGC on electrical/room noise.
        const bool speech=probability>=0.12f && raw_rms>=0.0015f;
        if(speech) voice_hold_=30; // Hold through s/f/t consonants for ~300 ms.
        else if(voice_hold_>0) --voice_hold_;
        // AGC/EQ does not control the RNNoise wet/dry amount.
        const float low_coeff=pole(210.f);
        const float presence_low_coeff=pole(1600.f);
        const float presence_high_coeff=pole(4300.f);
        float clean_sum=0.f, peak=0.f;
        for(std::size_t i=0;i<480;++i) {
            const float x=mixed[i];
            low_+=low_coeff*(x-low_);
            presence_low_+=presence_low_coeff*(x-presence_low_);
            presence_high_+=presence_high_coeff*(x-presence_high_);
            // Gentle low shelf (~ -2 dB) reduces boom; broad consonant
            // presence (1.6-4.3 kHz) is boosted without a pitch shift.
            const float y=x-0.22f*low_+0.32f*(presence_high_-presence_low_);
            output[i]=y;
            clean_sum+=y*y;
            peak=std::max(peak,std::fabs(y));
        }
        const float clean_rms=std::sqrt(clean_sum/480.f);
        // Only raise actual likely voice. RNNoise is not a perfect VAD:
        // held activity prevents dips on soft unvoiced consonants.
        const float desired=(voice_hold_>0 && raw_rms>=0.0015f)
            ? std::clamp(0.065f/std::max(clean_rms,0.001f),1.f,7.0f)
            : 1.f;
        const float speed=desired>gain_?0.12f:0.40f;
        gain_+=speed*(desired-gain_);
        // Peak-aware limiting prevents AGC from amplifying into clipping.
        const float limited=std::min(gain_,0.90f/std::max(peak,0.00001f));
        for(float& sample:output)
            sample=std::clamp(sample*limited,-0.90f,0.90f);
    }
};
}
