#pragma once

#include "peer_roster.h"
#include "identity_profile.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <wx/wx.h>

class RosterPanel : public wxPanel {
public:
    using PeerSelectedCallback = std::function<void(const std::string& peer_name)>;
    using FilesDroppedOnPeerCallback =
        std::function<void(const std::string& peer_name, const std::vector<std::string>& paths)>;

    RosterPanel(wxWindow* parent,
                PeerSelectedCallback on_peer_selected,
                FilesDroppedOnPeerCallback on_files_dropped);

    void UpdateRoster(const std::vector<PeerEntry>& peers,
                      const IdentityProfile& self,
                      bool fabric_connected,
                      int fabric_devices_seen,
                      int port_index,
                      bool transfer_busy = false);
    std::optional<std::string> SelectedPeer() const;

private:
    void RebuildList();
    void SelectPeer(const std::string& display_name);
    void OnPeerRowClicked(const std::string& display_name);
    void DropFilesOnPeer(const std::string& peer_name, const std::vector<std::string>& paths);

    wxStaticText* peers_empty_label_ = nullptr;
    wxPanel* peers_container_ = nullptr;
    wxBoxSizer* peers_sizer_ = nullptr;
    std::vector<PeerEntry> peers_;
    IdentityProfile self_;
    bool fabric_connected_ = false;
    int fabric_devices_seen_ = 0;
    int port_index_ = 0;
    bool transfer_busy_ = false;
    std::string selected_peer_;
    PeerSelectedCallback on_peer_selected_;
    FilesDroppedOnPeerCallback on_files_dropped_;

    class PeerDropZoneTarget;
};
