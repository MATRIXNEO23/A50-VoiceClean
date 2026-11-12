#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace a50 {
// Single-threaded bounded sample FIFO used by the WASAPI audio pump.
// If full, it discards the oldest audio to protect interactive latency.
class Queue {
    std::vector<float> buf_;
    std::size_t head_ = 0;
    std::size_t length_ = 0;
public:
    std::uint64_t lost = 0;
    std::uint64_t underflow = 0;
    explicit Queue(std::size_t capacity) : buf_(capacity,0.f) {
        if(capacity==0) throw std::invalid_argument("Queue requires positive capacity");
    }
    std::size_t size() const noexcept { return length_; }
    void push(float x) {
        if(length_ == buf_.size()) {
            head_=(head_+1)%buf_.size();
            --length_;
            ++lost;
        }
        buf_[(head_+length_)%buf_.size()]=x;
        ++length_;
    }
    float pop() {
        if(!length_) { ++underflow; return 0.f; }
        float x=buf_[head_];
        head_=(head_+1)%buf_.size();
        --length_;
        return x;
    }
    void trim_to(std::size_t max_frames) {
        if(length_<=max_frames) return;
        const auto dropped=length_-max_frames;
        head_=(head_+dropped)%buf_.size();
        length_=max_frames;
        lost+=dropped;
    }
};

// Streaming linear interpolator. Maintains phase at each packet boundary;
// no accumulating output drift caused by per-packet rounding.
class Resampler {
    double phase_ = 0.0;
    double ratio_;
    float previous_ = 0.f;
    bool have_previous_ = false;
public:
    Resampler(unsigned input_rate,unsigned output_rate)
        : ratio_(output_rate ? static_cast<double>(input_rate)/output_rate : 0.0) {
        if(input_rate==0 || output_rate==0) throw std::invalid_argument("Invalid sample rate");
    }
    void feed(float sample,Queue& output) {
        if(!have_previous_) {previous_=sample;have_previous_=true;return;}
        while(phase_ < 1.0) {
            output.push(previous_ + static_cast<float>(phase_)*(sample-previous_));
            phase_+=ratio_;
        }
        phase_-=1.0;
        previous_=sample;
    }
    void reset() noexcept {phase_=0;previous_=0;have_previous_=false;}
};
} // namespace a50
