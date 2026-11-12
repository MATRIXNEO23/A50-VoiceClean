// Shared-mode WASAPI microphone acquisition: input and meter only.
// This stage does not alter the Windows microphone, monitor headphones, or create a virtual device.
#include "live_capture_win.h"
#include "audio_samples.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace a50 {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT hr) { if (FAILED(hr)) throw hr; }
struct AudioFormat {
    CaptureFormat samples;
    unsigned sample_rate;
};
AudioFormat determine_format(const WAVEFORMATEX* wf) {
    if (!wf || wf->nChannels < 1 || wf->nChannels > 32 ||
        wf->nSamplesPerSec < 8000 || wf->nSamplesPerSec > 192000)
        throw E_INVALIDARG;
    WORD tag = wf->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        if (wf->cbSize < 22) throw E_INVALIDARG;
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
        // Reject padded 24-in-32 samples until their valid-bit handling is implemented.
        if (ext->Samples.wValidBitsPerSample &&
            ext->Samples.wValidBitsPerSample != wf->wBitsPerSample)
            throw AUDCLNT_E_UNSUPPORTED_FORMAT;
        const GUID pcm = {0x00000001,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
        const GUID ieee_float = {0x00000003,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
        if (IsEqualGUID(ext->SubFormat, pcm)) tag = WAVE_FORMAT_PCM;
        else if (IsEqualGUID(ext->SubFormat, ieee_float)) tag = WAVE_FORMAT_IEEE_FLOAT;
        else throw AUDCLNT_E_UNSUPPORTED_FORMAT;
    }
    SampleEncoding encoding;
    if (tag == WAVE_FORMAT_PCM && wf->wBitsPerSample == 16)
        encoding = SampleEncoding::pcm16;
    else if (tag == WAVE_FORMAT_PCM && wf->wBitsPerSample == 24)
        encoding = SampleEncoding::pcm24;
    else if (tag == WAVE_FORMAT_PCM && wf->wBitsPerSample == 32)
        encoding = SampleEncoding::pcm32;
    else if (tag == WAVE_FORMAT_IEEE_FLOAT && wf->wBitsPerSample == 32)
        encoding = SampleEncoding::float32;
    else throw AUDCLNT_E_UNSUPPORTED_FORMAT;
    if (wf->nBlockAlign != wf->nChannels * sample_bytes(encoding))
        throw AUDCLNT_E_UNSUPPORTED_FORMAT;
    return {{encoding, wf->nChannels}, wf->nSamplesPerSec};
}
} // namespace

std::vector<InputDevice> enumerate_input_devices() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    checked(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                             IID_PPV_ARGS(enumerator.GetAddressOf())));
    ComPtr<IMMDeviceCollection> collection;
    checked(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE,
                                           collection.GetAddressOf()));
    UINT count = 0;
    checked(collection->GetCount(&count));
    std::vector<InputDevice> result;
    result.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        checked(collection->Item(i, device.GetAddressOf()));
        LPWSTR id = nullptr;
        checked(device->GetId(&id));
        std::wstring endpoint_id(id);
        CoTaskMemFree(id);
        ComPtr<IPropertyStore> props;
        checked(device->OpenPropertyStore(STGM_READ, props.GetAddressOf()));
        PROPVARIANT value;
        PropVariantInit(&value);
        const HRESULT hr = props->GetValue(PKEY_Device_FriendlyName, &value);
        std::wstring display = SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal
            ? value.pwszVal : L"Microfono senza nome";
        PropVariantClear(&value);
        result.push_back({std::move(endpoint_id), std::move(display)});
    }
    return result;
}

LiveCapture::~LiveCapture() { stop(); }

void LiveCapture::start(const std::wstring& device_id, Frames on_frames) {
    stop();
    if (device_id.empty()) throw std::invalid_argument("No input device selected");
    error_ = S_OK;
    level_percent_ = 0;
    stop_requested_ = false;
    state_ = CaptureStatus::starting;
    try {
        worker_ = std::thread(&LiveCapture::run, this, device_id, std::move(on_frames));
    } catch (...) {
        stop_requested_ = true;
        state_ = CaptureStatus::failed;
        throw;
    }
}
void LiveCapture::stop() {
    stop_requested_ = true;
    if (worker_.joinable()) worker_.join();
    level_percent_ = 0;
    state_ = CaptureStatus::stopped;
}

void LiveCapture::run(std::wstring device_id, Frames on_frames) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) {
        error_ = initialized;
        state_ = CaptureStatus::failed;
        return;
    }
    ComPtr<IAudioClient> client;
    bool started = false;
    try {
        ComPtr<IMMDeviceEnumerator> enumerator;
        checked(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(enumerator.GetAddressOf())));
        ComPtr<IMMDevice> device;
        checked(enumerator->GetDevice(device_id.c_str(), device.GetAddressOf()));
        checked(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(client.GetAddressOf())));
        WAVEFORMATEX* raw_format = nullptr;
        checked(client->GetMixFormat(&raw_format));
        struct FormatGuard {
            WAVEFORMATEX* p;
            ~FormatGuard() { CoTaskMemFree(p); }
        } guard{raw_format};
        const AudioFormat input = determine_format(raw_format);
        checked(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                   1000000, 0, raw_format, nullptr));
        ComPtr<IAudioCaptureClient> capture;
        checked(client->GetService(IID_PPV_ARGS(capture.GetAddressOf())));
        checked(client->Start());
        started = true;
        state_ = CaptureStatus::running;
        std::vector<float> mono;
        while (!stop_requested_) {
            Sleep(12);
            UINT32 packet = 0;
            checked(capture->GetNextPacketSize(&packet));
            while (packet && !stop_requested_) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                checked(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr));
                try {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        mono.assign(frames, 0.0f);
                    } else {
                        decode_to_mono(data, frames, input.samples, mono);
                    }
                    float power = 0.0f;
                    for (float x : mono) power += x * x;
                    const float rms = mono.empty() ? 0.0f
                        : std::sqrt(power / static_cast<float>(mono.size()));
                    const float dbfs = 20.0f * std::log10(std::max(rms, 1.0e-8f));
                    level_percent_ = std::clamp(
                        static_cast<int>((dbfs + 60.0f) * (100.0f / 60.0f)), 0, 100);
                    if (on_frames && !mono.empty())
                        on_frames(mono.data(), mono.size(), input.sample_rate);
                } catch (...) {
                    capture->ReleaseBuffer(frames);
                    throw;
                }
                checked(capture->ReleaseBuffer(frames));
                checked(capture->GetNextPacketSize(&packet));
            }
        }
        state_ = CaptureStatus::stopped;
    } catch (HRESULT hr) {
        error_ = hr;
        state_ = CaptureStatus::failed;
    } catch (...) {
        error_ = E_FAIL;
        state_ = CaptureStatus::failed;
    }
    if (started) client->Stop();
    CoUninitialize();
}
} // namespace a50
