#include "connection_panel.h"

#include "speed_monitor.h"

#include <algorithm>
#include <wx/sizer.h>

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
    if (!connect_btn_ || !clear_btn_ || !disconnect_btn_ || !connect_item_ || !disconnect_item_) {
        return;
    }
    if (fabric_connected) {
        connect_btn_->Hide();
        clear_btn_->Hide();
        connect_item_->Show(false);
        disconnect_btn_->Show();
        disconnect_item_->Show(true);
        disconnect_item_->SetMinSize(wxSize(-1, 34));
    } else {
        disconnect_btn_->Hide();
        disconnect_item_->Show(false);
        connect_btn_->Show();
        clear_btn_->Show();
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
