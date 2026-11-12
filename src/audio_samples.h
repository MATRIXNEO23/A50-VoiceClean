#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace a50 {
enum class SampleEncoding { pcm16, pcm24, pcm32, float32 };
struct CaptureFormat {
    SampleEncoding encoding = SampleEncoding::pcm16;
    unsigned channels = 1;
};

inline std::size_t sample_bytes(SampleEncoding encoding) {
    switch (encoding) {
        case SampleEncoding::pcm16: return 2;
        case SampleEncoding::pcm24: return 3;
        case SampleEncoding::pcm32:
        case SampleEncoding::float32: return 4;
    }
    throw std::invalid_argument("Unsupported sample encoding");
}

inline float decode_sample(const unsigned char* p, SampleEncoding encoding) {
    switch (encoding) {
        case SampleEncoding::pcm16: {
            const auto bits = static_cast<std::uint16_t>(p[0])
                | (static_cast<std::uint16_t>(p[1]) << 8);
            const auto value = static_cast<std::int16_t>(bits);
            return static_cast<float>(value) / 32768.0f;
        }
        case SampleEncoding::pcm24: {
            std::int32_t value = static_cast<std::int32_t>(p[0])
                | (static_cast<std::int32_t>(p[1]) << 8)
                | (static_cast<std::int32_t>(p[2]) << 16);
            if (value & 0x800000) value |= ~0xffffff;
            return static_cast<float>(value) / 8388608.0f;
        }
        case SampleEncoding::pcm32: {
            const auto bits = static_cast<std::uint32_t>(p[0])
                | (static_cast<std::uint32_t>(p[1]) << 8)
                | (static_cast<std::uint32_t>(p[2]) << 16)
                | (static_cast<std::uint32_t>(p[3]) << 24);
            std::int32_t value;
            std::memcpy(&value, &bits, sizeof(value));
            return static_cast<float>(static_cast<double>(value) / 2147483648.0);
        }
        case SampleEncoding::float32: {
            float value;
            std::memcpy(&value, p, sizeof(value));
            return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
        }
    }
    throw std::invalid_argument("Unsupported sample encoding");
}

// WASAPI shared-mode frames are interleaved. Return normalized, mono samples.
// The input format's sample rate is preserved; no resampling takes place here.
inline void decode_to_mono(const unsigned char* data, std::size_t frames,
                           CaptureFormat format, std::vector<float>& mono) {
    if (format.channels < 1 || format.channels > 32)
        throw std::invalid_argument("Invalid channel count");
    if (!data && frames) throw std::invalid_argument("Null audio data");
    const auto stride = sample_bytes(format.encoding);
    if (frames > mono.max_size()) throw std::length_error("Too many frames");
    mono.resize(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float sum = 0.0f;
        for (unsigned channel = 0; channel < format.channels; ++channel) {
            const auto offset = (frame * format.channels + channel) * stride;
            sum += decode_sample(data + offset, format.encoding);
        }
        mono[frame] = sum / static_cast<float>(format.channels);
    }
}
}  // namespace a50
