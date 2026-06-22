#pragma once

#include "peer_roster.h"
#include "identity_profile.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <wx/wx.h>

class PeerDropZonePanel;

class RosterPanel : public wxPanel {
public:
    using PeerSelectedCallback = std::function<void(const std::string& peer_name)>;
    using FilesDroppedOnPeerCallback =
        std::function<void(const std::string& peer_name, const std::vector<std::string>& paths)>;
    using OpenSettingsCallback = std::function<void()>;

    RosterPanel(wxWindow* parent,
                PeerSelectedCallback on_peer_selected,
                FilesDroppedOnPeerCallback on_files_dropped,
                OpenSettingsCallback on_open_settings = {});

    void UpdateRoster(const std::vector<PeerEntry>& peers,
                      const IdentityProfile& self,
                      bool fabric_connected,
                      int fabric_devices_seen,
                      int port_index,
                      bool transfer_busy = false,
                      int64_t last_announce_ms = 0,
                      const std::string& transfer_status = "");
    std::optional<std::string> SelectedPeer() const;

private:
    struct PeerRowWidgets {
        std::string display_name;
        wxStaticText* sublabel = nullptr;
        PeerEntry peer;
    };

    void RebuildList();
    void RefreshCountdowns();
    void SelectPeer(const std::string& display_name);
    void OnPeerRowClicked(const std::string& display_name);
    void DropFilesOnPeer(const std::string& peer_name, const std::vector<std::string>& paths);
    void OnCountdownTimer(wxTimerEvent& event);
    std::string PeerSublineText(const PeerEntry& peer) const;
    std::string AnnounceLabelText() const;
    void UpdateEmptyState();
    std::vector<std::string> VisiblePeerNames() const;
    bool LayoutNeedsRebuild(const std::vector<std::string>& visible_names) const;

    struct RosterLayoutKey {
        bool fabric_connected = false;
        int fabric_devices_seen = 0;
        std::vector<std::string> visible_names;
    };

    RosterLayoutKey layout_key_{};

    friend class PeerDropZonePanel;

    wxStaticText* section_title_label_ = nullptr;
    wxStaticText* announce_label_ = nullptr;
    wxStaticText* peers_empty_label_ = nullptr;
    wxButton* settings_inline_btn_ = nullptr;
    wxPanel* peers_container_ = nullptr;
    wxBoxSizer* peers_sizer_ = nullptr;
    wxTimer* countdown_timer_ = nullptr;
    std::vector<PeerRowWidgets> peer_rows_;
    std::vector<PeerDropZonePanel*> drop_zones_;
    std::vector<PeerEntry> peers_;
    IdentityProfile self_;
    bool fabric_connected_ = false;
    int fabric_devices_seen_ = 0;
    int port_index_ = 0;
    bool transfer_busy_ = false;
    std::string transfer_status_;
    int64_t last_announce_ms_ = 0;
    std::string selected_peer_;
    PeerSelectedCallback on_peer_selected_;
    FilesDroppedOnPeerCallback on_files_dropped_;
    OpenSettingsCallback on_open_settings_;

    class PeerDropZoneTarget;
};
