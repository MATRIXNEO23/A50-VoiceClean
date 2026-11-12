#pragma once
#ifdef _WIN32
#include <windows.h>
#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace a50 {
struct InputDevice {
    std::wstring id;
    std::wstring name;
};
// Call on a COM-initialized UI thread.
std::vector<InputDevice> enumerate_input_devices();

enum class CaptureStatus { stopped, starting, running, failed };

class LiveCapture {
public:
    using Frames = std::function<void(const float*, std::size_t, unsigned)>;
    LiveCapture() = default;
    ~LiveCapture();
    LiveCapture(const LiveCapture&) = delete;
    LiveCapture& operator=(const LiveCapture&) = delete;
    void start(const std::wstring& device_id, Frames on_frames = {});
    void stop();
    CaptureStatus status() const { return state_.load(); }
    int meter_percent() const { return level_percent_.load(); }
    HRESULT error() const { return error_.load(); }
private:
    void run(std::wstring device_id, Frames on_frames);
    std::atomic<bool> stop_requested_{true};
    std::atomic<CaptureStatus> state_{CaptureStatus::stopped};
    std::atomic<int> level_percent_{0};
    std::atomic<HRESULT> error_{S_OK};
    std::thread worker_;
};
} // namespace a50
#endif
