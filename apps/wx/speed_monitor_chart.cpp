#include "speed_monitor.h"

#include <algorithm>
#include <cmath>
#include <wx/dcbuffer.h>

namespace {

const wxColour kMonitorBg(0x0f, 0x16, 0x20);
const wxColour kSessionBar(0x5b, 0x9f, 0xd4);
const wxColour kTransferBar(0x00, 0xd4, 0xaa);
const wxColour kLegendText(0x88, 0x99, 0xaa);

}  // namespace

SpeedMonitorChart::SpeedMonitorChart(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , timer_(this) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kMonitorBg);
    SetMinSize(wxSize(-1, kMonitorHeight));
    SetMaxSize(wxSize(-1, kMonitorHeight));
    Bind(wxEVT_PAINT, &SpeedMonitorChart::OnPaint, this);
    Bind(wxEVT_TIMER, &SpeedMonitorChart::OnTimer, this);
}

SpeedMonitorChart::~SpeedMonitorChart() {
    timer_.Stop();
}

void SpeedMonitorChart::SetScaleFloor(double mbps) {
    scale_floor_ = std::max(0.0, mbps);
}

void SpeedMonitorChart::Prime(const std::chrono::steady_clock::time_point& now) {
    buckets_.clear();
    bucket_start_ = now;
    for (size_t i = 0; i < kMaxBuckets; ++i) {
        buckets_.push_back({});
        buckets_.back().start = now;
    }
}

void SpeedMonitorChart::EnsureChart() {
    if (buckets_.empty()) {
        Prime(std::chrono::steady_clock::now());
    }
    Refresh();
}

void SpeedMonitorChart::SetRecording(bool recording) {
    if (recording_ == recording) {
        return;
    }
    recording_ = recording;
    if (recording_) {
        EnsureChart();
        timer_.Start(kTimerMs);
    } else {
        timer_.Stop();
    }
}

void SpeedMonitorChart::OnTimer(wxTimerEvent&) {
    EnsureBucket(std::chrono::steady_clock::now());
    Refresh();
}

void SpeedMonitorChart::EnsureBucket(const std::chrono::steady_clock::time_point& now) {
    if (buckets_.empty()) {
        Prime(now);
        return;
    }
    while (std::chrono::duration_cast<std::chrono::milliseconds>(now - bucket_start_).count()
           >= kBucketMs) {
        bucket_start_ += std::chrono::milliseconds(kBucketMs);
        buckets_.push_back({});
        buckets_.back().start = bucket_start_;
        while (buckets_.size() > kMaxBuckets) buckets_.pop_front();
    }
}

double SpeedMonitorChart::ScrollPhase(const std::chrono::steady_clock::time_point& now) {
    EnsureBucket(now);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - bucket_start_).count();
    return std::min(0.999, std::max(0.0, static_cast<double>(ms) / kBucketMs));
}

void SpeedMonitorChart::PushSessionPulse() {
    EnsureBucket(std::chrono::steady_clock::now());
    buckets_.back().data.session =
        std::min(kMaxSessionPerBucket, buckets_.back().data.session + 1);
    Refresh();
}

void SpeedMonitorChart::PushSample(double mbps) {
    const double value = std::max(0.0, mbps);
    if (value <= 0.0) return;
    const auto now = std::chrono::steady_clock::now();
    if (last_push_ != std::chrono::steady_clock::time_point{}) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_push_);
        if (elapsed.count() < kMinIntervalMs && std::abs(value - last_value_) < 0.05) return;
    }
    last_push_ = now;
    last_value_ = value;
    EnsureBucket(now);
    buckets_.back().data.transfer_mbps = std::max(buckets_.back().data.transfer_mbps, value);
    Refresh();
}

void SpeedMonitorChart::Clear() {
    buckets_.clear();
    last_value_ = -1.0;
    last_push_ = {};
    bucket_start_ = {};
    scale_floor_ = 0.0;
    Refresh();
}

void SpeedMonitorChart::ClearTransferTrack() {
    for (TimedBucket& bucket : buckets_) bucket.data.transfer_mbps = 0.0;
    scale_floor_ = 0.0;
    Refresh();
}

double SpeedMonitorChart::TransferScaleMax() const {
    double peak = 4.0;
    for (const TimedBucket& bucket : buckets_) peak = std::max(peak, bucket.data.transfer_mbps);
    if (scale_floor_ > 0.0) peak = std::max(peak, scale_floor_);
    return std::max(4.0, peak * 1.15);
}

void SpeedMonitorChart::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        return;
    }
    dc.SetBrush(wxBrush(kMonitorBg));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, sz.x, sz.y);

    const int pad_x = 8;
    const int pad_y = 3;
    const int legend_h = 14;
    const int legend_top = sz.y - 3 - legend_h;
    const int plot_w = std::max(0, sz.x - pad_x * 2);
    const int plot_h = std::max(14, legend_top - pad_y - 8);
    const int base_y = pad_y + plot_h;
    const auto now = std::chrono::steady_clock::now();
    const double phase = buckets_.empty() ? 0.0 : ScrollPhase(now);
    const double transfer_max = TransferScaleMax();
    const int bar_count = std::max(static_cast<int>(buckets_.size()), 1);
    const int gap = 2;
    const int bar_w = std::max(2, (plot_w - gap * (bar_count - 1)) / bar_count);
    const double stride = static_cast<double>(bar_w + gap);
    const int scroll_x = static_cast<int>(std::lround(phase * stride));
    const int session_cap = std::max(6, static_cast<int>(plot_h * 0.38));

    dc.SetClippingRegion(pad_x, pad_y, plot_w, plot_h);
    for (int i = 0; i < static_cast<int>(buckets_.size()); ++i) {
        const ActivityBucket& b = buckets_[static_cast<size_t>(i)].data;
        const int x = pad_x + static_cast<int>(i * stride) - scroll_x;
        if (b.session > 0) {
            const int h = std::max(2, static_cast<int>(std::lround(
                (static_cast<double>(b.session) / kMaxSessionPerBucket) * session_cap)));
            dc.SetBrush(wxBrush(kSessionBar));
            dc.DrawRectangle(x, base_y - h, bar_w, h);
        }
        if (b.transfer_mbps > 0.0) {
            const int h = std::max(2, static_cast<int>(
                std::lround((b.transfer_mbps / transfer_max) * plot_h)));
            dc.SetBrush(wxBrush(kTransferBar));
            dc.DrawRectangle(x, base_y - h, bar_w, h);
        }
    }
    dc.DestroyClippingRegion();

    wxFont legend = dc.GetFont();
    legend.SetPointSize(10);
    dc.SetFont(legend);
    dc.SetTextForeground(kLegendText);
    const int swatch = 8;
    const int cy = legend_top + legend_h / 2;
    const wxSize se = dc.GetTextExtent("session");
    dc.SetBrush(wxBrush(kSessionBar));
    dc.DrawRectangle(pad_x, cy - swatch / 2, swatch, swatch);
    dc.DrawText("session", pad_x + swatch + 5, cy - se.y / 2);
    const int tx = pad_x + swatch + 5 + se.x + 12;
    dc.SetBrush(wxBrush(kTransferBar));
    dc.DrawRectangle(tx, cy - swatch / 2, swatch, swatch);
    dc.DrawText("transfer", tx + swatch + 5, cy - se.y / 2);
}
