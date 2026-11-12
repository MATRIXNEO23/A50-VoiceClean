#include "wav.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace a50 {
namespace {
uint16_t u16(const unsigned char* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t u32(const unsigned char* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
void put16(std::ofstream& f, uint16_t n) {
    unsigned char b[2] = {static_cast<unsigned char>(n), static_cast<unsigned char>(n >> 8)};
    f.write(reinterpret_cast<char*>(b), 2);
}
void put32(std::ofstream& f, uint32_t n) {
    unsigned char b[4] = {static_cast<unsigned char>(n),static_cast<unsigned char>(n >> 8),static_cast<unsigned char>(n >> 16),static_cast<unsigned char>(n >> 24)};
    f.write(reinterpret_cast<char*>(b), 4);
}
void require(bool condition, const char* msg) { if (!condition) throw std::runtime_error(msg); }
}
Wave read_pcm16_mono_wave(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    require(bool(f), "Cannot open input WAV");
    unsigned char header[12]{};
    require(bool(f.read(reinterpret_cast<char*>(header), 12)), "WAV header is incomplete");
    require(!std::memcmp(header,"RIFF",4) && !std::memcmp(header+8,"WAVE",4), "Not a RIFF/WAVE file");
    unsigned rate = 0;
    bool fmt_found = false, data_found = false;
    std::vector<unsigned char> raw;
    while (f) {
        unsigned char ch[8]{};
        if (!f.read(reinterpret_cast<char*>(ch), 8)) break;
        const uint32_t length = u32(ch + 4);
        require(length <= 256 * 1024 * 1024, "WAV chunk too large");
        if (!std::memcmp(ch, "fmt ", 4)) {
            require(length >= 16 && length <= 64 * 1024, "Unsupported WAV fmt chunk");
            std::vector<unsigned char> fmt(length);
            require(bool(f.read(reinterpret_cast<char*>(fmt.data()), length)), "Truncated WAV fmt");
            require(u16(fmt.data()) == 1 && u16(fmt.data()+2) == 1 && u16(fmt.data()+14) == 16,
                "Only mono PCM 16-bit WAV is supported");
            rate = u32(fmt.data()+4);
            require(rate >= 8000 && rate <= 192000, "Invalid WAV sample rate");
            fmt_found = true;
        } else if (!std::memcmp(ch, "data", 4)) {
            require(length % 2 == 0, "Odd PCM data length");
            raw.resize(length);
            require(bool(f.read(reinterpret_cast<char*>(raw.data()), length)), "Truncated WAV data");
            data_found = true;
        } else {
            f.seekg(length, std::ios::cur);
            require(bool(f), "Truncated WAV chunk");
        }
        if (length & 1) f.seekg(1, std::ios::cur);
    }
    require(fmt_found && data_found, "WAV fmt or data chunk missing");
    Wave w; w.sample_rate = rate; w.samples.resize(raw.size() / 2);
    for (std::size_t i=0; i<w.samples.size(); ++i) {
        const int16_t v = static_cast<int16_t>(u16(raw.data() + i * 2));
        w.samples[i] = static_cast<float>(v) / 32768.0f;
    }
    return w;
}
void write_pcm16_mono_wave(const std::filesystem::path& path, const Wave& w) {
    require(w.sample_rate >= 8000 && w.sample_rate <= 192000, "Invalid sample rate");
    require(w.samples.size() <= (0xffffffffu - 36u) / 2u, "WAV exceeds RIFF size limit");
    std::ofstream f(path, std::ios::binary);
    require(bool(f), "Cannot create output WAV");
    const auto bytes = static_cast<uint32_t>(w.samples.size() * 2);
    f.write("RIFF",4); put32(f,36 + bytes); f.write("WAVEfmt ",8);
    put32(f,16); put16(f,1); put16(f,1); put32(f,w.sample_rate);
    put32(f, w.sample_rate * 2); put16(f,2); put16(f,16);
    f.write("data",4); put32(f,bytes);
    for (float x: w.samples) {
        int s = static_cast<int>(std::lrint(std::max(-1.0f, std::min(x, 1.0f)) * 32767.0f));
        put16(f, static_cast<uint16_t>(static_cast<int16_t>(s)));
    }
    require(bool(f), "Error writing output WAV");
}
}
