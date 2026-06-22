#pragma once

#include <chrono>
#include <deque>
#include <wx/wx.h>

struct ActivityBucket {
    int session = 0;
    double transfer_mbps = 0.0;
};

class SpeedMonitor : public wxPanel {
public:
    explicit SpeedMonitor(wxWindow* parent);

    void SetRecording(bool recording);
    void EnsureChart();
    void PushSessionPulse();
    void PushSample(double mbps);
    void Clear();
    void SetScaleFloor(double mbps);

private:
    struct TimedBucket {
        ActivityBucket data;
        std::chrono::steady_clock::time_point start{};
    };

    void OnPaint(wxPaintEvent&);
    void EnsureBucket(const std::chrono::steady_clock::time_point& now);
    double TransferScaleMax() const;

    std::deque<TimedBucket> buckets_;
    bool recording_ = false;
    double last_value_ = -1.0;
    std::chrono::steady_clock::time_point last_push_{};
    std::chrono::steady_clock::time_point bucket_start_{};
    double scale_floor_ = 0.0;

    static constexpr size_t kMaxBuckets = 28;
    static constexpr int kBucketMs = 400;
    static constexpr int kMinIntervalMs = 100;
    static constexpr int kMaxSessionPerBucket = 5;
    static constexpr int kMonitorHeight = 56;
};
