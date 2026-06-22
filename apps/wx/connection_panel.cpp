#include "connection_panel.h"

#include "link_status.h"
#include "speed_monitor.h"

#include <sstream>
#include <wx/sizer.h>

namespace {

const wxColour kCard(0x12, 0x18, 0x22);
const wxColour kBorder(0x24, 0x30, 0x42);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kError(0xff, 0x6b, 0x6b);
const wxColour kWarn(0xff, 0x9f, 0x43);

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

bool transfer_is_sending(const std::string& status) {
    return status.rfind("Sending ", 0) == 0;
}

bool transfer_is_receiving(const std::string& status) {
    return status.rfind("Receiving ", 0) == 0 || status.rfind("Waiting for ", 0) == 0;
}

std::string device_meta_line(int device_port_index, int devices_seen) {
    std::ostringstream out;
    out << "Port " << device_port_index;
    if (devices_seen == 0) {
        out << " · no devices detected";
    } else if (devices_seen == 1) {
        out << " · 1 device connected";
    } else {
        out << " · " << devices_seen << " devices connected";
    }
    return out.str();
}

}  // namespace

ConnectionPanel::ConnectionPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundColour(kCard);
    SetMinSize(wxSize(-1, 88));

    auto* root = new wxBoxSizer(wxVERTICAL);
    section_label_ = MakeLabel(this, "USB device", kAccent, 11, wxFONTWEIGHT_BOLD);
    root->Add(section_label_, 0, wxEXPAND | wxBOTTOM, 4);

    hint_label_ = MakeLabel(this,
                            "Plug in your USB cable, then choose it when prompted.",
                            kMuted,
                            11);
    hint_label_->Wrap(520);
    root->Add(hint_label_, 0, wxEXPAND | wxTOP, 10);

    device_label_ = MakeLabel(this, wxEmptyString, kText, 13, wxFONTWEIGHT_BOLD);
    device_label_->Wrap(520);
    device_label_->Hide();
    root->Add(device_label_, 0, wxEXPAND | wxTOP, 10);

    meta_label_ = MakeLabel(this, wxEmptyString, kMuted, 11);
    meta_label_->Wrap(520);
    meta_label_->Hide();
    root->Add(meta_label_, 0, wxEXPAND | wxTOP, 6);

    warn_label_ = MakeLabel(this, wxEmptyString, kWarn, 10);
    warn_label_->Wrap(520);
    warn_label_->Hide();
    root->Add(warn_label_, 0, wxEXPAND | wxTOP, 6);

    speed_label_ = MakeLabel(this, wxEmptyString, kAccent, 12, wxFONTWEIGHT_BOLD);
    speed_label_->Hide();
    root->Add(speed_label_, 0, wxEXPAND | wxTOP, 8);

    error_label_ = MakeLabel(this, wxEmptyString, kError, 10);
    error_label_->Wrap(520);
    error_label_->Hide();
    root->Add(error_label_, 0, wxEXPAND | wxTOP, 8);

    activity_monitor_ = new SpeedMonitor(this);
    activity_monitor_->SetMinSize(wxSize(-1, 56));
    activity_monitor_item_ = root->Add(activity_monitor_, 0, wxEXPAND | wxTOP, 10);
    activity_monitor_item_->Show(false);
    activity_monitor_item_->SetMinSize(wxSize(-1, 0));
    activity_monitor_->Hide();

    root_sizer_ = root;
    SetSizer(root);
}

void ConnectionPanel::SyncActivityMonitor(bool show,
                                          uint32_t fabric_activity_seq,
                                          double live_mbps,
                                          int64_t last_announce_ms,
                                          double chart_scale_mbps) {
    if (!activity_monitor_ || !activity_monitor_item_ || !root_sizer_) {
        return;
    }

    if (show) {
        activity_monitor_item_->Show(true);
        activity_monitor_item_->SetMinSize(wxSize(-1, 56));
        activity_monitor_->Show(true);
        activity_monitor_->SetMinSize(wxSize(-1, 56));
        activity_monitor_->SetScaleFloor(chart_scale_mbps > 0.0 ? chart_scale_mbps : live_mbps);
        activity_monitor_->SetRecording(true);
        activity_monitor_->EnsureChart();
        if (fabric_activity_seq > last_activity_seq_) {
            const uint32_t delta = fabric_activity_seq - last_activity_seq_;
            for (uint32_t i = 0; i < delta; ++i) {
                activity_monitor_->PushSessionPulse();
            }
            last_activity_seq_ = fabric_activity_seq;
        }
        if (live_mbps > 0.0) {
            activity_monitor_->PushSample(live_mbps);
        }
        (void)last_announce_ms;
        SetMinSize(wxSize(-1, 168));
        return;
    }

    activity_monitor_item_->Show(false);
    activity_monitor_item_->SetMinSize(wxSize(-1, 0));
    activity_monitor_->Hide();
    activity_monitor_->SetRecording(false);
    activity_monitor_->Clear();
    last_activity_seq_ = 0;
    SetMinSize(wxSize(-1, 88));
}

void ConnectionPanel::RelayoutAncestors() {
    Layout();
    for (wxWindow* ancestor = GetParent(); ancestor != nullptr; ancestor = ancestor->GetParent()) {
        ancestor->Layout();
    }
}

void ConnectionPanel::ApplyState(bool fabric_connected,
                                 int fabric_port_index,
                                 int fabric_devices_seen,
                                 const std::string& fabric_device_label,
                                 bool busy,
                                 double live_mbps,
                                 double booth_display_mib_s,
                                 int64_t last_announce_ms,
                                 uint32_t fabric_activity_seq,
                                 const std::string& status_message,
                                 const std::string& error_message) {
    if (fabric_connected && !fabric_device_label.empty()) {
        hint_label_->Hide();
        device_label_->SetLabel(wxString::FromUTF8(fabric_device_label.c_str()));
        device_label_->Show();
        meta_label_->SetLabel(
            wxString::FromUTF8(device_meta_line(fabric_port_index, fabric_devices_seen).c_str()));
        meta_label_->Show();
        warn_label_->Hide();
    } else if (fabric_devices_seen > 0) {
        hint_label_->SetLabel("USB cable detected — restart the app to choose it.");
        hint_label_->Show();
        device_label_->Hide();
        meta_label_->SetLabel(
            wxString::FromUTF8(device_meta_line(fabric_port_index, fabric_devices_seen).c_str()));
        meta_label_->Show();
        warn_label_->Hide();
    } else {
        hint_label_->SetLabel("Plug in your USB cable, then choose it when prompted.");
        hint_label_->Show();
        device_label_->Hide();
        meta_label_->Hide();
        warn_label_->Hide();
    }

    const double display_mbps =
        live_mbps > 0.0 ? live_mbps : (busy && booth_display_mib_s > 0.0 ? booth_display_mib_s : 0.0);

    if (busy && display_mbps > 0.0) {
        speed_label_->SetLabel(wxString::FromUTF8(format_mbps_rate(display_mbps).c_str()));
        speed_label_->SetForegroundColour(kAccent);
        speed_label_->Show();
    } else if (fabric_connected) {
        speed_label_->SetLabel("Device active");
        speed_label_->SetForegroundColour(kMuted);
        speed_label_->Show();
    } else {
        speed_label_->Hide();
    }

    const double chart_scale =
        display_mbps > 0.0 ? display_mbps
        : (booth_display_mib_s > 0.0 ? booth_display_mib_s : 0.0);

    SyncActivityMonitor(fabric_connected,
                        fabric_activity_seq,
                        display_mbps,
                        last_announce_ms,
                        chart_scale);

    error_label_->Hide();

    RelayoutAncestors();
}
