#pragma once

#include "session_rate_stats.h"
#include "speed_monitor.h"

#include <cstdint>
#include <functional>
#include <string>
#include <wx/wx.h>

class SpeedMonitor;

class ConnectionPanel : public wxPanel {
public:
    ConnectionPanel(wxWindow* parent);

    void SetActionHandlers(std::function<void()> on_connect, std::function<void()> on_disconnect);
    void SetLayoutChangedHandler(std::function<void()> on_layout_changed);

    void ApplyState(bool usb_connected,
                    int usb_port_index,
                    int devices_seen,
                    const std::string& device_label,
                    bool busy,
                    double live_mbps,
                    double display_rate_mib_s,
                    double result_mbps,
                    int64_t last_announce_ms,
                    uint32_t usb_activity_seq,
                    const std::string& status_message,
                    const std::string& error_message);

private:
    wxStaticText* section_label_ = nullptr;
    wxStaticText* hint_label_ = nullptr;
    wxStaticText* device_label_ = nullptr;
    wxStaticText* meta_label_ = nullptr;
    wxStaticText* warn_label_ = nullptr;
    wxStaticText* error_label_ = nullptr;
    SpeedMonitor* activity_monitor_ = nullptr;
    wxButton* connect_btn_ = nullptr;
    wxPanel* disconnect_btn_ = nullptr;
    wxSizer* root_sizer_ = nullptr;
    wxSizerItem* activity_monitor_item_ = nullptr;
    wxSizerItem* connect_item_ = nullptr;
    wxSizerItem* disconnect_item_ = nullptr;
    std::function<void()> on_connect_;
    std::function<void()> on_disconnect_;
    std::function<void()> on_layout_changed_;
    uint32_t last_activity_seq_ = 0;
    double last_live_mbps_ = 0.0;
    int last_stats_count_ = 0;
    SessionRateTracker rate_tracker_;

    void RelayoutAncestors();
    void SyncConnectActions(bool usb_connected);
    void SyncActivityMonitor(bool show,
                             uint32_t usb_activity_seq,
                             double live_mbps,
                             double result_mbps,
                             double display_rate_mib_s);
    void SyncPanelMinSize();
};
