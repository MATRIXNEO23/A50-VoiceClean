#include "live_buffer.h"
#ifdef NDEBUG
#undef NDEBUG // Keep assertions enabled during Release CI runs.
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    { // Ring buffer overrun must discard oldest samples, never grow indefinitely.
        a50::Queue q(8);
        for(int i=0;i<10;++i) q.push(static_cast<float>(i));
        assert(q.size()==8 && q.lost==2);
        for(int i=2;i<10;++i) assert(q.pop()==static_cast<float>(i));
        assert(q.pop()==0.f && q.underflow==1);
    }
    { // Fixed-length latency limit.
        a50::Queue q(100);
        for(int i=0;i<100;++i) q.push(static_cast<float>(i));
        q.trim_to(25);
        assert(q.size()==25 && q.lost==75 && q.pop()==75.f);
    }
    { // Upsampling 16 kHz -> 48 kHz. 160 intervals should produce 480 frames.
        a50::Queue q(1000);
        a50::Resampler converter(16000,48000);
        for(int i=0;i<=160;++i) converter.feed(static_cast<float>(i)/160.f,q);
        assert(q.size()==480);
        for(int i=0;i<480;++i) {
            float actual=q.pop();
            assert(std::fabs(actual-static_cast<float>(i)/480.f)<0.00001f);
        }
    }
    { // Downsampling 48 kHz -> 16 kHz, sample index timing is preserved.
        a50::Queue q(1000);
        a50::Resampler converter(48000,16000);
        for(int i=0;i<480;++i) converter.feed(static_cast<float>(i)/480.f,q);
        assert(q.size()==160);
        for(int i=0;i<160;++i)
            assert(std::fabs(q.pop()-static_cast<float>(i*3)/480.f)<0.00001f);
    }
    { // Feed boundaries do not cause cumulative drift.
        a50::Queue a(4000),b(4000);
        a50::Resampler whole(16000,44100),streamed(16000,44100);
        std::vector<float> input(601);
        for(std::size_t i=0;i<input.size();++i) input[i]=std::sin(i*0.05f);
        for(float x:input) whole.feed(x,a);
        for(std::size_t i=0;i<230;++i) streamed.feed(input[i],b);
        for(std::size_t i=230;i<input.size();++i) streamed.feed(input[i],b);
        assert(a.size()==b.size());
        while(a.size()) assert(std::fabs(a.pop()-b.pop())<0.000001f);
    }
    { // Invalid config is rejected before touching audio devices.
        bool invalid=false;
        try {a50::Resampler r(0,48000);} catch(const std::invalid_argument&) { invalid=true; }
        assert(invalid);
    }
    std::cout << "Audio buffering and streaming resampling tests passed\n";
}
