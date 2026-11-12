// Step 2: real-time WASAPI capture -> lightweight DSP -> selectable render endpoint.
// Intended for routing to a virtual cable (e.g. CABLE Input), NOT as a virtual driver.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "dsp.h"
#include "live_buffer.h"
#include "rnnoise_filter.h"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmreg.h>
#include <algorithm>
#include <array>
#include <memory>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
std::atomic_bool interrupted{false};
BOOL WINAPI on_console_event(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        interrupted.store(true);
        return TRUE;
    }
    return FALSE;
}
void check(HRESULT hr, const char* action) {
    if (FAILED(hr)) {
        char info[160]{};
        sprintf_s(info, "%s: HRESULT 0x%08lX", action, static_cast<unsigned long>(hr));
        throw std::runtime_error(info);
    }
}
struct Apartment {
    Apartment() { check(CoInitializeEx(nullptr,COINIT_MULTITHREADED), "CoInitializeEx"); }
    ~Apartment() { CoUninitialize(); }
};
struct AudioFormat {
    enum class Kind { pcm16, pcm24, pcm32, f32 };
    Kind kind;
    unsigned rate, channels, bytes, block;
};
struct MixFormat {
    WAVEFORMATEX* ptr = nullptr;
    ~MixFormat() { if (ptr) CoTaskMemFree(ptr); }
    MixFormat(const MixFormat&) = delete;
    MixFormat& operator=(const MixFormat&) = delete;
    MixFormat() = default;
};
AudioFormat inspect(const WAVEFORMATEX* f) {
    if (!f || !f->nChannels || f->nChannels > 32 ||
        f->nSamplesPerSec < 8000 || f->nSamplesPerSec > 192000 ||
        f->nBlockAlign % f->nChannels != 0)
        throw std::runtime_error("Unsupported audio format fields");
    const unsigned bytes = f->nBlockAlign / f->nChannels;
    bool floating = f->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool pcm = f->wFormatTag == WAVE_FORMAT_PCM;
    if (f->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (f->cbSize < 22) throw std::runtime_error("Invalid WAVEFORMATEXTENSIBLE");
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(f);
        floating = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        pcm = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != 0;
    }
    AudioFormat::Kind kind;
    if (floating && f->wBitsPerSample == 32 && bytes == 4) kind=AudioFormat::Kind::f32;
    else if (pcm && f->wBitsPerSample == 16 && bytes == 2) kind=AudioFormat::Kind::pcm16;
    else if (pcm && f->wBitsPerSample == 24 && bytes == 3) kind=AudioFormat::Kind::pcm24;
    else if (pcm && f->wBitsPerSample == 32 && bytes == 4) kind=AudioFormat::Kind::pcm32;
    else throw std::runtime_error("Only PCM16/24/32 and Float32 are supported");
    return {kind, f->nSamplesPerSec, f->nChannels, bytes, f->nBlockAlign};
}
float decode(const BYTE* b, AudioFormat::Kind kind) {
    switch (kind) {
    case AudioFormat::Kind::pcm16: {
        std::int16_t v; std::memcpy(&v,b,2); return v/32768.f;
    }
    case AudioFormat::Kind::pcm24: {
        std::int32_t v = std::int32_t(b[0]) | (std::int32_t(b[1])<<8) | (std::int32_t(b[2])<<16);
        if (v&0x800000) v |= ~0xFFFFFF;
        return v/8388608.f;
    }
    case AudioFormat::Kind::pcm32: {
        std::int32_t v; std::memcpy(&v,b,4); return static_cast<float>(v/2147483648.0);
    }
    case AudioFormat::Kind::f32: {
        float v; std::memcpy(&v,b,4); return std::isfinite(v)?std::clamp(v,-1.f,1.f):0.f;
    }
    }
    return 0.f;
}
void encode(BYTE* b, AudioFormat::Kind kind, float x) {
    x=std::clamp(x,-1.f,1.f);
    switch(kind) {
    case AudioFormat::Kind::pcm16: {
        const std::int16_t v=static_cast<std::int16_t>(std::lround(x*32767.f));
        std::memcpy(b,&v,2); break;
    }
    case AudioFormat::Kind::pcm24: {
        const std::int32_t v=static_cast<std::int32_t>(std::lround(x*8388607.f));
        for(int i=0;i<3;++i) b[i]=static_cast<BYTE>((static_cast<std::uint32_t>(v)>>(8*i))&255u);
        break;
    }
    case AudioFormat::Kind::pcm32: {
        const std::int32_t v=static_cast<std::int32_t>(std::llround(x*2147483647.0));
        std::memcpy(b,&v,4); break;
    }
    case AudioFormat::Kind::f32: std::memcpy(b,&x,4); break;
    }
}
struct Endpoint {
    ComPtr<IMMDevice> device;
    std::wstring name;
};
std::wstring name_of(IMMDevice* device) {
    ComPtr<IPropertyStore> store;
    check(device->OpenPropertyStore(STGM_READ,&store),"OpenPropertyStore");
    PROPVARIANT v;
    PropVariantInit(&v);
    check(store->GetValue(PKEY_Device_FriendlyName,&v),"GetValue");
    std::wstring name=(v.vt==VT_LPWSTR && v.pwszVal)?v.pwszVal:L"(unknown)";
    PropVariantClear(&v);
    return name;
}
std::vector<Endpoint> list(IMMDeviceEnumerator* enumerator,EDataFlow flow) {
    ComPtr<IMMDeviceCollection> coll;
    check(enumerator->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&coll),"EnumAudioEndpoints");
    UINT count=0;
    check(coll->GetCount(&count),"GetCount");
    std::vector<Endpoint> result;
    for(UINT i=0;i<count;++i) {
        Endpoint ep;
        check(coll->Item(i,&ep.device),"Item");
        ep.name=name_of(ep.device.Get());
        result.push_back(std::move(ep));
    }
    return result;
}
std::wstring lowercase(std::wstring s) {
    std::transform(s.begin(),s.end(),s.begin(),
      [](wchar_t x){return static_cast<wchar_t>(std::towlower(x));});
    return s;
}
bool is_virtual_cable(const std::wstring& name) {
    auto n=lowercase(name);
    return n.find(L"cable input")!=std::wstring::npos ||
           n.find(L"vb-audio")!=std::wstring::npos ||
           n.find(L"virtual cable")!=std::wstring::npos;
}
int find_microphone(const std::vector<Endpoint>& eps) {
    for(std::size_t i=0;i<eps.size();++i) {
        auto n=lowercase(eps[i].name);
        if(n.find(L"astro a50")!=std::wstring::npos && n.find(L"voice")!=std::wstring::npos)
            return static_cast<int>(i);
    }
    return -1;
}
int find_virtual_output(const std::vector<Endpoint>& eps) {
    for(std::size_t i=0;i<eps.size();++i)
        if(is_virtual_cable(eps[i].name)) return static_cast<int>(i);
    return -1;
}
using a50::Queue;
using a50::Resampler;
void help() {
    std::wcout <<
      L"A50 VoiceClean — WASAPI live routing diagnostic (step 2)\n"
      L"  A50VoiceClean-Live.exe --list\n"
      L"  A50VoiceClean-Live.exe --input N --output N [--seconds 30]\n"
      L"  [--gain-db N] [--threshold-db N] [--no-gate] [--no-highpass]\n"
      L"  [--allow-speaker-output] [--no-denoise]\n\n"
      L"Defaults: ASTRO A50 Voice input and VB-CABLE/CABLE Input output.\n"
      L"For safety the output must be a virtual cable unless --allow-speaker-output.\n"
      L"Run with --seconds 0 (default) to stop using Ctrl+C.\n"
      L"RNNoise is ON by default; true VAD and voice changing are not live yet.\n";
}
void route(IMMDevice* microphone, IMMDevice* destination,
           a50::Settings settings, int duration, bool denoise_enabled) {
    ComPtr<IAudioClient> input, output;
    check(microphone->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&input),
          "Activate input");
    check(destination->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&output),
          "Activate output");
    MixFormat input_mix, output_mix;
    check(input->GetMixFormat(&input_mix.ptr),"Input GetMixFormat");
    check(output->GetMixFormat(&output_mix.ptr),"Output GetMixFormat");
    const AudioFormat in=inspect(input_mix.ptr),out=inspect(output_mix.ptr);
    std::wcout << L"Input: " << in.rate << L" Hz " << in.channels << L" channel(s); "
               << L"output: " << out.rate << L" Hz " << out.channels << L" channel(s).\n";
    // Shared mode: leave all device properties and driver settings unchanged.
    check(input->Initialize(AUDCLNT_SHAREMODE_SHARED,0,500000,0,input_mix.ptr,nullptr),
          "Initialize input");
    check(output->Initialize(AUDCLNT_SHAREMODE_SHARED,0,500000,0,output_mix.ptr,nullptr),
          "Initialize output");
    ComPtr<IAudioCaptureClient> capture;
    ComPtr<IAudioRenderClient> render;
    check(input->GetService(__uuidof(IAudioCaptureClient),&capture),"GetService capture");
    check(output->GetService(__uuidof(IAudioRenderClient),&render),"GetService render");
    UINT32 out_buffer_frames=0;
    check(output->GetBufferSize(&out_buffer_frames),"GetBufferSize output");
    Queue queue(static_cast<std::size_t>(out.rate)*2);
    Resampler resampler(in.rate,out.rate); // bypass path
    Resampler to_48k(in.rate,48000);
    Resampler from_48k(48000,out.rate);
    Queue pending_48k(48000);
    std::array<float,480> denoised_frame{};
    std::unique_ptr<a50::RNNoiseFilter> denoiser;
    if(denoise_enabled) denoiser=std::make_unique<a50::RNNoiseFilter>();
    a50::Processor dsp(in.rate,settings);
    std::vector<float> block;
    const std::size_t prebuffer=static_cast<std::size_t>(out.rate)*40/1000;
    bool active=false;
    // Keep the render stream alive with silence until the initial 40 ms is available.
    const auto start=std::chrono::steady_clock::now();
    auto last=start;
    std::uint64_t captured=0;
    check(input->Start(),"Start input");
    try {
        check(output->Start(),"Start output");
        try {
            while(!interrupted.load() &&
                  (duration==0 || std::chrono::steady_clock::now()-start <
                    std::chrono::seconds(duration))) {
                UINT32 available=0;
                check(capture->GetNextPacketSize(&available),"GetNextPacketSize");
                while(available) {
                    BYTE* data=nullptr;
                    UINT32 frames=0;
                    DWORD flags=0;
                    check(capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr),"Capture GetBuffer");
                    const bool silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
                    block.resize(frames);
                    for(UINT32 i=0;i<frames;++i) {
                        float mono=0.f;
                        if(!silent) {
                            const BYTE* frame=data+static_cast<std::size_t>(i)*in.block;
                            for(unsigned c=0;c<in.channels;++c)
                                mono+=decode(frame+c*in.bytes,in.kind);
                            mono/=in.channels;
                        }
                        block[i]=mono;
                    }
                    check(capture->ReleaseBuffer(frames),"Capture ReleaseBuffer");
                    dsp.process(block.data(),block.size());
                    if(denoiser) {
                        for(float sample:block) to_48k.feed(sample,pending_48k);
                        while(pending_48k.size()) {
                            if(denoiser->feed(pending_48k.pop(),denoised_frame))
                                for(float sample:denoised_frame)
                                    from_48k.feed(sample,queue);
                        }
                    } else {
                        for(float sample:block) resampler.feed(sample,queue);
                    }
                    queue.trim_to(static_cast<std::size_t>(out.rate)*200/1000);
                    captured+=frames;
                    check(capture->GetNextPacketSize(&available),"GetNextPacketSize");
                }
                UINT32 padding=0;
                check(output->GetCurrentPadding(&padding),"GetCurrentPadding");
                const UINT32 to_write=out_buffer_frames-padding;
                if(to_write) {
                    BYTE* data=nullptr;
                    check(render->GetBuffer(to_write,&data),"Render GetBuffer");
                    if(!active && queue.size()>=prebuffer) active=true;
                    for(UINT32 i=0;i<to_write;++i) {
                        const float v=active?queue.pop():0.f;
                        BYTE* frame=data+static_cast<std::size_t>(i)*out.block;
                        for(unsigned c=0;c<out.channels;++c)
                            encode(frame+c*out.bytes,out.kind,v);
                    }
                    check(render->ReleaseBuffer(to_write,0),"Render ReleaseBuffer");
                }
                const auto now=std::chrono::steady_clock::now();
                if(now-last>=std::chrono::seconds(3)) {
                    std::wcout << L"Captured frames " << captured
                               << L"; queue " << queue.size()
                               << L"; underruns " << queue.underflow
                               << L"; dropped " << queue.lost << L"\n";
                    last=now;
                }
                Sleep(5);
            }
        } catch (...) {
            output->Stop();
            throw;
        }
        check(output->Stop(),"Stop output");
    } catch (...) {
        input->Stop();
        throw;
    }
    check(input->Stop(),"Stop input");
    std::wcout << L"Terminated. Captured frames " << captured
               << L"; underflows " << queue.underflow
               << L"; overflows " << queue.lost << L"\n";
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        int selected_in=-1,selected_out=-1,seconds=0;
        bool list_only=false,allow_speakers=false,denoise_enabled=true;
        a50::Settings settings;
        for(int i=1;i<argc;++i) {
            std::wstring option=argv[i];
            if(option==L"--help" || option==L"-h") {help();return 0;}
            if(option==L"--list") list_only=true;
            else if(option==L"--allow-speaker-output") allow_speakers=true;
            else if(option==L"--no-gate") settings.gate_enabled=false;
            else if(option==L"--no-denoise") denoise_enabled=false;
            else if(option==L"--no-highpass") settings.highpass_enabled=false;
            else if(option==L"--input" && i+1<argc) selected_in=std::stoi(argv[++i]);
            else if(option==L"--output" && i+1<argc) selected_out=std::stoi(argv[++i]);
            else if(option==L"--seconds" && i+1<argc) seconds=std::stoi(argv[++i]);
            else if(option==L"--gain-db" && i+1<argc) settings.gain_db=std::stof(argv[++i]);
            else if(option==L"--threshold-db" && i+1<argc) settings.threshold_dbfs=std::stof(argv[++i]);
            else throw std::invalid_argument("Unknown or incomplete option");
        }
        if(seconds<0 || seconds>3600) throw std::invalid_argument("seconds must be 0..3600");
        Apartment apartment;
        ComPtr<IMMDeviceEnumerator> enumerator;
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,
              IID_PPV_ARGS(&enumerator)),"CoCreateInstance");
        const auto ins=list(enumerator.Get(),eCapture);
        const auto outs=list(enumerator.Get(),eRender);
        std::wcout << L"Capture inputs:\n";
        for(std::size_t i=0;i<ins.size();++i)
            std::wcout << L"  [" << i << L"] " << ins[i].name << L"\n";
        std::wcout << L"Render outputs:\n";
        for(std::size_t i=0;i<outs.size();++i)
            std::wcout << L"  [" << i << L"] " << outs[i].name << L"\n";
        if(list_only) return 0;
        if(selected_in == -1) selected_in=find_microphone(ins);
        if(selected_out == -1) selected_out=find_virtual_output(outs);
        if(selected_in<0 || static_cast<std::size_t>(selected_in)>=ins.size())
            throw std::runtime_error("ASTRO A50 Voice input not found. Run --list.");
        if(selected_out<0 || static_cast<std::size_t>(selected_out)>=outs.size())
            throw std::runtime_error("Virtual cable output not found. Install/configure VB-CABLE or use --list.");
        if(!is_virtual_cable(outs[selected_out].name) && !allow_speakers)
            throw std::runtime_error("Refusing physical speaker output to prevent microphone feedback.");
        std::wcout << L"Input: " << ins[selected_in].name
                   << L"\nOutput: " << outs[selected_out].name << L"\n";
        if(!SetConsoleCtrlHandler(on_console_event,TRUE))
            throw std::runtime_error("Could not register Ctrl+C handler");
        route(ins[selected_in].device.Get(),outs[selected_out].device.Get(),settings,seconds,denoise_enabled);
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "A50 VoiceClean live route: " << e.what() << "\n";
        return 1;
    }
}
