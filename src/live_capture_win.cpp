// Windows 10 WASAPI microphone capture diagnostic. It does not route or filter live audio.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        char buffer[128];
        sprintf_s(buffer, "%s (HRESULT 0x%08lX)", operation, static_cast<unsigned long>(hr));
        throw std::runtime_error(buffer);
    }
}
struct ComApartment {
    ComApartment() { checked(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx"); }
    ~ComApartment() { CoUninitialize(); }
};
struct Format {
    enum class Kind { Pcm16, Pcm24, Pcm32, Float32 };
    Kind kind;
    unsigned channels;
    unsigned rate;
    unsigned stride;
    unsigned bytes_per_channel;
};
Format inspect(const WAVEFORMATEX* wave) {
    if (!wave || !wave->nChannels || wave->nChannels > 32 ||
        wave->nSamplesPerSec < 8000 || wave->nSamplesPerSec > 192000 ||
        !wave->nBlockAlign || wave->nBlockAlign % wave->nChannels != 0) {
        throw std::runtime_error("Formato audio WASAPI non valido");
    }
    bool is_float = wave->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool is_pcm = wave->wFormatTag == WAVE_FORMAT_PCM;
    if (wave->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (wave->cbSize < 22) throw std::runtime_error("WAVEFORMATEXTENSIBLE troncato");
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave);
        is_float = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        is_pcm = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != 0;
    }
    const unsigned bytes = wave->nBlockAlign / wave->nChannels;
    Format::Kind kind;
    if (is_float && wave->wBitsPerSample == 32 && bytes == 4) kind = Format::Kind::Float32;
    else if (is_pcm && wave->wBitsPerSample == 16 && bytes == 2) kind = Format::Kind::Pcm16;
    else if (is_pcm && wave->wBitsPerSample == 24 && bytes == 3) kind = Format::Kind::Pcm24;
    else if (is_pcm && wave->wBitsPerSample == 32 && bytes == 4) kind = Format::Kind::Pcm32;
    else throw std::runtime_error("Formato microfono non supportato (PCM16/24/32 o Float32 richiesto)");
    return {kind, wave->nChannels, wave->nSamplesPerSec, wave->nBlockAlign, bytes};
}
float read_sample(const BYTE* p, Format::Kind kind) {
    switch (kind) {
    case Format::Kind::Pcm16: {
        std::int16_t v; std::memcpy(&v, p, sizeof(v)); return v / 32768.f;
    }
    case Format::Kind::Pcm24: {
        std::int32_t v = static_cast<std::int32_t>(
            std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16));
        if (v & 0x00800000) v |= static_cast<std::int32_t>(0xFF000000u);
        return v / 8388608.f;
    }
    case Format::Kind::Pcm32: {
        std::int32_t v; std::memcpy(&v, p, sizeof(v)); return static_cast<float>(v / 2147483648.0);
    }
    case Format::Kind::Float32: {
        float v; std::memcpy(&v, p, sizeof(v)); return std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f;
    }
    }
    return 0.f;
}
void write_le16(std::ofstream& f, std::uint16_t value) {
    const char b[] = {static_cast<char>(value), static_cast<char>(value >> 8)};
    f.write(b, 2);
}
void write_le32(std::ofstream& f, std::uint32_t value) {
    const char b[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                      static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    f.write(b, 4);
}
class WaveWriter {
    std::ofstream f_;
    std::uint64_t samples_ = 0;
    bool finished_ = false;
public:
    WaveWriter(const std::filesystem::path& output, unsigned sample_rate) : f_(output, std::ios::binary) {
        if (!f_) throw std::runtime_error("Impossibile creare il WAV");
        f_.write("RIFF",4); write_le32(f_,0); f_.write("WAVEfmt ",8);
        write_le32(f_,16); write_le16(f_,1); write_le16(f_,1);
        write_le32(f_,sample_rate); write_le32(f_,sample_rate*2);
        write_le16(f_,2); write_le16(f_,16); f_.write("data",4); write_le32(f_,0);
        if (!f_) throw std::runtime_error("Scrittura intestazione WAV fallita");
    }
    void write(float x) {
        if (samples_ >= (0xFFFFFFFFULL - 36ULL)/2ULL)
            throw std::runtime_error("WAV troppo lungo");
        const auto pcm = static_cast<std::int16_t>(std::lround(std::clamp(x,-1.f,1.f)*32767.f));
        write_le16(f_, static_cast<std::uint16_t>(pcm));
        ++samples_;
        if (!f_) throw std::runtime_error("Scrittura dati WAV fallita");
    }
    void finish() {
        if (finished_) return;
        const auto bytes = static_cast<std::uint32_t>(samples_*2ULL);
        f_.seekp(4); write_le32(f_,36u+bytes);
        f_.seekp(40); write_le32(f_,bytes);
        f_.flush();
        if (!f_) throw std::runtime_error("Finalizzazione WAV fallita");
        finished_=true;
    }
    ~WaveWriter() { if (!finished_) { try { finish(); } catch (...) {} } }
    std::uint64_t samples() const { return samples_; }
};
std::wstring device_name(IMMDevice* device) {
    ComPtr<IPropertyStore> store;
    checked(device->OpenPropertyStore(STGM_READ,&store), "OpenPropertyStore");
    PROPVARIANT var;
    PropVariantInit(&var);
    checked(store->GetValue(PKEY_Device_FriendlyName,&var), "GetValue(FriendlyName)");
    const std::wstring name = (var.vt == VT_LPWSTR && var.pwszVal) ? var.pwszVal : L"(senza nome)";
    PropVariantClear(&var);
    return name;
}
struct InputDevice {
    ComPtr<IMMDevice> device;
    std::wstring name;
};
std::vector<InputDevice> inputs(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDeviceCollection> coll;
    checked(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &coll), "EnumAudioEndpoints");
    UINT count=0;
    checked(coll->GetCount(&count), "GetCount");
    std::vector<InputDevice> out;
    for(UINT i=0;i<count;++i) {
        InputDevice d;
        checked(coll->Item(i,&d.device), "Device Item");
        d.name = device_name(d.device.Get());
        out.push_back(std::move(d));
    }
    return out;
}
std::wstring lowercase(std::wstring x) {
    std::transform(x.begin(),x.end(),x.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return x;
}
void usage() {
    std::wcout << L"A50 VoiceClean - diagnostica microfono WASAPI (registrazione live)\n"
      << L"  A50VoiceClean-MicTest.exe --list\n"
      << L"  A50VoiceClean-MicTest.exe --seconds 10 --output prova.wav [--device N]\n"
      << L"Se non specifichi --device, viene cercato ASTRO A50 Voice.\n"
      << L"Questa utility NON modifica la voce e NON fornisce un microfono virtuale.\n";
}
void record(IMMDevice* mic, const std::filesystem::path& output, int seconds) {
    ComPtr<IAudioClient> audio;
    checked(mic->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audio), "Activate IAudioClient");
    WAVEFORMATEX* raw=nullptr;
    checked(audio->GetMixFormat(&raw), "GetMixFormat");
    const Format format = inspect(raw);
    std::wcout << L"Formato ingresso: " << format.rate << L" Hz, " << format.channels
               << L" canale/i, " << format.bytes_per_channel*8 << L" bit.\n";
    const HRESULT init = audio->Initialize(AUDCLNT_SHAREMODE_SHARED,0,1000000,0,raw,nullptr);
    CoTaskMemFree(raw);
    checked(init,"Initialize IAudioClient");
    ComPtr<IAudioCaptureClient> reader;
    checked(audio->GetService(__uuidof(IAudioCaptureClient), &reader), "GetService capture");
    WaveWriter writer(output,format.rate);
    checked(audio->Start(),"Start recording");
    std::wcout << L"Registrazione in corso. Parla nel microfono...\n";
    const auto start=std::chrono::steady_clock::now();
    auto last_meter=start;
    float peak=0.f;
    std::uint64_t packets=0;
    try {
        while (std::chrono::steady_clock::now()-start < std::chrono::seconds(seconds)) {
            UINT32 pending=0;
            checked(reader->GetNextPacketSize(&pending),"GetNextPacketSize");
            while(pending) {
                BYTE* data=nullptr;
                UINT32 count=0;
                DWORD flags=0;
                checked(reader->GetBuffer(&data,&count,&flags,nullptr,nullptr),"GetBuffer");
                const bool silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
                for(UINT32 frame=0;frame<count;++frame) {
                    float y=0;
                    if(!silent) {
                        const BYTE* row=data+static_cast<std::size_t>(frame)*format.stride;
                        for(unsigned ch=0;ch<format.channels;++ch)
                            y+=read_sample(row+ch*format.bytes_per_channel,format.kind);
                        y/=format.channels;
                    }
                    y=std::clamp(y,-1.f,1.f);
                    peak=std::max(peak,std::fabs(y));
                    writer.write(y);
                }
                checked(reader->ReleaseBuffer(count),"ReleaseBuffer");
                ++packets;
                checked(reader->GetNextPacketSize(&pending),"GetNextPacketSize");
            }
            const auto now=std::chrono::steady_clock::now();
            if(now-last_meter >= std::chrono::milliseconds(250)) {
                const float db = peak>0.f ? 20.f*std::log10(peak) : -100.f;
                std::wcout << L"\rLivello picco: " << std::fixed << std::setprecision(1)
                           << db << L" dBFS   " << std::flush;
                peak=0.f;
                last_meter=now;
            }
            Sleep(5);
        }
    } catch (...) {
        audio->Stop();
        throw;
    }
    checked(audio->Stop(),"Stop recording");
    writer.finish();
    std::wcout << L"\nSalvati " << writer.samples() << L" campioni, "
               << packets << L" pacchetti nel file " << output.wstring() << L"\n";
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        int seconds=10;
        int device_index=-1;
        std::filesystem::path output;
        bool list_only=false;
        for(int i=1;i<argc;++i) {
            const std::wstring arg=argv[i];
            if(arg==L"--help" || arg==L"-h") { usage();return 0; }
            if(arg==L"--list") list_only=true;
            else if(arg==L"--seconds" && i+1<argc) seconds=std::stoi(argv[++i]);
            else if(arg==L"--device" && i+1<argc) device_index=std::stoi(argv[++i]);
            else if(arg==L"--output" && i+1<argc) output=argv[++i];
            else throw std::invalid_argument("Argomento non riconosciuto o incompleto");
        }
        if(seconds<1 || seconds>180) throw std::invalid_argument("Durata: da 1 a 180 secondi");
        ComApartment apartment;
        ComPtr<IMMDeviceEnumerator> enumerator;
        checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,
              IID_PPV_ARGS(&enumerator)), "CoCreateInstance");
        const auto devices=inputs(enumerator.Get());
        std::wcout << L"Microfoni attivi (" << devices.size() << L"):\n";
        for(std::size_t i=0;i<devices.size();++i)
            std::wcout << L"  [" << i << L"] " << devices[i].name << L"\n";
        if(list_only) return 0;
        if(output.empty()) { usage(); return 2; }
        if(device_index<0) {
            for(std::size_t i=0;i<devices.size();++i) {
                const auto name=lowercase(devices[i].name);
                if(name.find(L"astro a50")!=std::wstring::npos &&
                   (name.find(L"voice")!=std::wstring::npos ||
                    name.find(L"microfono")!=std::wstring::npos)) {
                    device_index=static_cast<int>(i);
                    break;
                }
            }
            if(device_index<0) throw std::runtime_error(
                "ASTRO A50 Voice non trovato. Esegui --list, poi scegli --device N.");
        }
        if(static_cast<std::size_t>(device_index)>=devices.size())
            throw std::invalid_argument("Indice microfono fuori intervallo");
        std::wcout << L"Selezionato: " << devices[device_index].name << L"\n";
        record(devices[device_index].device.Get(),output,seconds);
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "\nERRORE: " << e.what() << "\n";
        return 1;
    }
}
