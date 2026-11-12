#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <utility>

namespace a50 {
// Frame-level 48 kHz voice activity gate. It works on RNNoise's 480-sample
// output plus speech probability, NOT merely on microphone volume.
// Delay provides 60 ms preroll to preserve initial consonants.
class VoiceGate {
public:
    struct Options {
        float probability_threshold = 0.65f; // 0..1
        float minimum_voice_db = -46.f;      // voice RMS floor
        unsigned attack_frames = 2;         // 20 ms confirmation
        unsigned preroll_frames = 6;        // 60 ms look-back
        unsigned hold_frames = 25;          // 250 ms after voice end
        float fade_in_ms = 5.f;
        float fade_out_ms = 60.f;
    };
private:
    struct Buffered {
        std::array<float,480> frame{};
        std::uint64_t index=0;
    };
    Options opt_;
    std::deque<Buffered> pending_;
    std::uint64_t next_index_=0, open_from_=0, open_until_=0;
    unsigned hits_=0;
    bool heard_voice_=false;
    float gain_=0.f;

public:
    explicit VoiceGate(Options options = {}) : opt_(options) {
        if(!std::isfinite(options.probability_threshold) ||
           options.probability_threshold<0.f || options.probability_threshold>1.f ||
           !std::isfinite(options.minimum_voice_db) ||
           options.minimum_voice_db < -95.f || options.minimum_voice_db > 0.f ||
           options.attack_frames<1 || options.attack_frames>20 ||
           options.preroll_frames>30 || options.hold_frames>200 ||
           options.fade_in_ms<0.5f || options.fade_out_ms<0.5f)
            throw std::invalid_argument("VoiceGate parameters outside supported range");
    }
    // Push a 10 ms denoised frame. Returns true when a delayed output frame
    // becomes available, preserving a constant sample rate after startup.
    bool feed(const std::array<float,480>& frame, float speech_probability,
              std::array<float,480>& output) {
        double sum=0.0;
        for(float x:frame) {
            const double valid=std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;
            sum+=valid*valid;
        }
        const float rms=static_cast<float>(std::sqrt(sum/480.0));
        const float floor=std::pow(10.f,opt_.minimum_voice_db/20.f);
        const bool candidate=std::isfinite(speech_probability) &&
             speech_probability>=opt_.probability_threshold && rms>=floor;
        if(candidate) ++hits_;
        else hits_=0;
        if(hits_>=opt_.attack_frames) {
            const auto start=next_index_>opt_.preroll_frames
                ? next_index_-opt_.preroll_frames : 0;
            if(!heard_voice_) {open_from_=start;heard_voice_=true;}
            open_until_=next_index_+opt_.hold_frames;
        }
        pending_.push_back(Buffered{frame,next_index_});
        ++next_index_;
        if(pending_.size()<=opt_.preroll_frames) return false;
        const Buffered old=std::move(pending_.front());
        pending_.pop_front();
        const bool open=heard_voice_ && old.index>=open_from_ &&
                        old.index<=open_until_;
        const float rate=48000.f;
        const float attack=std::exp(-1.f/(opt_.fade_in_ms*0.001f*rate));
        const float release=std::exp(-1.f/(opt_.fade_out_ms*0.001f*rate));
        for(std::size_t i=0;i<480;++i) {
            const float target=open?1.f:0.f;
            const float coefficient=open?attack:release;
            gain_=coefficient*gain_ + (1.f-coefficient)*target;
            const float v=std::isfinite(old.frame[i])?old.frame[i]:0.f;
            output[i]=std::clamp(v*gain_,-1.f,1.f);
        }
        return true;
    }
};
}
