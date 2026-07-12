#include "speed_monitor.h"

#include "link_status.h"

#include <algorithm>
#include <sstream>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {

const wxColour kMonitorBg(0x0f, 0x16, 0x20);
const wxColour kMonitorBorder(0x24, 0x30, 0x42);
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
    if (!head_panel_ || !head_item_) return;
    head_panel_->Show();
    head_item_->Show(true);
    if (live_value_ && live_label_) {
        live_value_->Show(show_live);
        live_label_->Show(show_live);
        if (show_live) {
            live_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(live_mbps).c_str()));
        }
    }
    if (count_label_) {
        if (show_live) {
            count_label_->Hide();
        } else if (show_stats) {
            std::ostringstream out;
            out << stats.count << (stats.count == 1 ? " TRANSFER" : " TRANSFERS");
            count_label_->SetLabel(wxString::FromUTF8(out.str().c_str()));
            count_label_->Show();
        } else {
            count_label_->SetLabel("NO TRANSFERS YET");
            count_label_->Show();
        }
    }
    if (show_stats) {
        if (median_value_)
            median_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.median).c_str()));
        if (max_value_)
            max_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.max).c_str()));
        if (avg_value_)
            avg_value_->SetLabel(wxString::FromUTF8(format_mbps_rate(stats.average).c_str()));
    }
    if (stats_row_item_) stats_row_item_->Show(show_stats);
    for (wxStaticText* label :
         {median_label_, median_value_, max_label_, max_value_, avg_label_, avg_value_}) {
        if (label) label->Show(show_stats);
    }
    head_panel_->Layout();
}

void SpeedMonitor::UpdateHead(double live_mbps, const RateStats& stats) {
    RelayoutHead(live_mbps, stats);
    SyncChartSize();
    Layout();
    const int height = PreferredHeight();
    SetMinSize(wxSize(-1, height));
    if (GetSizer()) GetSizer()->SetMinSize(wxSize(-1, height));
    if (chart_) chart_->Refresh();
    Refresh(false);
}

void SpeedMonitor::SetRecording(bool recording) {
    if (chart_) chart_->SetRecording(recording);
}
void SpeedMonitor::EnsureChart() {
    if (chart_) chart_->EnsureChart();
}
void SpeedMonitor::PushSessionPulse() {
    if (chart_) chart_->PushSessionPulse();
}
void SpeedMonitor::PushSample(double mbps) {
    if (chart_) chart_->PushSample(mbps);
}
void SpeedMonitor::Clear() {
    if (chart_) chart_->Clear();
}
void SpeedMonitor::ClearTransferTrack() {
    if (chart_) chart_->ClearTransferTrack();
}
void SpeedMonitor::SetScaleFloor(double mbps) {
    if (chart_) chart_->SetScaleFloor(mbps);
}
