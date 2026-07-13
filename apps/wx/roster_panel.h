#pragma once

#include "peer_roster.h"
#include "identity_profile.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <wx/wx.h>

class PeerDropZonePanel;

/** Peer-card link glyph: absent / attempting / established. */
enum class RosterLinkIcon { None, Linking, Linked };

class RosterPanel : public wxPanel {
public:
    using PeerSelectedCallback = std::function<void(const std::string& peer_name)>;
    using FilesDroppedOnPeerCallback =
        std::function<void(const std::string& peer_name, const std::vector<std::string>& paths)>;
    using OpenSettingsCallback = std::function<void()>;
    using ReleaseLinkCallback =
        std::function<void(const std::string& peer_name, int display_port)>;

    RosterPanel(wxWindow* parent,
                PeerSelectedCallback on_peer_selected,
                FilesDroppedOnPeerCallback on_files_dropped,
                OpenSettingsCallback on_open_settings = {},
                ReleaseLinkCallback on_release_link = {});

    void UpdateRoster(const std::vector<PeerEntry>& peers,
                      const IdentityProfile& self,
                      bool fabric_connected,
                      int fabric_devices_seen,
                      int port_index,
                      bool transfer_busy = false,
                      int64_t last_announce_ms = 0,
                      const std::string& transfer_status = "",
                      int linked_display_port = 0,
                      RosterLinkIcon link_icon = RosterLinkIcon::None);
    std::optional<std::string> SelectedPeer() const;

private:
    struct PeerRowWidgets {
        int leg = -1;
        bool offline = false;
        std::string display_name;
        wxStaticText* name_label = nullptr;
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
    void SyncPeersContainerHeight();
    std::vector<std::string> SlotSignature() const;
    bool LayoutNeedsRebuild(const std::vector<std::string>& slot_signature) const;

    struct RosterLayoutKey {
        bool fabric_connected = false;
        int local_leg = -1;
        std::vector<std::string> slot_signature;
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
    int linked_display_port_ = 0;
    RosterLinkIcon link_icon_ = RosterLinkIcon::None;
    std::string selected_peer_;
    PeerSelectedCallback on_peer_selected_;
    FilesDroppedOnPeerCallback on_files_dropped_;
    OpenSettingsCallback on_open_settings_;
    ReleaseLinkCallback on_release_link_;

    class PeerDropZoneTarget;
};
