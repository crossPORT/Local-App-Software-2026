#include "speed_monitor.h"

#include <algorithm>
#include <wx/dcbuffer.h>

namespace {

const wxColour kMonitorBg(0x0f, 0x16, 0x20);
const wxColour kSessionBar(0x5b, 0x9f, 0xd4);
const wxColour kTransferBar(0x00, 0xd4, 0xaa);
const wxColour kMonitorBorder(0x24, 0x30, 0x42);
const wxColour kLegendText(0x88, 0x99, 0xaa);

}  // namespace

SpeedMonitor::SpeedMonitor(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kMonitorBg);
    SetMinSize(wxSize(-1, kMonitorHeight));
    Bind(wxEVT_PAINT, &SpeedMonitor::OnPaint, this);
}

void SpeedMonitor::SetScaleFloor(double mbps) {
    scale_floor_ = std::max(0.0, mbps);
}

void SpeedMonitor::EnsureChart() {
    if (buckets_.empty()) {
        EnsureBucket(std::chrono::steady_clock::now());
    }
    Refresh();
}

void SpeedMonitor::SetRecording(bool recording) {
    recording_ = recording;
}

void SpeedMonitor::EnsureBucket(const std::chrono::steady_clock::time_point& now) {
    if (buckets_.empty()) {
        bucket_start_ = now;
        buckets_.push_back({});
        buckets_.back().start = now;
        return;
    }

    while (std::chrono::duration_cast<std::chrono::milliseconds>(now - bucket_start_).count()
           >= kBucketMs) {
        bucket_start_ += std::chrono::milliseconds(kBucketMs);
        buckets_.push_back({});
        buckets_.back().start = bucket_start_;
        while (buckets_.size() > kMaxBuckets) {
            buckets_.pop_front();
        }
    }
}

void SpeedMonitor::PushSessionPulse() {
    const auto now = std::chrono::steady_clock::now();
    EnsureBucket(now);
    ActivityBucket& bucket = buckets_.back().data;
    bucket.session = std::min(kMaxSessionPerBucket, bucket.session + 1);
    Refresh();
}

void SpeedMonitor::PushSample(double mbps) {
    const double value = std::max(0.0, mbps);
    const auto now = std::chrono::steady_clock::now();
    if (last_push_ != std::chrono::steady_clock::time_point{}) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_push_);
        if (elapsed.count() < kMinIntervalMs && std::abs(value - last_value_) < 0.05) {
            return;
        }
    }
    last_push_ = now;
    last_value_ = value;
    EnsureBucket(now);
    ActivityBucket& bucket = buckets_.back().data;
    bucket.transfer_mbps = std::max(bucket.transfer_mbps, value);
    Refresh();
}

void SpeedMonitor::Clear() {
    buckets_.clear();
    last_value_ = -1.0;
    last_push_ = {};
    bucket_start_ = {};
    scale_floor_ = 0.0;
    Refresh();
}

double SpeedMonitor::TransferScaleMax() const {
    const double idle_floor = 4.0;
    double peak = scale_floor_ > 0.0 ? std::max(idle_floor, scale_floor_ * 0.85) : idle_floor;
    for (const TimedBucket& bucket : buckets_) {
        peak = std::max(peak, bucket.data.transfer_mbps);
    }
    return std::max(idle_floor, peak * 1.15);
}

void SpeedMonitor::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        return;
    }

    dc.SetPen(wxPen(kMonitorBorder));
    dc.SetBrush(wxBrush(kMonitorBg));
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);

    const int pad_x = 8;
    const int pad_y = 3;
    const int legend_h = 10;
    const int plot_w = std::max(0, sz.x - pad_x * 2);
    const int plot_h = std::max(0, sz.y - pad_y * 2 - legend_h);
    const int plot_top = pad_y;
    const int base_y = plot_top + plot_h;
    const double transfer_max = TransferScaleMax();

    if (buckets_.empty()) {
        wxFont hint = dc.GetFont();
        hint.SetPointSize(9);
        dc.SetFont(hint);
        dc.SetTextForeground(kLegendText);
        dc.DrawText("Waiting for USB activity…", pad_x, plot_top + (plot_h / 2) - 6);
    } else {
        const int bar_count = static_cast<int>(buckets_.size());
        const int gap = 2;
        const int bar_w = std::max(2, (plot_w - gap * (bar_count - 1)) / bar_count);
        const int session_cap = static_cast<int>(plot_h * 0.38);

        for (int i = 0; i < bar_count; ++i) {
            const ActivityBucket& bucket = buckets_[static_cast<size_t>(i)].data;
            const int x = pad_x + i * (bar_w + gap);

            if (bucket.session > 0) {
                const int session_h =
                    static_cast<int>(std::lround((static_cast<double>(bucket.session)
                                                  / static_cast<double>(kMaxSessionPerBucket))
                                                 * session_cap));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(kSessionBar));
                dc.DrawRectangle(x, base_y - session_h, bar_w, session_h);
            }

            if (bucket.transfer_mbps > 0.0) {
                const int transfer_h = static_cast<int>(
                    std::lround((bucket.transfer_mbps / transfer_max) * plot_h));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(kTransferBar));
                dc.DrawRectangle(x, base_y - transfer_h, bar_w, transfer_h);
            }
        }
    }

    wxFont legend = dc.GetFont();
    legend.SetPointSize(9);
    dc.SetFont(legend);
    dc.SetTextForeground(kLegendText);
    const int legend_y = plot_top + plot_h + 7;
    dc.SetBrush(wxBrush(kSessionBar));
    dc.DrawRectangle(pad_x, legend_y - 3, 6, 6);
    dc.DrawText("session", pad_x + 10, legend_y - 6);
    dc.SetBrush(wxBrush(kTransferBar));
    dc.DrawRectangle(pad_x + 58, legend_y - 3, 6, 6);
    dc.DrawText("transfer", pad_x + 68, legend_y - 6);
}
