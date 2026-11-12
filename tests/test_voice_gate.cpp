#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include "voice_gate.h"
#include <array>
#include <cmath>
#include <iostream>

int main() {
    a50::VoiceGate gate;
    std::array<float,480> out{};
    std::array<float,480> silence{};
    std::array<float,480> voice{};
    voice.fill(0.2f);
    for(int i=0;i<6;++i) assert(!gate.feed(silence,0.f,out)); // startup preroll
    for(int i=0;i<20;++i) {
        assert(gate.feed(silence,0.f,out));
        for(float x:out) assert(x==0.f);
    }
    float max_voice=0.f;
    for(int i=0;i<20;++i) {
        assert(gate.feed(voice,0.95f,out));
        for(float x:out) max_voice=std::max(max_voice,std::fabs(x));
    }
    assert(max_voice>0.15f);
    for(int i=0;i<45;++i) assert(gate.feed(silence,0.f,out));
    float background_output=0.f;
    // Loud non-speech with low RNNoise speech probability must stay muted.
    for(int i=0;i<25;++i) {
        assert(gate.feed(voice,0.05f,out));
        for(float x:out) background_output=std::max(background_output,std::fabs(x));
    }
    assert(background_output<0.003f);
    // Soft near-silence is ignored even if NN probability is high.
    voice.fill(0.00001f);
    float whisper_output=0.f;
    for(int i=0;i<30;++i) {
        assert(gate.feed(voice,0.99f,out));
        for(float x:out) whisper_output=std::max(whisper_output,std::fabs(x));
    }
    assert(whisper_output<0.003f);
    std::cout << "Voice gate probability/floor/attack/preroll tests passed\n";
}
