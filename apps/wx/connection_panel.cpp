#include "connection_panel.h"

#include "connection_led.h"
#include "connection_widgets.h"
#include "system_names.h"
#include "ui_colours.h"

#include <wx/button.h>
#include <wx/sizer.h>

ConnectionPanel::ConnectionPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundColour(kUsbCard);

    auto* root = new wxBoxSizer(wxVERTICAL);
    section_label_ = MakeUiLabel(this, "This system", kAccent, 11, wxFONTWEIGHT_BOLD);
    root->Add(section_label_, 0, wxEXPAND | wxBOTTOM, 2);

    hint_label_ = MakeUiLabel(this,
                              "Plug in your USB cable, then choose it when prompted.",
                              kMuted,
                              11);
    hint_label_->Wrap(520);
    root->Add(hint_label_, 0, wxEXPAND | wxTOP, 8);

    auto* name_row = new wxBoxSizer(wxHORIZONTAL);
    auto* led = new ConnectionLedPanel(this);
    link_indicator_ = led;
    name_row->Add(led, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    device_label_ = MakeUiLabel(this, wxEmptyString, kText, 13, wxFONTWEIGHT_BOLD);
    name_row->Add(device_label_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(name_row, 0, wxEXPAND | wxTOP, 8);

    meta_label_ = MakeUiLabel(this, wxEmptyString, kMuted, 11);
    meta_label_->Wrap(520);
    meta_label_->Hide();
    root->Add(meta_label_, 0, wxEXPAND | wxTOP, 6);

    warn_label_ = MakeUiLabel(this, wxEmptyString, kWarn, 10);
    warn_label_->Wrap(520);
    warn_label_->Hide();
    root->Add(warn_label_, 0, wxEXPAND | wxTOP, 6);

    error_label_ = MakeUiLabel(this, wxEmptyString, kError, 10);
    error_label_->Wrap(520);
    error_label_->Hide();
    root->Add(error_label_, 0, wxEXPAND | wxTOP, 8);

    activity_monitor_ = new SpeedMonitor(this);
    activity_monitor_item_ = root->Add(activity_monitor_, 0, wxEXPAND | wxTOP, 4);
    activity_monitor_item_->Show(false);
    activity_monitor_item_->SetMinSize(wxSize(-1, SpeedMonitorChart::kMonitorHeight));
    activity_monitor_->Hide();

    disconnect_btn_ = MakeDisconnectButton(this, [this]() {
        if (on_disconnect_) {
            on_disconnect_();
        }
    });
    disconnect_btn_->Hide();
    disconnect_item_ = root->Add(disconnect_btn_, 0, wxEXPAND | wxTOP, 8);
    disconnect_item_->Show(false);
    disconnect_item_->SetMinSize(wxSize(-1, 34));

    auto* connect_row = new wxBoxSizer(wxHORIZONTAL);
    connect_btn_ = new wxButton(this, wxID_ANY, "Connect this system");
    StylePrimaryButton(connect_btn_);
    connect_btn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (on_connect_) {
            on_connect_();
        }
    });
    clear_btn_ = new wxButton(this, wxID_ANY, "Clear");
    StyleClearButton(clear_btn_);
    clear_btn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (on_clear_) {
            on_clear_();
        }
    });
    connect_row->Add(connect_btn_, 1, wxEXPAND);
    connect_row->Add(clear_btn_, 0, wxEXPAND | wxLEFT, 8);
    connect_item_ = root->Add(connect_row, 0, wxEXPAND | wxTOP, 10);

    root_sizer_ = root;
    SetSizer(root);
}

void ConnectionPanel::SetActionHandlers(std::function<void()> on_connect,
                                        std::function<void()> on_disconnect,
                                        std::function<void()> on_clear) {
    on_connect_ = std::move(on_connect);
    on_disconnect_ = std::move(on_disconnect);
    on_clear_ = std::move(on_clear);
}

void ConnectionPanel::SetLayoutChangedHandler(std::function<void()> on_layout_changed) {
    on_layout_changed_ = std::move(on_layout_changed);
}

void ConnectionPanel::SetLinkLed(const wxColour& colour, const wxString& tooltip) {
    auto* led = static_cast<ConnectionLedPanel*>(link_indicator_);
    if (!led) {
        return;
    }
    led->SetLedColour(colour);
    led->SetToolTip(tooltip);
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
    if (device_label.empty() && fabric_port_index >= 0) {
        device_label = system_name_for_leg(fabric_port_index);
    }
    if (device_label.empty()) {
        device_label = "This system";
    }
    device_label_->SetLabel(wxString::FromUTF8(device_label.c_str()));
    device_label_->Show();

    bool show_connected = false;
    if (fabric_connected) {
        hint_label_->Hide();
        show_connected = true;
        const std::string multi_meta = multi_system_meta_line(fabric_devices_seen);
        if (multi_meta.empty()) {
            meta_label_->Hide();
        } else {
            meta_label_->SetLabel(wxString::FromUTF8(multi_meta.c_str()));
            meta_label_->Show();
        }
        warn_label_->Hide();
    } else if (fabric_devices_seen > 0) {
        hint_label_->SetLabel(
            "USB cable detected — click Connect this system and pick it in the USB dialog.");
        hint_label_->Show();
        const std::string multi_meta = multi_system_meta_line(fabric_devices_seen);
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
