#include "dsp.h"
#include "wav.h"
#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void usage() {
    std::cout << "A50 VoiceClean v0.1 (offline WAV test)\n"
              << "Usage: a50voiceclean input.wav output.wav [--gain-db N] [--threshold-db N] [--no-gate] [--no-highpass]\n"
              << "Input: mono PCM16 WAV. This is NOT yet a live virtual microphone or a voice changer.\n";
}
}
int main(int argc, char** argv) {
    if (argc < 3) { usage(); return argc == 1 ? 0 : 2; }
    try {
        a50::Settings s;
        for (int i=3; i<argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--no-gate") s.gate_enabled = false;
            else if (arg == "--no-highpass") s.highpass_enabled = false;
            else if ((arg == "--gain-db" || arg == "--threshold-db") && i+1 < argc) {
                const float v = std::stof(argv[++i]);
                if (arg == "--gain-db") s.gain_db = v;
                else s.threshold_dbfs = v;
            } else { throw std::invalid_argument("Unknown or incomplete argument: " + arg); }
        }
        auto wave = a50::read_pcm16_mono_wave(argv[1]);
        a50::Processor dsp(wave.sample_rate, s);
        constexpr std::size_t block = 160;
        for (std::size_t i=0; i<wave.samples.size(); i += block)
            dsp.process(wave.samples.data()+i, std::min(block,wave.samples.size()-i));
        a50::write_pcm16_mono_wave(argv[2], wave);
        std::cout << "Processed " << wave.samples.size() << " samples at " << wave.sample_rate << " Hz\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
