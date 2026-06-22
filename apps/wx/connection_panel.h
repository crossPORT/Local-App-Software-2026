#pragma once

#include <cstdint>
#include <string>
#include <wx/wx.h>

class SpeedMonitor;

class ConnectionPanel : public wxPanel {
public:
    ConnectionPanel(wxWindow* parent);

    void ApplyState(bool fabric_connected,
                    int fabric_port_index,
                    int fabric_devices_seen,
                    const std::string& fabric_device_label,
                    bool busy,
                    double live_mbps,
                    double booth_display_mib_s,
                    int64_t last_announce_ms,
                    uint32_t fabric_activity_seq,
                    const std::string& status_message,
                    const std::string& error_message);

private:
    wxStaticText* section_label_ = nullptr;
    wxStaticText* hint_label_ = nullptr;
    wxStaticText* device_label_ = nullptr;
    wxStaticText* meta_label_ = nullptr;
    wxStaticText* warn_label_ = nullptr;
    wxStaticText* speed_label_ = nullptr;
    wxStaticText* error_label_ = nullptr;
    SpeedMonitor* activity_monitor_ = nullptr;
    wxSizer* root_sizer_ = nullptr;
    wxSizerItem* activity_monitor_item_ = nullptr;
    uint32_t last_activity_seq_ = 0;

    void RelayoutAncestors();
    void SyncActivityMonitor(bool show,
                             uint32_t fabric_activity_seq,
                             double live_mbps,
                             int64_t last_announce_ms,
                             double chart_scale_mbps = 0.0);
};
