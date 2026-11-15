#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "signalsmith-stretch.h"

namespace a50 {
// The timbre slider is a formant offset, independent of fundamental pitch.
// +/- 100% maps to +/- 6 semitones; 35% -> +2.1 st, regardless of pitch.
inline float formant_semitones_from_timbre(float timbre) noexcept {
    return std::isfinite(timbre) ? std::clamp(timbre,-1.f,1.f)*6.f : 0.f;
}
// Spectral pitch and formant shifting, using Signalsmith Stretch (MIT).
// Unlike the old double-grain delay shift, formants can be adjusted separately.
// Frame size: 480 samples / 10 ms @ 48 kHz. Buffers are allocated at setup,
// not for each incoming audio frame. THIS IS NOT a neural voice converter.
class FormantVoice {
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    std::vector<float> dry_delay_;
    std::size_t cursor_=0;
    float mix_=1.f;
    float pitch_=4.f;
    float formants_=4.f;
    float target_pitch_=4.f;
    float target_formants_=4.f;
    float target_mix_=1.f;
public:
    FormantVoice() {
        // 64ms window, 20ms processing interval. Balances CPU and latency on
        // older x86 processors; must be measured on the actual i3-2100.
        stretch_.configure(1,3072,960,true);
        stretch_.setFormantBase(145.f/48000.f);
        stretch_.setTransposeSemitones(pitch_);
        stretch_.setFormantSemitones(formants_);
        const int delay=stretch_.inputLatency()+stretch_.outputLatency();
        dry_delay_.assign(static_cast<std::size_t>(std::max(0,delay))+1,0.f);
    }
    int latency_samples() const noexcept {
        return stretch_.inputLatency()+stretch_.outputLatency();
    }
    void set_settings(float pitch_semitones,float formant_semitones,float wet_mix) {
        if(!std::isfinite(pitch_semitones)||!std::isfinite(formant_semitones)||
           !std::isfinite(wet_mix)) return;
        pitch_semitones=std::clamp(pitch_semitones,-12.f,12.f);
        formant_semitones=std::clamp(formant_semitones,-12.f,12.f);
        // Target values are ramped by process() to avoid instant parameter jumps.
        target_mix_=std::clamp(wet_mix,0.f,1.f);
        target_pitch_=pitch_semitones;
        target_formants_=formant_semitones;
    }
    void reset() {
        stretch_.reset();
        std::fill(dry_delay_.begin(),dry_delay_.end(),0.f);
        cursor_=0;
    }
    void process(std::array<float,480>& frame) {
        // Limit pitch/formant motion to 0.25 semitones per 10-ms frame.
        // This softens slider-induced discontinuities and does not by itself
        // establish perceptual naturalness; headphone listening is still required.
        const float next_pitch=pitch_+
            std::clamp(target_pitch_-pitch_,-0.25f,0.25f);
        const float next_formants=formants_+
            std::clamp(target_formants_-formants_,-0.25f,0.25f);
        if(next_pitch!=pitch_) {
            pitch_=next_pitch;
            stretch_.setTransposeSemitones(pitch_);
        }
        if(next_formants!=formants_) {
            formants_=next_formants;
            stretch_.setFormantSemitones(formants_);
        }
        const float mix_step=(target_mix_-mix_)/480.f;
        // Separate buffers are required by Signalsmith Stretch.
        std::array<float,480> processed{};
        const float* input_ch[1]={frame.data()};
        float* output_ch[1]={processed.data()};
        stretch_.process(input_ch,480,output_ch,480);
        for(std::size_t i=0;i<frame.size();++i) {
            const float dry=std::isfinite(frame[i]) ? frame[i] : 0.f;
            const float delayed=dry_delay_[cursor_];
            dry_delay_[cursor_]=dry;
            if(++cursor_==dry_delay_.size()) cursor_=0;
            const float wet=std::isfinite(processed[i]) ? processed[i] : 0.f;
            mix_+=mix_step;
            frame[i]=std::clamp(mix_*wet+(1.f-mix_)*delayed,-0.98f,0.98f);
        }
    }
};
}
