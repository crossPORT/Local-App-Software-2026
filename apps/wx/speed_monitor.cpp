#include "speed_monitor.h"

#include "link_status.h"

#include <algorithm>
#include <sstream>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {

const wxColour kMonitorBg(0x0f, 0x16, 0x20);
const wxColour kSessionBar(0x5b, 0x9f, 0xd4);
const wxColour kTransferBar(0x00, 0xd4, 0xaa);
const wxColour kMonitorBorder(0x24, 0x30, 0x42);
const wxColour kLegendText(0x88, 0x99, 0xaa);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);

constexpr int kHeadPad = 6;
constexpr int kFramePad = 1;

wxStaticText* MakeStatLabel(wxWindow* parent,
                            const wxString& text,
                            const wxColour& fg,
                            int pt,
                            wxFontWeight weight = wxFONTWEIGHT_NORMAL) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    font.SetWeight(weight);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(kMonitorBg);
    return label;
}

wxPanel* MakeStatCell(wxWindow* parent,
                      wxStaticText** value_out,
                      wxStaticText** label_out,
                      const wxString& caption) {
    auto* cell = new wxPanel(parent, wxID_ANY);
    cell->SetBackgroundColour(kMonitorBg);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    *value_out = MakeStatLabel(cell, wxEmptyString, kText, 10, wxFONTWEIGHT_BOLD);
    *label_out = MakeStatLabel(cell, caption, kMuted, 7);
    (*value_out)->SetWindowStyleFlag(wxALIGN_CENTER_HORIZONTAL);
    (*label_out)->SetWindowStyleFlag(wxALIGN_CENTER_HORIZONTAL);
    sizer->Add(*value_out, 0, wxALIGN_CENTER_HORIZONTAL);
    sizer->Add(*label_out, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 2);
    cell->SetSizer(sizer);
    return cell;
}

}  // namespace

SpeedMonitor::SpeedMonitor(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kMonitorBg);
    Bind(wxEVT_PAINT, &SpeedMonitor::OnPaint, this);

    root_sizer_ = new wxBoxSizer(wxVERTICAL);

    head_panel_ = new wxPanel(this, wxID_ANY);
    head_panel_->SetBackgroundColour(kMonitorBg);
    auto* head = new wxBoxSizer(wxVERTICAL);

    auto* top_row = new wxBoxSizer(wxHORIZONTAL);
    count_label_ = MakeStatLabel(head_panel_, wxEmptyString, kMuted, 8, wxFONTWEIGHT_BOLD);
    live_value_ = MakeStatLabel(head_panel_, wxEmptyString, kAccent, 12, wxFONTWEIGHT_BOLD);
    live_label_ = MakeStatLabel(head_panel_, "live", kMuted, 8);
    top_row->Add(count_label_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    top_row->Add(live_value_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    top_row->Add(live_label_, 0, wxALIGN_CENTER_VERTICAL);
    head->Add(top_row, 0, wxEXPAND | wxBOTTOM, 4);

    auto* stats_row = new wxBoxSizer(wxHORIZONTAL);
    stats_row->AddStretchSpacer();
    stats_row->Add(MakeStatCell(head_panel_, &median_value_, &median_label_, "MEDIAN"), 1, wxEXPAND);
    stats_row->Add(MakeStatCell(head_panel_, &max_value_, &max_label_, "MAX"), 1, wxEXPAND);
    stats_row->Add(MakeStatCell(head_panel_, &avg_value_, &avg_label_, "AVG"), 1, wxEXPAND);
    stats_row->AddStretchSpacer();
    stats_row_item_ = head->Add(stats_row, 0, wxEXPAND);
    stats_row_item_->Show(false);

    head_panel_->SetSizer(head);
    head_item_ = root_sizer_->Add(head_panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, kHeadPad);
    head_panel_->Show();
    head_item_->Show(true);

    chart_ = new SpeedMonitorChart(this);
    chart_item_ = root_sizer_->Add(chart_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kHeadPad);

    SyncChartSize();
    SetSizer(root_sizer_);
    SetMinSize(wxSize(-1, PreferredHeight()));
}

void SpeedMonitor::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        return;
    }
    dc.SetPen(wxPen(kMonitorBorder));
    dc.SetBrush(wxBrush(kMonitorBg));
    dc.DrawRoundedRectangle(kFramePad, kFramePad, sz.x - kFramePad * 2, sz.y - kFramePad * 2, 8);
}

void SpeedMonitor::SyncChartSize() {
    if (!chart_ || !chart_item_) {
        return;
    }
    constexpr int kChartH = SpeedMonitorChart::kMonitorHeight;
    chart_->Show(true);
    chart_item_->Show(true);
    chart_->SetMinSize(wxSize(-1, kChartH));
    chart_->SetMaxSize(wxSize(-1, kChartH));
    chart_item_->SetMinSize(wxSize(-1, kChartH));
}

int SpeedMonitor::PreferredHeight() const {
    constexpr int kChart = SpeedMonitorChart::kMonitorHeight;
    int head_h = 24;
    if (head_panel_ && head_panel_->IsShown()) {
        head_panel_->Layout();
        head_h = std::max(head_panel_->GetBestSize().GetHeight(), 18);
    }
    return head_h + kChart + kHeadPad * 2 + 4;
}

void SpeedMonitor::RelayoutHead(double live_mbps, const RateStats& stats) {
    const bool show_live = live_mbps > 0.0;
    const bool show_stats = stats.count > 0;

    if (!head_panel_ || !head_item_) {
        return;
    }

    head_panel_->Show();
    head_item_->Show(true);

    if (live_value_ && live_label_) {
        if (show_live) {
            live_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(live_mbps).c_str()));
            live_value_->Show();
            live_label_->Show();
        } else {
            live_value_->Hide();
            live_label_->Hide();
        }
    }

    if (count_label_) {
        if (show_stats && !show_live) {
            std::ostringstream out;
            out << stats.count << (stats.count == 1 ? " TRANSFER" : " TRANSFERS");
            count_label_->SetLabel(wxString::FromUTF8(out.str().c_str()));
            count_label_->Show();
        } else if (!show_live) {
            count_label_->SetLabel("NO TRANSFERS YET");
            count_label_->Show();
        } else {
            count_label_->Hide();
        }
    }

    if (show_stats) {
        if (median_value_) {
            median_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.median).c_str()));
        }
        if (max_value_) {
            max_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.max).c_str()));
        }
        if (avg_value_) {
            avg_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.average).c_str()));
        }
    }

    if (stats_row_item_) {
        stats_row_item_->Show(show_stats);
    }
    for (wxStaticText* label :
         {median_label_, median_value_, max_label_, max_value_, avg_label_, avg_value_}) {
        if (label) {
            label->Show(show_stats);
        }
    }

    head_panel_->Layout();
}

void SpeedMonitor::UpdateHead(double live_mbps, const RateStats& stats) {
    RelayoutHead(live_mbps, stats);
    SyncChartSize();
    Layout();

    const int height = PreferredHeight();
    SetMinSize(wxSize(-1, height));
    if (GetSizer()) {
        GetSizer()->SetMinSize(wxSize(-1, height));
    }
    if (chart_) {
        chart_->Refresh();
    }
    Refresh(false);
}

void SpeedMonitor::SetRecording(bool recording) {
    if (chart_) {
        chart_->SetRecording(recording);
    }
}

void SpeedMonitor::EnsureChart() {
    if (chart_) {
        chart_->EnsureChart();
    }
}

void SpeedMonitor::PushSessionPulse() {
    if (chart_) {
        chart_->PushSessionPulse();
    }
}

void SpeedMonitor::PushSample(double mbps) {
    if (chart_) {
        chart_->PushSample(mbps);
    }
}

void SpeedMonitor::Clear() {
    if (chart_) {
        chart_->Clear();
    }
}

void SpeedMonitor::ClearTransferTrack() {
    if (chart_) {
        chart_->ClearTransferTrack();
    }
}

void SpeedMonitor::SetScaleFloor(double mbps) {
    if (chart_) {
        chart_->SetScaleFloor(mbps);
    }
}

SpeedMonitorChart::SpeedMonitorChart(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kMonitorBg);
    SetMinSize(wxSize(-1, kMonitorHeight));
    SetMaxSize(wxSize(-1, kMonitorHeight));
    Bind(wxEVT_PAINT, &SpeedMonitorChart::OnPaint, this);
}

void SpeedMonitorChart::SetScaleFloor(double mbps) {
    scale_floor_ = std::max(0.0, mbps);
}

void SpeedMonitorChart::EnsureChart() {
    if (buckets_.empty()) {
        EnsureBucket(std::chrono::steady_clock::now());
    }
    Refresh();
}

void SpeedMonitorChart::SetRecording(bool recording) {
    recording_ = recording;
}

void SpeedMonitorChart::EnsureBucket(const std::chrono::steady_clock::time_point& now) {
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

void SpeedMonitorChart::PushSessionPulse() {
    const auto now = std::chrono::steady_clock::now();
    EnsureBucket(now);
    ActivityBucket& bucket = buckets_.back().data;
    bucket.session = std::min(kMaxSessionPerBucket, bucket.session + 1);
    Refresh();
}

void SpeedMonitorChart::PushSample(double mbps) {
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

void SpeedMonitorChart::Clear() {
    buckets_.clear();
    last_value_ = -1.0;
    last_push_ = {};
    bucket_start_ = {};
    scale_floor_ = 0.0;
    Refresh();
}

void SpeedMonitorChart::ClearTransferTrack() {
    for (TimedBucket& bucket : buckets_) {
        bucket.data.transfer_mbps = 0.0;
    }
    scale_floor_ = 0.0;
    Refresh();
}

void SpeedMonitorChart::Redraw() {
    Refresh();
}

double SpeedMonitorChart::TransferScaleMax() const {
    const double idle_floor = 4.0;
    double peak = idle_floor;
    for (const TimedBucket& bucket : buckets_) {
        peak = std::max(peak, bucket.data.transfer_mbps);
    }
    if (scale_floor_ > 0.0) {
        peak = std::max(peak, scale_floor_);
    }
    return std::max(idle_floor, peak * 1.15);
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
    const int pad_y_top = 3;
    constexpr int kLegendRowH = 14;
    constexpr int kLegendBottomPad = 3;
    constexpr int kLegendGap = 8;
    const int legend_row_top = sz.y - kLegendBottomPad - kLegendRowH;
    const int plot_w = std::max(0, sz.x - pad_x * 2);
    const int plot_h = std::max(14, legend_row_top - pad_y_top - kLegendGap);
    const int plot_top = pad_y_top;
    const int base_y = plot_top + plot_h;
    const double transfer_max = TransferScaleMax();

    const int bar_count = std::max(static_cast<int>(buckets_.size()), 1);
    const int gap = 2;
    const int bar_w = std::max(2, (plot_w - gap * (bar_count - 1)) / bar_count);
    const int session_cap = std::max(6, static_cast<int>(plot_h * 0.38));

    if (!buckets_.empty()) {
        for (int i = 0; i < static_cast<int>(buckets_.size()); ++i) {
            const ActivityBucket& bucket = buckets_[static_cast<size_t>(i)].data;
            const int x = pad_x + i * (bar_w + gap);

            if (bucket.session > 0) {
                const int session_h =
                    static_cast<int>(std::lround((static_cast<double>(bucket.session)
                                                  / static_cast<double>(kMaxSessionPerBucket))
                                                 * session_cap));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(kSessionBar));
                dc.DrawRectangle(x, base_y - session_h, bar_w, std::max(session_h, 2));
            }

            if (bucket.transfer_mbps > 0.0) {
                const int transfer_h = static_cast<int>(
                    std::lround((bucket.transfer_mbps / transfer_max) * plot_h));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(kTransferBar));
                dc.DrawRectangle(x, base_y - transfer_h, bar_w, std::max(transfer_h, 2));
            }
        }
    } else {
        wxFont hint = dc.GetFont();
        hint.SetPointSize(9);
        dc.SetFont(hint);
        dc.SetTextForeground(kLegendText);
        dc.DrawText("Waiting for USB activity…", pad_x, plot_top + std::max(0, plot_h / 2 - 6));
    }

    wxFont legend = dc.GetFont();
    legend.SetPointSize(10);
    dc.SetFont(legend);
    dc.SetTextForeground(kLegendText);

    const wxSize session_ext = dc.GetTextExtent("session");
    const int swatch = 8;
    const int legend_center_y = legend_row_top + kLegendRowH / 2;
    const int swatch_y = legend_center_y - swatch / 2;
    const int text_y = legend_center_y - session_ext.y / 2;

    dc.SetBrush(wxBrush(kSessionBar));
    dc.DrawRectangle(pad_x, swatch_y, swatch, swatch);
    dc.DrawText("session", pad_x + swatch + 5, text_y);

    const int transfer_x = pad_x + swatch + 5 + session_ext.x + 12;
    dc.SetBrush(wxBrush(kTransferBar));
    dc.DrawRectangle(transfer_x, swatch_y, swatch, swatch);
    dc.DrawText("transfer", transfer_x + swatch + 5, text_y);
}
