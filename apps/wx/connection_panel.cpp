#include "connection_panel.h"

#include "fabric_port.h"
#include "link_status.h"
#include "session_rate_stats.h"
#include "speed_monitor.h"

#include <sstream>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {

const wxColour kCard(0x12, 0x18, 0x22);
const wxColour kField(0x12, 0x18, 0x22);
const wxColour kBorder(0x24, 0x30, 0x42);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kError(0xff, 0x6b, 0x6b);
const wxColour kWarn(0xff, 0x9f, 0x43);
const wxColour kBg(0x0f, 0x14, 0x19);

void StylePrimaryButton(wxButton* button) {
    button->SetBackgroundColour(kAccent);
    button->SetForegroundColour(kBg);
    const wxSize best = button->GetBestSize();
    button->SetMinSize(wxSize(-1, std::max(best.GetHeight() + 4, 36)));
}

wxStaticText* MakeLabel(wxWindow* parent,
                        const wxString& text,
                        const wxColour& fg,
                        int pt = 10,
                        wxFontWeight weight = wxFONTWEIGHT_NORMAL) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    font.SetWeight(weight);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

std::string multi_device_meta_line(int devices_seen) {
    std::ostringstream out;
    if (devices_seen <= 1) {
        return {};
    }
    out << devices_seen << " devices — pick your cable in the USB dialog";
    return out.str();
}

class DisconnectButton : public wxPanel {
public:
    explicit DisconnectButton(wxWindow* parent, std::function<void()> on_click)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , on_click_(std::move(on_click)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kField);
        SetMinSize(wxSize(-1, 34));
        SetCursor(wxCursor(wxCURSOR_HAND));

        auto* sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* label = MakeLabel(this, "Disconnect", kText, 10);
        sizer->AddStretchSpacer();
        sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL);
        sizer->AddStretchSpacer();
        SetSizer(sizer);

        Bind(wxEVT_PAINT, &DisconnectButton::OnPaint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
            event.StopPropagation();
            if (on_click_) {
                on_click_();
            }
        });
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(wxPen(kBorder));
        dc.SetBrush(wxBrush(kField));
        dc.DrawRoundedRectangle(0, 0, sz.x - 1, sz.y - 1, 8);
    }

    std::function<void()> on_click_;
};

}  // namespace

ConnectionPanel::ConnectionPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundColour(kCard);

    auto* root = new wxBoxSizer(wxVERTICAL);
    section_label_ = MakeLabel(this, "USB device", kAccent, 11, wxFONTWEIGHT_BOLD);
    root->Add(section_label_, 0, wxEXPAND | wxBOTTOM, 2);

    hint_label_ = MakeLabel(this,
                            "Plug in your USB cable, then choose it when prompted.",
                            kMuted,
                            11);
    hint_label_->Wrap(520);
    root->Add(hint_label_, 0, wxEXPAND | wxTOP, 8);

    device_label_ = MakeLabel(this, wxEmptyString, kText, 13, wxFONTWEIGHT_BOLD);
    device_label_->Wrap(520);
    device_label_->Hide();
    root->Add(device_label_, 0, wxEXPAND | wxTOP, 2);

    meta_label_ = MakeLabel(this, wxEmptyString, kMuted, 11);
    meta_label_->Wrap(520);
    meta_label_->Hide();
    root->Add(meta_label_, 0, wxEXPAND | wxTOP, 6);

    warn_label_ = MakeLabel(this, wxEmptyString, kWarn, 10);
    warn_label_->Wrap(520);
    warn_label_->Hide();
    root->Add(warn_label_, 0, wxEXPAND | wxTOP, 6);

    error_label_ = MakeLabel(this, wxEmptyString, kError, 10);
    error_label_->Wrap(520);
    error_label_->Hide();
    root->Add(error_label_, 0, wxEXPAND | wxTOP, 8);

    activity_monitor_ = new SpeedMonitor(this);
    activity_monitor_item_ = root->Add(activity_monitor_, 0, wxEXPAND | wxTOP, 4);
    activity_monitor_item_->Show(false);
    activity_monitor_item_->SetMinSize(wxSize(-1, SpeedMonitorChart::kMonitorHeight));
    activity_monitor_->Hide();

    disconnect_btn_ = new DisconnectButton(this, [this]() {
        if (on_disconnect_) {
            on_disconnect_();
        }
    });
    disconnect_btn_->Hide();
    disconnect_item_ = root->Add(disconnect_btn_, 0, wxEXPAND | wxTOP, 8);
    disconnect_item_->Show(false);
    disconnect_item_->SetMinSize(wxSize(-1, 34));

    connect_btn_ = new wxButton(this, wxID_ANY, "Connect USB");
    StylePrimaryButton(connect_btn_);
    connect_btn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (on_connect_) {
            on_connect_();
        }
    });
    connect_item_ = root->Add(connect_btn_, 0, wxEXPAND | wxTOP, 10);

    root_sizer_ = root;
    SetSizer(root);
}

void ConnectionPanel::SetActionHandlers(std::function<void()> on_connect,
                                        std::function<void()> on_disconnect) {
    on_connect_ = std::move(on_connect);
    on_disconnect_ = std::move(on_disconnect);
}

void ConnectionPanel::SetLayoutChangedHandler(std::function<void()> on_layout_changed) {
    on_layout_changed_ = std::move(on_layout_changed);
}

void ConnectionPanel::SyncActivityMonitor(bool show,
                                          uint32_t fabric_activity_seq,
                                          double live_mbps,
                                          double result_mbps,
                                          double booth_display_mib_s) {
    if (!activity_monitor_ || !activity_monitor_item_ || !root_sizer_) {
        return;
    }

    if (!show) {
        activity_monitor_->Hide();
        activity_monitor_item_->Show(false);
        activity_monitor_item_->SetMinSize(wxSize(-1, 0));
        activity_monitor_->SetRecording(false);
        activity_monitor_->Clear();
        last_activity_seq_ = 0;
        last_live_mbps_ = 0.0;
        last_stats_count_ = 0;
        return;
    }

    rate_tracker_.observe_result_mbps(result_mbps);
    const RateStats stats = rate_tracker_.stats();

    const bool has_live = live_mbps > 0.0;
    const double scale_floor =
        has_live && booth_display_mib_s > 0.0
            ? std::max(live_mbps, booth_display_mib_s)
            : (has_live ? live_mbps : 0.0);

    activity_monitor_->Show(true);
    activity_monitor_item_->Show(true);
    activity_monitor_->SetRecording(true);
    activity_monitor_->SetScaleFloor(scale_floor);
    activity_monitor_->EnsureChart();

    if (fabric_activity_seq > last_activity_seq_) {
        const uint32_t delta = fabric_activity_seq - last_activity_seq_;
        for (uint32_t i = 0; i < delta; ++i) {
            activity_monitor_->PushSessionPulse();
        }
        last_activity_seq_ = fabric_activity_seq;
    }

    if (has_live) {
        activity_monitor_->PushSample(live_mbps);
    } else if (stats.count > last_stats_count_ && result_mbps > 0.0) {
        activity_monitor_->PushSample(result_mbps);
    }
    last_stats_count_ = stats.count;
    last_live_mbps_ = live_mbps;

    activity_monitor_->UpdateHead(live_mbps, stats);

    const int monitor_h = activity_monitor_->PreferredHeight();
    activity_monitor_item_->SetMinSize(wxSize(-1, monitor_h));
    activity_monitor_->SetMinSize(wxSize(-1, monitor_h));
    activity_monitor_->Layout();
    activity_monitor_->Refresh();
}

void ConnectionPanel::SyncPanelMinSize() {
    if (!root_sizer_) {
        return;
    }
    Layout();
    const wxSize min = root_sizer_->CalcMin();
    SetMinSize(wxSize(-1, min.GetHeight()));
}

void ConnectionPanel::SyncConnectActions(bool fabric_connected) {
    if (!connect_btn_ || !disconnect_btn_ || !connect_item_ || !disconnect_item_) {
        return;
    }
    if (fabric_connected) {
        connect_btn_->Hide();
        connect_item_->Show(false);
        disconnect_btn_->Show();
        disconnect_item_->Show(true);
        disconnect_item_->SetMinSize(wxSize(-1, 34));
    } else {
        disconnect_btn_->Hide();
        disconnect_item_->Show(false);
        connect_btn_->Show();
        connect_item_->Show(true);
    }
    if (root_sizer_) {
        root_sizer_->Layout();
    }
    Layout();
    InvalidateBestSize();
}

void ConnectionPanel::RelayoutAncestors() {
    SyncPanelMinSize();
    InvalidateBestSize();
    Layout();
    for (wxWindow* ancestor = GetParent(); ancestor != nullptr; ancestor = ancestor->GetParent()) {
        ancestor->Layout();
        ancestor->Refresh(false);
    }
    if (on_layout_changed_) {
        on_layout_changed_();
    }
}

void ConnectionPanel::ApplyState(bool fabric_connected,
                                 int fabric_port_index,
                                 int fabric_devices_seen,
                                 const std::string& fabric_device_label,
                                 bool busy,
                                 double live_mbps,
                                 double booth_display_mib_s,
                                 double result_mbps,
                                 int64_t last_announce_ms,
                                 uint32_t fabric_activity_seq,
                                 const std::string& status_message,
                                 const std::string& error_message) {
    (void)last_announce_ms;
    (void)status_message;
    (void)error_message;

    std::string device_label = fabric_device_label;
    if (fabric_connected && device_label.empty() && fabric_port_index >= 0) {
        device_label = format_fabric_port_label(fabric_port_index);
    }

    bool show_connected = false;
    if (fabric_connected && !device_label.empty()) {
        hint_label_->Hide();
        device_label_->SetLabel(wxString::FromUTF8(device_label.c_str()));
        device_label_->Show();
        show_connected = true;
        const std::string multi_meta = multi_device_meta_line(fabric_devices_seen);
        if (multi_meta.empty()) {
            meta_label_->Hide();
        } else {
            meta_label_->SetLabel(wxString::FromUTF8(multi_meta.c_str()));
            meta_label_->Show();
        }
        warn_label_->Hide();
    } else if (fabric_devices_seen > 0) {
        hint_label_->SetLabel(
            "USB cable detected — click Connect USB and pick it in the USB dialog.");
        hint_label_->Show();
        device_label_->Hide();
        const std::string multi_meta = multi_device_meta_line(fabric_devices_seen);
        if (multi_meta.empty()) {
            meta_label_->Hide();
        } else {
            meta_label_->SetLabel(wxString::FromUTF8(multi_meta.c_str()));
            meta_label_->Show();
        }
        warn_label_->Hide();
    } else {
        hint_label_->SetLabel("Plug in your USB cable, then choose it when prompted.");
        hint_label_->Show();
        device_label_->Hide();
        meta_label_->Hide();
        warn_label_->Hide();
    }

    const double chart_mbps =
        busy && live_mbps <= 0.0 && booth_display_mib_s > 0.0 ? booth_display_mib_s
                                                                : live_mbps;

    SyncActivityMonitor(show_connected,
                        fabric_activity_seq,
                        chart_mbps,
                        result_mbps,
                        booth_display_mib_s);
    SyncConnectActions(show_connected);

    error_label_->Hide();

    RelayoutAncestors();
}
