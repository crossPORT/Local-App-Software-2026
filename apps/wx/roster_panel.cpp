#include "roster_panel.h"

#include <sstream>
#include <vector>
#include <wx/dnd.h>
#include <wx/sizer.h>

namespace {

const wxColour kPanel(0x1a, 0x23, 0x32);
const wxColour kRow(0x12, 0x18, 0x22);
const wxColour kRowSelected(0x1c, 0x2a, 0x38);
const wxColour kDropZone(0x0f, 0x16, 0x20);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kOnlineDot(0x3d, 0xdb, 0x8a);

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

std::string receive_label(ReceiveStatus status) {
    switch (status) {
        case ReceiveStatus::Open:
            return "auto-accept";
        case ReceiveStatus::AskFirst:
            return "asks first";
        case ReceiveStatus::Busy:
            return "busy";
    }
    return "asks first";
}

std::vector<PeerEntry> visible_peers(const std::vector<PeerEntry>& peers,
                                     bool fabric_connected) {
    if (!fabric_connected) {
        return {};
    }
    std::vector<PeerEntry> online;
    for (const PeerEntry& peer : peers) {
        if (peer.online) {
            online.push_back(peer);
        }
    }
    return online;
}

}  // namespace

class RosterPanel::PeerDropZoneTarget : public wxFileDropTarget {
public:
    PeerDropZoneTarget(RosterPanel* panel, std::string peer_name)
        : panel_(panel)
        , peer_name_(std::move(peer_name)) {}

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
        if (!panel_) {
            return false;
        }
        std::vector<std::string> paths;
        for (const wxString& name : filenames) {
            paths.push_back(name.ToStdString());
        }
        panel_->DropFilesOnPeer(peer_name_, paths);
        return true;
    }

private:
    RosterPanel* panel_;
    std::string peer_name_;
};

enum { ID_PeerRowBase = wxID_HIGHEST + 500 };

RosterPanel::RosterPanel(wxWindow* parent,
                         PeerSelectedCallback on_peer_selected,
                         FilesDroppedOnPeerCallback on_files_dropped)
    : wxPanel(parent, wxID_ANY)
    , on_peer_selected_(std::move(on_peer_selected))
    , on_files_dropped_(std::move(on_files_dropped)) {
    SetBackgroundColour(kPanel);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(MakeLabel(this, "Peers", kAccent, 9, wxFONTWEIGHT_BOLD),
              0, wxLEFT | wxRIGHT | wxTOP, 12);

    peers_container_ = new wxPanel(this, wxID_ANY);
    peers_container_->SetBackgroundColour(kPanel);
    auto* container_sizer = new wxBoxSizer(wxVERTICAL);
    peers_empty_label_ = MakeLabel(peers_container_, "No peers online", kMuted, 10);
    peers_empty_label_->Wrap(400);
    container_sizer->Add(peers_empty_label_, 0, wxALL, 12);
    peers_sizer_ = new wxBoxSizer(wxVERTICAL);
    container_sizer->Add(peers_sizer_, 0, wxEXPAND);
    peers_container_->SetSizer(container_sizer);
    root->Add(peers_container_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    root->AddSpacer(4);
    SetSizer(root);
}

void RosterPanel::UpdateRoster(const std::vector<PeerEntry>& peers,
                               const IdentityProfile& self,
                               bool fabric_connected,
                               int fabric_devices_seen,
                               int port_index,
                               bool transfer_busy) {
    peers_ = peers;
    self_ = self;
    fabric_connected_ = fabric_connected;
    fabric_devices_seen_ = fabric_devices_seen;
    port_index_ = port_index;
    transfer_busy_ = transfer_busy;
    RebuildList();
}

void RosterPanel::RebuildList() {
    peers_sizer_->Clear(true);

    const std::vector<PeerEntry> visible = visible_peers(peers_, fabric_connected_);

    if (visible.empty()) {
        selected_peer_.clear();
        if (on_peer_selected_) {
            on_peer_selected_(selected_peer_);
        }
        if (peers_.empty()) {
            peers_empty_label_->SetLabel("No peers configured — check your config file");
        } else if (!fabric_connected_) {
            peers_empty_label_->SetLabel("Connect to fabric to see peers");
        } else {
            peers_empty_label_->SetLabel("No peers online");
        }
        peers_empty_label_->Show();
        peers_sizer_->Show(false);
        Layout();
        return;
    }

    peers_empty_label_->Hide();
    peers_sizer_->Show(true);

    bool selected_still_valid = false;
    for (const PeerEntry& peer : visible) {
        if (peer.display_name == selected_peer_) {
            selected_still_valid = true;
            break;
        }
    }
    if (!selected_still_valid) {
        selected_peer_ = visible.front().display_name;
    }

    int row_id = 0;
    for (const PeerEntry& peer : visible) {
        const bool selected = (peer.display_name == selected_peer_);

        auto* row = new wxPanel(peers_container_, ID_PeerRowBase + row_id++);
        row->SetBackgroundColour(selected ? kRowSelected : kRow);
        row->SetMinSize(wxSize(420, 58));

        auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);

        auto* presence_dot = new wxPanel(row,
                                         wxID_ANY,
                                         wxDefaultPosition,
                                         wxSize(8, 8),
                                         wxBORDER_NONE);
        presence_dot->SetBackgroundColour(kOnlineDot);
        presence_dot->SetMinSize(wxSize(8, 8));
        presence_dot->SetMaxSize(wxSize(8, 8));
        row_sizer->Add(presence_dot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

        auto* name_col = new wxBoxSizer(wxVERTICAL);
        auto* name_label = MakeLabel(row,
                                     peer.display_name,
                                     kText,
                                     12,
                                     wxFONTWEIGHT_BOLD);
        name_label->SetMinSize(wxSize(180, -1));
        name_col->Add(name_label, 0, wxTOP, 10);
        std::ostringstream sub;
        sub << peer.team << " · " << receive_label(peer.receive_status);
        name_col->Add(MakeLabel(row, sub.str(), kMuted, 8), 0, wxTOP, 2);
        row_sizer->Add(name_col, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

        auto* drop_zone = new wxPanel(row,
                                      wxID_ANY,
                                      wxDefaultPosition,
                                      wxSize(170, 40),
                                      wxBORDER_SIMPLE);
        drop_zone->SetBackgroundColour(kDropZone);
        drop_zone->SetMinSize(wxSize(170, 40));
        drop_zone->SetMaxSize(wxSize(170, 40));
        auto* drop_sizer = new wxBoxSizer(wxVERTICAL);
        auto* drop_hint = MakeLabel(
            drop_zone,
            transfer_busy_ ? "Transfer in progress…" : "Drop files or folder",
            transfer_busy_ ? kMuted : kAccent,
            8);
        drop_hint->Wrap(160);
        drop_sizer->AddStretchSpacer();
        drop_sizer->Add(drop_hint, 0, wxALIGN_CENTER_HORIZONTAL);
        drop_sizer->AddStretchSpacer();
        drop_zone->SetSizer(drop_sizer);
        if (!transfer_busy_) {
            drop_zone->SetDropTarget(new PeerDropZoneTarget(this, peer.display_name));
        }
        row_sizer->Add(drop_zone, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

        row->SetSizer(row_sizer);

        const std::string peer_name = peer.display_name;
        name_label->Bind(wxEVT_LEFT_UP, [this, peer_name](wxMouseEvent&) {
            OnPeerRowClicked(peer_name);
        });

        peers_sizer_->Add(row, 0, wxEXPAND | wxBOTTOM, 6);
    }

    if (on_peer_selected_) {
        on_peer_selected_(selected_peer_);
    }

    peers_container_->Layout();
    Layout();
}

void RosterPanel::SelectPeer(const std::string& display_name) {
    if (selected_peer_ == display_name) {
        return;
    }
    selected_peer_ = display_name;
    RebuildList();
}

void RosterPanel::OnPeerRowClicked(const std::string& display_name) {
    SelectPeer(display_name);
}

std::optional<std::string> RosterPanel::SelectedPeer() const {
    if (selected_peer_.empty()) {
        return std::nullopt;
    }
    return selected_peer_;
}

void RosterPanel::DropFilesOnPeer(const std::string& peer_name,
                                  const std::vector<std::string>& paths) {
    if (transfer_busy_ || paths.empty() || peer_name.empty()) {
        return;
    }
    selected_peer_ = peer_name;
    if (on_peer_selected_) {
        on_peer_selected_(peer_name);
    }
    if (on_files_dropped_) {
        on_files_dropped_(peer_name, paths);
    }
}
