#include "rnnoise_filter.h"
#include "rnnoise.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace a50 {
RNNoiseFilter::RNNoiseFilter() : state_(rnnoise_create(nullptr)) {
    if(!state_) throw std::runtime_error("RNNoise state initialization failed");
}
RNNoiseFilter::~RNNoiseFilter() {if(state_) rnnoise_destroy(state_);}
bool RNNoiseFilter::feed(float sample,std::array<float,480>& result) {
    if(!std::isfinite(sample)) sample=0.f;
    input_[filled_++]=std::clamp(sample,-1.f,1.f)*32768.f;
    if(filled_ < 480) return false;
    filled_=0;
    voice_probability_=rnnoise_process_frame(state_,denoised_.data(),input_.data());
    if(!std::isfinite(voice_probability_)) voice_probability_=0.f;
    voice_probability_=std::clamp(voice_probability_,0.f,1.f);
    for(std::size_t i=0;i<480;++i) {
        float v=denoised_[i]/32768.f;
        result[i]=std::isfinite(v)?std::clamp(v,-1.f,1.f):0.f;
    }
    return true;
}
}
