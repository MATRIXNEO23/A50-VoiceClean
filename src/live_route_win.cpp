// Step 2: real-time WASAPI capture -> lightweight DSP -> selectable render endpoint.
// Intended for routing to a virtual cable (e.g. CABLE Input), NOT as a virtual driver.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "dsp.h"
#include "live_buffer.h"
#include "rnnoise_filter.h"
#include "voice_gate.h"
#include "voice_clarity.h"
#include "live_shared.h"
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
HANDLE external_stop_event=nullptr;
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
    std::wstring id;
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
        LPWSTR device_id=nullptr;
        check(ep.device->GetId(&device_id),"Endpoint GetId");
        ep.id=device_id?device_id:L"";
        CoTaskMemFree(device_id);
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
// Second WASAPI render sink. The virtual microphone keeps working independently.
class HeadphoneMonitor {
    ComPtr<IAudioClient> client_;
    ComPtr<IAudioRenderClient> render_;
    AudioFormat fmt_{AudioFormat::Kind::f32,48000,2,4,8};
    UINT32 frames_=0;
    std::unique_ptr<Queue> fifo_;
    std::unique_ptr<Resampler> resampler_;
    bool started_=false, primed_=false, enabled_=false;
    float volume_=0.35f, gain_=0.f;
public:
    HeadphoneMonitor(IMMDevice* endpoint,unsigned source_rate) {
        check(endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&client_),
              "Activate headphone monitor");
        MixFormat mix;
        check(client_->GetMixFormat(&mix.ptr),"Headphone mix format");
        fmt_=inspect(mix.ptr);
        check(client_->Initialize(AUDCLNT_SHAREMODE_SHARED,0,500000,0,mix.ptr,nullptr),
              "Initialize headphone monitor");
        check(client_->GetService(__uuidof(IAudioRenderClient),&render_),
              "Headphone render service");
        check(client_->GetBufferSize(&frames_),"Headphone buffer size");
        fifo_=std::make_unique<Queue>(static_cast<std::size_t>(fmt_.rate));
        resampler_=std::make_unique<Resampler>(source_rate,fmt_.rate);
    }
    void start() {
        check(client_->Start(),"Start headphone monitor");
        started_=true;
    }
    ~HeadphoneMonitor() {
        if(started_) client_->Stop();
    }
    void settings(bool enabled,float volume) {
        enabled_=enabled;
        volume_=std::clamp(volume,0.f,1.f);
    }
    void feed(float sample) {
        resampler_->feed(sample,*fifo_);
        fifo_->trim_to(static_cast<std::size_t>(fmt_.rate)*200/1000);
    }
    void pump() {
        UINT32 padding=0;
        check(client_->GetCurrentPadding(&padding),"Headphone padding");
        const UINT32 free_frames=frames_-padding;
        if(!free_frames)return;
        BYTE* data=nullptr;
        check(render_->GetBuffer(free_frames,&data),"Headphone GetBuffer");
        if(!primed_ && fifo_->size()>=static_cast<std::size_t>(fmt_.rate)*40/1000)
            primed_=true;
        for(UINT32 i=0;i<free_frames;++i) {
            // Pop even when muted: resuming the monitor must not replay stale audio.
            const float sample=primed_?fifo_->pop():0.f;
            const float target=enabled_?volume_:0.f;
            // Smooth 15 ms ramp avoids pops when switching or moving slider.
            const float coefficient=1.f-std::exp(-1.f/(0.015f*fmt_.rate));
            gain_+=(target-gain_)*coefficient;
            const float output=sample*gain_;
            BYTE* frame=data+static_cast<std::size_t>(i)*fmt_.block;
            for(unsigned ch=0;ch<fmt_.channels;++ch)
                encode(frame+ch*fmt_.bytes,fmt_.kind,output);
        }
        check(render_->ReleaseBuffer(free_frames,0),"Headphone ReleaseBuffer");
    }
};

LONG meter_units(float level) {
    if(!std::isfinite(level)||level<=0.00001f) return 0;
    const float db=20.f*std::log10(std::clamp(level,0.00001f,1.f));
    return static_cast<LONG>(std::lround(
        std::clamp((db+70.f)/70.f,0.f,1.f)*1000.f));
}
void help() {
    std::wcout <<
      L"A50 VoiceClean — WASAPI live routing diagnostic (step 2)\n"
      L"  A50VoiceClean-Live.exe --list\n"
      L"  A50VoiceClean-Live.exe --input N --output N [--seconds 30]\n"
      L"  A50VoiceClean-Live.exe --input-id ID --output-id ID\n"
      L"  [--gain-db N] [--threshold-db N] [--no-gate] [--no-highpass]\n"
      L"  [--allow-speaker-output] [--no-denoise] [--no-vad]\n"
      L"  [--vad-prob 0.65] [--voice-min-db -46] [--stop-event NAME]\n"
      L"  [--config-map NAME] (Windows native control panel)\n"
      L"  [--voice] [--pitch 4] [--resonance 0.35] [--bass-db -3]\n"
      L"  [--brightness-db 2] [--voice-mix 1.0]\n"
      L"  [--monitor-id ID] [--monitor-on] [--monitor-volume 0.35]\n"
      L"  [--headphone-preview] (headset only, without VB-CABLE)\n"
      L"  [--no-clarity] [--denoise-strength 0.80] (0..1, quiet speech)\n\n"
      L"Defaults: ASTRO A50 Voice input and VB-CABLE/CABLE Input output.\n"
      L"For safety the output must be a virtual cable unless --allow-speaker-output.\n"
      L"Run with --seconds 0 (default) to stop using Ctrl+C.\n"
      L"RNNoise and VAD are ON by default. Voice DSP is optional.\n"
      L"Live tempo/speed changes are NOT supported: use WAV editor for speed.\n";
}
void route(IMMDevice* microphone, IMMDevice* destination,
           a50::Settings settings, int duration, bool denoise_enabled,
           bool vad_enabled, a50::VoiceGate::Options vad_options,
           a50::Settings voice_settings,a50::LiveShared* live_config,
           IMMDevice* monitor_device,bool preview_only,bool monitor_initially_on,
           float initial_monitor_volume, bool clarity_initially_on,
           float initial_denoise_strength) {
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
    std::unique_ptr<HeadphoneMonitor> headphones;
    if(monitor_device) headphones=std::make_unique<HeadphoneMonitor>(monitor_device,out.rate);
    bool monitor_enabled=monitor_initially_on;
    float monitor_volume=std::clamp(initial_monitor_volume,0.f,1.f);
    if(headphones) headphones->settings(monitor_enabled,monitor_volume);
    Resampler resampler(in.rate,out.rate); // bypass path
    Resampler to_48k(in.rate,48000);
    Resampler from_48k(48000,out.rate);
    Queue pending_48k(48000);
    std::array<float,480> denoised_frame{};
    std::array<float,480> clear_frame{};
    std::array<float,480> gated_frame{};
    std::unique_ptr<a50::VoiceGate> vad;
    if(vad_enabled) vad=std::make_unique<a50::VoiceGate>(vad_options);
    std::unique_ptr<a50::RNNoiseFilter> denoiser;
    if(denoise_enabled) denoiser=std::make_unique<a50::RNNoiseFilter>();
    a50::VoiceClarity clarity;
    clarity.set_enabled(clarity_initially_on);
    clarity.set_denoise_strength(initial_denoise_strength);
    a50::Processor dsp(in.rate,settings);
    // Voice DSP follows RNNoise/VAD so the noise model sees original speech.
    a50::Processor voice_48k(48000,voice_settings);
    a50::Processor voice_input_rate(in.rate,voice_settings);
    std::vector<float> block;
    const std::size_t prebuffer=static_cast<std::size_t>(out.rate)*40/1000;
    bool active=false;
    // Keep the render stream alive with silence until the initial 40 ms is available.
    const auto start=std::chrono::steady_clock::now();
    auto last=start;
    std::uint64_t captured=0;
    float input_meter=0.f, output_meter=0.f;
    LONG last_settings_generation=-1;
    float preview_gain=0.f;
    check(input->Start(),"Start input");
    try {
        check(output->Start(),"Start output");
        if(headphones) headphones->start();
        try {
            while(!interrupted.load() &&
                  !(external_stop_event && WaitForSingleObject(external_stop_event,0)==WAIT_OBJECT_0) &&
                  (duration==0 || std::chrono::steady_clock::now()-start <
                    std::chrono::seconds(duration))) {
                if(live_config) {
                    a50::LiveValues vals;
                    LONG generation=0;
                    if(a50::snapshot(live_config,vals,generation) &&
                       generation!=last_settings_generation) {
                        last_settings_generation=generation;
                        settings.gain_db=std::clamp(vals.gain_tenths/10.f,-24.f,24.f);
                        dsp.set_settings(settings);
                        voice_settings.voice_enabled=vals.voice_enabled!=0;
                        voice_settings.pitch_semitones=std::clamp(vals.pitch_tenths/10.f,-12.f,12.f);
                        voice_settings.resonance=std::clamp(vals.resonance_milli/1000.f,-1.f,1.f);
                        voice_settings.bass_db=std::clamp(vals.bass_tenths/10.f,-18.f,18.f);
                        voice_settings.brightness_db=std::clamp(vals.brightness_tenths/10.f,-18.f,18.f);
                        voice_settings.effect_mix=std::clamp(vals.mix_milli/1000.f,0.f,1.f);
                        voice_48k.set_settings(voice_settings);
                        voice_input_rate.set_settings(voice_settings);
                        if(vad) vad->set_sensitivity(
                            std::clamp(vals.probability_milli/1000.f,0.f,1.f),
                            std::clamp(vals.min_voice_db_tenths/10.f,-90.f,0.f));
                        monitor_enabled=vals.monitor_enabled!=0;
                        monitor_volume=std::clamp(vals.monitor_volume_milli/1000.f,0.f,1.f);
                        if(headphones) headphones->settings(monitor_enabled,monitor_volume);
                        clarity.set_enabled(vals.clarity_enabled!=0);
                        clarity.set_denoise_strength(std::clamp(vals.denoise_strength_milli/1000.f,0.f,1.f));
                    }
                }
                float input_peak=0.f,output_peak=0.f;
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
                        input_peak=std::max(input_peak,std::fabs(mono));
                    }
                    check(capture->ReleaseBuffer(frames),"Capture ReleaseBuffer");
                    dsp.process(block.data(),block.size());
                    if(denoiser) {
                        for(float sample:block) to_48k.feed(sample,pending_48k);
                        while(pending_48k.size()) {
                            if(denoiser->feed(pending_48k.pop(),denoised_frame)) {
                                clarity.process(denoiser->last_original_frame(),
                                                denoised_frame,
                                                denoiser->last_voice_probability(),clear_frame);
                                if(vad) {
                                    if(vad->feed(clear_frame,
                                                 denoiser->last_voice_probability(),
                                                 gated_frame)) {
                                        if(voice_settings.voice_enabled)
                                            voice_48k.process(gated_frame.data(),gated_frame.size());
                                        for(float sample:gated_frame)
                                            from_48k.feed(sample,queue);
                                    }
                                } else {
                                    if(voice_settings.voice_enabled)
                                        voice_48k.process(clear_frame.data(),clear_frame.size());
                                    for(float sample:clear_frame)
                                        from_48k.feed(sample,queue);
                                }
                            }
                        }
                    } else {
                        if(voice_settings.voice_enabled)
                            voice_input_rate.process(block.data(),block.size());
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
                        output_peak=std::max(output_peak,std::fabs(v));
                        if(headphones) headphones->feed(v);
                        const float preview_target=monitor_enabled?monitor_volume:0.f;
                        if(preview_only)
                            preview_gain+=(preview_target-preview_gain)*
                                 (1.f-std::exp(-1.f/(0.015f*out.rate)));
                        const float routed=preview_only?v*preview_gain:v;
                        BYTE* frame=data+static_cast<std::size_t>(i)*out.block;
                        for(unsigned c=0;c<out.channels;++c)
                            encode(frame+c*out.bytes,out.kind,routed);
                    }
                    check(render->ReleaseBuffer(to_write,0),"Render ReleaseBuffer");
                }
                if(headphones) headphones->pump();
                if(live_config) {
                    input_meter=std::max(input_peak,input_meter*0.96f);
                    output_meter=std::max(output_peak,output_meter*0.96f);
                    InterlockedExchange(&live_config->meter_input_db,meter_units(input_meter));
                    InterlockedExchange(&live_config->meter_output_db,meter_units(output_meter));
                    const float p=denoiser?denoiser->last_voice_probability():0.f;
                    InterlockedExchange(&live_config->voice_probability_milli,
                                        static_cast<LONG>(std::lround(p*1000.f)));
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
        bool list_only=false,allow_speakers=false,denoise_enabled=true,vad_enabled=true;
        bool preview_only=false,monitor_initially_on=false;
        bool clarity_initially_on=true;
        float denoise_strength=0.80f;
        float monitor_volume=0.35f;
        std::wstring monitor_id;
        a50::VoiceGate::Options vad_options;
        a50::Settings voice_settings;
        voice_settings.highpass_enabled=false;
        voice_settings.gate_enabled=false;
        voice_settings.gain_db=0.f;
        std::wstring stop_event_name,config_map_name,input_device_id,output_device_id;
        a50::Settings settings;
        // Do not use the legacy -38 dB amplitude gate: it drops quiet speech
        // and initial consonants, even with VAD switched OFF.
        settings.gate_enabled=false;
        for(int i=1;i<argc;++i) {
            std::wstring option=argv[i];
            if(option==L"--help" || option==L"-h") {help();return 0;}
            if(option==L"--list") list_only=true;
            else if(option==L"--allow-speaker-output") allow_speakers=true;
            else if(option==L"--headphone-preview") preview_only=true;
            else if(option==L"--monitor-on") monitor_initially_on=true;
            else if(option==L"--monitor-volume" && i+1<argc) monitor_volume=std::stof(argv[++i]);
            else if(option==L"--monitor-id" && i+1<argc) monitor_id=argv[++i];
            else if(option==L"--no-gate") settings.gate_enabled=false;
            else if(option==L"--no-denoise") denoise_enabled=false;
            else if(option==L"--no-clarity") clarity_initially_on=false;
            else if(option==L"--denoise-strength" && i+1<argc) denoise_strength=std::stof(argv[++i]);
            else if(option==L"--no-vad") vad_enabled=false;
            else if(option==L"--voice") voice_settings.voice_enabled=true;
            else if(option==L"--pitch" && i+1<argc) voice_settings.pitch_semitones=std::stof(argv[++i]);
            else if(option==L"--resonance" && i+1<argc) voice_settings.resonance=std::stof(argv[++i]);
            else if(option==L"--bass-db" && i+1<argc) voice_settings.bass_db=std::stof(argv[++i]);
            else if(option==L"--brightness-db" && i+1<argc) voice_settings.brightness_db=std::stof(argv[++i]);
            else if(option==L"--voice-mix" && i+1<argc) voice_settings.effect_mix=std::stof(argv[++i]);
            else if(option==L"--stop-event" && i+1<argc) stop_event_name=argv[++i];
            else if(option==L"--config-map" && i+1<argc) config_map_name=argv[++i];
            else if(option==L"--vad-prob" && i+1<argc) vad_options.probability_threshold=std::stof(argv[++i]);
            else if(option==L"--voice-min-db" && i+1<argc) vad_options.minimum_voice_db=std::stof(argv[++i]);
            else if(option==L"--no-highpass") settings.highpass_enabled=false;
            else if(option==L"--input" && i+1<argc) selected_in=std::stoi(argv[++i]);
            else if(option==L"--input-id" && i+1<argc) input_device_id=argv[++i];
            else if(option==L"--output" && i+1<argc) selected_out=std::stoi(argv[++i]);
            else if(option==L"--output-id" && i+1<argc) output_device_id=argv[++i];
            else if(option==L"--seconds" && i+1<argc) seconds=std::stoi(argv[++i]);
            else if(option==L"--gain-db" && i+1<argc) settings.gain_db=std::stof(argv[++i]);
            else if(option==L"--threshold-db" && i+1<argc) settings.threshold_dbfs=std::stof(argv[++i]);
            else throw std::invalid_argument("Unknown or incomplete option");
        }
        if(seconds<0 || seconds>3600) throw std::invalid_argument("seconds must be 0..3600");
        if(!std::isfinite(denoise_strength)||denoise_strength<0.f||denoise_strength>1.f)
            throw std::invalid_argument("denoise-strength must be 0..1");
        if(!std::isfinite(monitor_volume)||monitor_volume<0.f||monitor_volume>1.f)
            throw std::invalid_argument("Monitor volume must be between 0 and 1");
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
        // Stable endpoint IDs prevent misrouting if Windows changes device order.
        if(!input_device_id.empty()) {
            selected_in=-1;
            for(std::size_t i=0;i<ins.size();++i)
                if(ins[i].id==input_device_id) selected_in=static_cast<int>(i);
            if(selected_in<0) throw std::runtime_error("Selected microphone disconnected");
        } else if(selected_in == -1) selected_in=find_microphone(ins);
        if(!output_device_id.empty()) {
            selected_out=-1;
            for(std::size_t i=0;i<outs.size();++i)
                if(outs[i].id==output_device_id) selected_out=static_cast<int>(i);
            if(selected_out<0) throw std::runtime_error("Selected output disconnected");
        } else if(selected_out == -1) selected_out=find_virtual_output(outs);
        if(selected_in<0 || static_cast<std::size_t>(selected_in)>=ins.size())
            throw std::runtime_error("ASTRO A50 Voice input not found. Run --list.");
        if(selected_out<0 || static_cast<std::size_t>(selected_out)>=outs.size())
            throw std::runtime_error("Virtual cable output not found. Install/configure VB-CABLE or use --list.");
        const auto astro_headphones=[](const std::wstring& name) {
            const auto n=lowercase(name);
            return n.find(L"astro a50")!=std::wstring::npos &&
                   n.find(L"game")!=std::wstring::npos;
        };
        if(preview_only) {
            if(!astro_headphones(outs[selected_out].name))
                throw std::runtime_error("Preview output must be ASTRO A50 Game headphones");
        } else if(!is_virtual_cable(outs[selected_out].name) && !allow_speakers) {
            throw std::runtime_error("Refusing physical speaker output to prevent microphone feedback.");
        }
        IMMDevice* monitor_device=nullptr;
        if(!monitor_id.empty() && !preview_only) {
            for(const auto& device:outs)
                if(device.id==monitor_id) {
                    if(!astro_headphones(device.name))
                        throw std::runtime_error("Live monitor requires ASTRO A50 Game headphones");
                    if(device.id==outs[selected_out].id)
                        throw std::runtime_error("Monitor and virtual output must differ");
                    monitor_device=device.device.Get();
                    break;
                }
            if(!monitor_device)
                throw std::runtime_error("Selected ASTRO headphone monitor disconnected");
        }
        std::wcout << L"Input: " << ins[selected_in].name
                   << L"\nOutput: " << outs[selected_out].name << L"\n";
        if(!stop_event_name.empty()) {
            external_stop_event=OpenEventW(SYNCHRONIZE,FALSE,stop_event_name.c_str());
            if(!external_stop_event) throw std::runtime_error("Cannot open GUI stop event");
        }
        if(!SetConsoleCtrlHandler(on_console_event,TRUE))
            throw std::runtime_error("Could not register Ctrl+C handler");
        if(!denoise_enabled) vad_enabled=false;
        // The old amplitude gate is disabled by default, independently of VAD.
        struct MappedControl {
            HANDLE mapping=nullptr;
            a50::LiveShared* config=nullptr;
            ~MappedControl() {
                if(config) UnmapViewOfFile(config);
                if(mapping) CloseHandle(mapping);
            }
        } mapped;
        if(!config_map_name.empty()) {
            mapped.mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,config_map_name.c_str());
            if(!mapped.mapping) throw std::runtime_error("Cannot open GUI config mapping");
            mapped.config=static_cast<a50::LiveShared*>(MapViewOfFile(
                mapped.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(a50::LiveShared)));
            if(!mapped.config) throw std::runtime_error("Cannot map GUI config");
        }
        route(ins[selected_in].device.Get(),outs[selected_out].device.Get(),settings,seconds,
              denoise_enabled,vad_enabled,vad_options,voice_settings,mapped.config,
              monitor_device,preview_only,monitor_initially_on,monitor_volume,
              clarity_initially_on,denoise_strength);
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "A50 VoiceClean live route: " << e.what() << "\n";
        return 1;
    }
}
