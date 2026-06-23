#pragma once

#include "session_rate_stats.h"

#include <chrono>
#include <deque>
#include <wx/wx.h>

struct ActivityBucket {
    int session = 0;
    double transfer_mbps = 0.0;
};

class SpeedMonitorChart;

class SpeedMonitor : public wxPanel {
public:
    explicit SpeedMonitor(wxWindow* parent);

    void SetRecording(bool recording);
    void EnsureChart();
    void PushSessionPulse();
    void PushSample(double mbps);
    void Clear();
    void ClearTransferTrack();
    void SetScaleFloor(double mbps);
    void UpdateHead(double live_mbps, const RateStats& stats);
    /** Height after UpdateHead — use for parent sizer min size. */
    int PreferredHeight() const;

private:
    void OnPaint(wxPaintEvent&);
    void RelayoutHead(double live_mbps, const RateStats& stats);
    void SyncChartSize();

    wxBoxSizer* root_sizer_ = nullptr;
    SpeedMonitorChart* chart_ = nullptr;
    wxPanel* head_panel_ = nullptr;
    wxStaticText* live_value_ = nullptr;
    wxStaticText* live_label_ = nullptr;
    wxStaticText* count_label_ = nullptr;
    wxStaticText* median_value_ = nullptr;
    wxStaticText* median_label_ = nullptr;
    wxStaticText* max_value_ = nullptr;
    wxStaticText* max_label_ = nullptr;
    wxStaticText* avg_value_ = nullptr;
    wxStaticText* avg_label_ = nullptr;
    wxSizerItem* head_item_ = nullptr;
    wxSizerItem* stats_row_item_ = nullptr;
    wxSizerItem* chart_item_ = nullptr;
};

class SpeedMonitorChart : public wxPanel {
public:
    static constexpr int kMonitorHeight = 64;

    explicit SpeedMonitorChart(wxWindow* parent);

    void SetRecording(bool recording);
    void EnsureChart();
    void PushSessionPulse();
    void PushSample(double mbps);
    void Clear();
    void ClearTransferTrack();
    void SetScaleFloor(double mbps);

private:
    void OnPaint(wxPaintEvent&);
    void EnsureBucket(const std::chrono::steady_clock::time_point& now);
    double TransferScaleMax() const;
    void Redraw();

    struct TimedBucket {
        ActivityBucket data;
        std::chrono::steady_clock::time_point start{};
    };

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
};
