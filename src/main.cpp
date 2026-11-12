#include "transform.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void usage() {
    std::cout << "A50 VoiceClean v0.2 (WAV offline)\n"
              << "Usage: a50voiceclean input.wav output.wav [--gain-db N] [--threshold-db N] [--no-gate] [--no-highpass]"
              << " [--voice] [--pitch N] [--resonance N] [--bass-db N] [--brightness-db N] [--mix N] [--speed N]\n"
              << "pitch: -12..12 semitones; resonance: -1..1; mix: 0..1; speed (offline): 0.70..1.30.\n";
}
}
int main(int argc, char** argv) {
    if (argc < 3) { usage(); return argc == 1 ? 0 : 2; }
    try {
        a50::Settings s;
        float speed = 1.0f;
        for (int i=3; i<argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--no-gate") s.gate_enabled = false;
            else if (arg == "--no-highpass") s.highpass_enabled = false;
            else if (arg == "--voice") s.voice_enabled = true;
            else if (i+1<argc) {
                const float v = std::stof(argv[++i]);
                if (arg == "--gain-db") s.gain_db = v;
                else if (arg == "--threshold-db") s.threshold_dbfs = v;
                else if (arg == "--pitch") { s.pitch_semitones = v; s.voice_enabled = true; }
                else if (arg == "--resonance") { s.resonance = v; s.voice_enabled = true; }
                else if (arg == "--bass-db") { s.bass_db = v; s.voice_enabled = true; }
                else if (arg == "--brightness-db") { s.brightness_db = v; s.voice_enabled = true; }
                else if (arg == "--mix") { s.effect_mix = v; s.voice_enabled = true; }
                else if (arg == "--speed") speed = v;
                else throw std::invalid_argument("Unknown argument: " + arg);
            } else throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }
        const auto input = a50::read_pcm16_mono_wave(argv[1]);
        const auto output = a50::transform(input, s, speed);
        a50::write_pcm16_mono_wave(argv[2], output);
        std::cout << "Processed " << output.samples.size() << " samples at " << output.sample_rate << " Hz\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
