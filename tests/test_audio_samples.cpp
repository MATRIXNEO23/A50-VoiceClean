#include "audio_samples.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

int main() {
    using namespace a50;
    std::vector<float> mono;
    const unsigned char stereo16[] = {
        0xff,0x7f, 0x00,0x80, // +32767, -32768 -> approx 0
        0x00,0x40, 0x00,0x40  // +16384, +16384 -> 0.5
    };
    decode_to_mono(stereo16, 2, {SampleEncoding::pcm16, 2}, mono);
    assert(mono.size() == 2);
    assert(std::fabs(mono[0]) < 0.0001f);
    assert(std::fabs(mono[1] - 0.5f) < 0.0001f);

    const unsigned char pcm24[] = {0x00,0x00,0x80, 0xff,0xff,0x7f};
    decode_to_mono(pcm24, 2, {SampleEncoding::pcm24, 1}, mono);
    assert(mono[0] == -1.0f);
    assert(mono[1] > 0.999f);

    const unsigned char pcm32[] = {0x00,0x00,0x00,0x80};
    decode_to_mono(pcm32, 1, {SampleEncoding::pcm32, 1}, mono);
    assert(mono[0] == -1.0f);

    const float floats[] = {-0.25f, 0.75f, 2.0f};
    decode_to_mono(reinterpret_cast<const unsigned char*>(floats), 3,
                   {SampleEncoding::float32, 1}, mono);
    assert(mono.size() == 3);
    assert(mono[0] == -0.25f && mono[1] == 0.75f && mono[2] == 1.0f);

    bool rejected = false;
    try { decode_to_mono(nullptr, 1, {SampleEncoding::pcm16, 1}, mono); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    decode_to_mono(nullptr, 0, {SampleEncoding::pcm16, 1}, mono);
    assert(mono.empty());
}
