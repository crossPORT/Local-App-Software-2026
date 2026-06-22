#include "roster_panel.h"

#include "link_status.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>
#include <vector>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>

const wxColour kPanel(0x1a, 0x23, 0x32);
const wxColour kRow(0x12, 0x18, 0x22);
const wxColour kRowSelected(0x1c, 0x2a, 0x38);
const wxColour kDropZone(0x0f, 0x16, 0x20);
const wxColour kDropZoneActive(0x0f, 0x20, 0x18);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kSubText(0xa8, 0xb8, 0xc8);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kOnlineDot(0x3d, 0xdb, 0x8a);
const wxColour kBorder(0x24, 0x30, 0x42);
const wxColour kChooseBorder(0x5a, 0x7a, 0x8a);

enum { ID_CountdownTimer = wxID_HIGHEST + 400 };

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

int64_t steady_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::vector<PeerEntry> visible_peers(const std::vector<PeerEntry>& peers,
                                     bool fabric_connected,
                                     const IdentityProfile& self,
                                     int /*local_port_index*/) {
    if (!fabric_connected) {
        return {};
    }
    std::vector<PeerEntry> online;
    for (const PeerEntry& peer : peers) {
        if (!peer.online) {
            continue;
        }
        if (peer.display_name == self.display_name) {
            continue;
        }
        online.push_back(peer);
    }
    return online;
}

class PeerDropZonePanel;

class PeerDropZonePanel : public wxPanel {
public:
    PeerDropZonePanel(RosterPanel* roster, wxWindow* parent, std::string peer_name);

    void SetTransferState(bool active, const wxString& status_hint = wxEmptyString);
    void SetPeerName(const std::string& peer_name);
    void SetDragActive(bool active);
    void AttachDropTarget();

private:
    wxString DropHintText() const {
        if (busy_) {
            return "Transfer in progress…";
        }
        if (drag_active_) {
            return "Release to send";
        }
        return wxString::Format("Drop a file to send to %s", peer_name_);
    }

    void OnChooseFile() {
        if (busy_ || !roster_) {
            return;
        }
        wxFileDialog dlg(this,
                         "Select files to send",
                         wxEmptyString,
                         wxEmptyString,
                         "All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
        wxArrayString paths;
        dlg.GetPaths(paths);
        std::vector<std::string> out;
        for (const wxString& path : paths) {
            out.push_back(path.ToStdString());
        }
        roster_->DropFilesOnPeer(peer_name_, out);
    }

    void OnPaint(wxPaintEvent&);
    void OnChooseRowPaint(wxPaintEvent&);
    void UpdateDropZoneFill();

    RosterPanel* roster_ = nullptr;
    std::string peer_name_;
    bool busy_ = false;
    bool drag_active_ = false;
    wxPanel* body_ = nullptr;
    wxPanel* hint_row_ = nullptr;
    wxStaticText* hint_label_ = nullptr;
    wxPanel* choose_row_ = nullptr;
    wxStaticText* choose_label_ = nullptr;
};

class RosterPanel::PeerDropZoneTarget : public wxFileDropTarget {
public:
    PeerDropZoneTarget(RosterPanel* panel, PeerDropZonePanel* zone, std::string peer_name)
        : panel_(panel)
        , zone_(zone)
        , peer_name_(std::move(peer_name)) {}

    wxDragResult OnEnter(wxCoord, wxCoord, wxDragResult def) override {
        if (zone_) {
            zone_->SetDragActive(true);
        }
        return def;
    }

    void OnLeave() override {
        if (zone_) {
            zone_->SetDragActive(false);
        }
    }

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
        if (zone_) {
            zone_->SetDragActive(false);
        }
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
    PeerDropZonePanel* zone_;
    std::string peer_name_;
};

PeerDropZonePanel::PeerDropZonePanel(RosterPanel* roster,
                                     wxWindow* parent,
                                     std::string peer_name)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , roster_(roster)
    , peer_name_(std::move(peer_name)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(-1, 108));
    SetBackgroundColour(kRow);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    body_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    body_->SetBackgroundColour(kDropZone);
    body_->SetMinSize(wxSize(-1, 104));

    auto* root = new wxBoxSizer(wxVERTICAL);

    // Plain panel (no border) — bare wxStaticText on dark GTK panels is invisible.
    hint_row_ = new wxPanel(body_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    hint_row_->SetBackgroundColour(kDropZone);
    auto* hint_sizer = new wxBoxSizer(wxHORIZONTAL);
    hint_sizer->AddStretchSpacer();
    hint_label_ = MakeLabel(hint_row_, DropHintText(), kAccent, 11);
    hint_label_->Wrap(460);
    hint_sizer->Add(hint_label_, 0, wxALIGN_CENTER_VERTICAL);
    hint_sizer->AddStretchSpacer();
    hint_row_->SetSizer(hint_sizer);

    root->AddStretchSpacer(1);
    root->Add(hint_row_, 0, wxEXPAND | wxLEFT | wxRIGHT, 14);
    root->AddSpacer(12);

    choose_row_ = new wxPanel(body_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    choose_row_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    choose_row_->SetBackgroundColour(kRow);
    choose_row_->SetMinSize(wxSize(148, 36));
    choose_row_->SetCursor(wxCursor(wxCURSOR_HAND));
    choose_row_->Bind(wxEVT_PAINT, &PeerDropZonePanel::OnChooseRowPaint, this);
    auto* choose_sizer = new wxBoxSizer(wxHORIZONTAL);
    choose_label_ = MakeLabel(choose_row_, "Choose file...", kText, 11);
    choose_sizer->AddStretchSpacer();
    choose_sizer->Add(choose_label_, 0, wxALIGN_CENTER_VERTICAL);
    choose_sizer->AddStretchSpacer();
    choose_row_->SetSizer(choose_sizer);
    choose_row_->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
        event.StopPropagation();
        OnChooseFile();
    });
    root->Add(choose_row_, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 14);
    root->AddSpacer(12);
    root->AddStretchSpacer(1);
    body_->SetSizer(root);

    outer->Add(body_, 1, wxEXPAND | wxALL, 2);
    SetSizer(outer);

    Bind(wxEVT_PAINT, &PeerDropZonePanel::OnPaint, this);
    AttachDropTarget();
}

void PeerDropZonePanel::UpdateDropZoneFill() {
    if (!body_) {
        return;
    }
    wxColour fill = kDropZone;
    if (drag_active_) {
        fill = kDropZoneActive;
    } else if (busy_) {
        fill = wxColour(0x14, 0x1c, 0x28);
    }
    body_->SetBackgroundColour(fill);
    if (hint_row_) {
        hint_row_->SetBackgroundColour(busy_ ? wxColour(0x14, 0x1c, 0x28) : fill);
    }
    if (hint_label_) {
        hint_label_->SetBackgroundColour(hint_row_ ? hint_row_->GetBackgroundColour() : fill);
    }
    if (choose_row_) {
        choose_row_->Refresh(false);
    }
    body_->Refresh(false);
}

void PeerDropZonePanel::OnChooseRowPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(choose_row_);
    const wxSize sz = choose_row_->GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        return;
    }
    dc.SetPen(wxPen(choose_row_->IsEnabled() ? kChooseBorder : kBorder, 1));
    dc.SetBrush(wxBrush(kRow));
    dc.DrawRoundedRectangle(0, 0, sz.x - 1, sz.y - 1, 6);
    event.Skip();
}

void PeerDropZonePanel::AttachDropTarget() {
    SetDropTarget(new RosterPanel::PeerDropZoneTarget(roster_, this, peer_name_));
}

void PeerDropZonePanel::SetTransferState(bool active, const wxString& status_hint) {
    wxString new_text;
    if (active) {
        new_text = status_hint.empty() ? "Transfer in progress…" : status_hint;
    } else {
        new_text = DropHintText();
    }

    const bool active_changed = busy_ != active;
    const bool text_changed = !hint_label_ || hint_label_->GetLabel() != new_text;
    if (!active_changed && !text_changed) {
        return;
    }
    busy_ = active;

    hint_label_->SetLabel(new_text);
    if (active) {
        wxFont font = hint_label_->GetFont();
        font.SetPointSize(12);
        font.SetWeight(wxFONTWEIGHT_BOLD);
        hint_label_->SetFont(font);
        hint_label_->SetForegroundColour(kText);
    } else {
        wxFont font = hint_label_->GetFont();
        font.SetPointSize(11);
        font.SetWeight(wxFONTWEIGHT_NORMAL);
        hint_label_->SetFont(font);
        hint_label_->SetForegroundColour(drag_active_ ? kAccent : kAccent);
    }
    if (choose_row_) {
        choose_row_->Enable(!active);
    }
    if (choose_label_) {
        choose_label_->SetForegroundColour(kText);
    }
    if (choose_row_ && choose_row_->IsShown() != !active) {
        choose_row_->Show(!active);
    }
    if (active) {
        SetDropTarget(nullptr);
    } else {
        AttachDropTarget();
    }
    UpdateDropZoneFill();
    if (hint_row_) {
        hint_row_->Refresh();
    }
    if (hint_label_) {
        hint_label_->Refresh();
    }
    if (body_) {
        body_->Layout();
    }
    Layout();
}

void PeerDropZonePanel::SetPeerName(const std::string& peer_name) {
    peer_name_ = peer_name;
    hint_label_->SetLabel(DropHintText());
}

void PeerDropZonePanel::SetDragActive(bool active) {
    if (drag_active_ == active) {
        return;
    }
    drag_active_ = active;
    hint_label_->SetLabel(DropHintText());
    if (!busy_) {
        hint_label_->SetForegroundColour(kAccent);
    }
    UpdateDropZoneFill();
    Refresh(false);
}

void PeerDropZonePanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        return;
    }

    // Border only — labels live on body_ so GTK does not paint over them.
    wxPen border_pen(drag_active_ ? kAccent : wxColour(0x00, 0xd4, 0xaa, 0x66));
    border_pen.SetStyle(wxPENSTYLE_SHORT_DASH);
    dc.SetPen(border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRoundedRectangle(1, 1, sz.x - 2, sz.y - 2, 8);
}

class PeerRowPanel : public wxPanel {
public:
    PeerRowPanel(wxWindow* parent,
                 const wxColour& fill,
                 const wxColour& border,
                 bool selected)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , fill_(fill)
        , border_(border)
        , selected_(selected) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &PeerRowPanel::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(wxPen(selected_ ? kAccent : border_));
        dc.SetBrush(wxBrush(fill_));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 10);
    }

    wxColour fill_;
    wxColour border_;
    bool selected_;
};

RosterPanel::RosterPanel(wxWindow* parent,
                         PeerSelectedCallback on_peer_selected,
                         FilesDroppedOnPeerCallback on_files_dropped,
                         OpenSettingsCallback on_open_settings)
    : wxPanel(parent, wxID_ANY)
    , on_peer_selected_(std::move(on_peer_selected))
    , on_files_dropped_(std::move(on_files_dropped))
    , on_open_settings_(std::move(on_open_settings)) {
    SetBackgroundColour(kPanel);

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* section_row = new wxBoxSizer(wxHORIZONTAL);
    section_title_label_ = MakeLabel(this, "Connected peers", kAccent, 11, wxFONTWEIGHT_BOLD);
    section_row->Add(section_title_label_, 0, wxALIGN_CENTER_VERTICAL);
    section_row->AddStretchSpacer();
    announce_label_ = MakeLabel(this, wxEmptyString, kSubText, 10);
    announce_label_->Hide();
    section_row->Add(announce_label_, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(section_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);

    peers_container_ = new wxPanel(this, wxID_ANY);
    peers_container_->SetBackgroundColour(kPanel);
    auto* container_sizer = new wxBoxSizer(wxVERTICAL);
    peers_empty_label_ = MakeLabel(peers_container_, "No peers online", kSubText, 11);
    peers_empty_label_->Wrap(520);
    container_sizer->Add(peers_empty_label_, 0, wxALL, 12);

    settings_inline_btn_ = new wxButton(peers_container_, wxID_ANY, "Open Settings");
    settings_inline_btn_->SetBackgroundColour(kRow);
    settings_inline_btn_->SetForegroundColour(kText);
    {
        const wxSize best = settings_inline_btn_->GetBestSize();
        settings_inline_btn_->SetMinSize(
            wxSize(std::max(best.GetWidth() + 12, 110), std::max(best.GetHeight(), 28)));
    }
    settings_inline_btn_->Hide();
    settings_inline_btn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (on_open_settings_) {
            on_open_settings_();
        }
    });
    container_sizer->Add(settings_inline_btn_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    peers_sizer_ = new wxBoxSizer(wxVERTICAL);
    container_sizer->Add(peers_sizer_, 0, wxEXPAND);
    peers_container_->SetSizer(container_sizer);
    root->Add(peers_container_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    root->AddSpacer(8);
    SetSizer(root);

    countdown_timer_ = new wxTimer(this, ID_CountdownTimer);
    Bind(wxEVT_TIMER, &RosterPanel::OnCountdownTimer, this, ID_CountdownTimer);
}

void RosterPanel::UpdateRoster(const std::vector<PeerEntry>& peers,
                               const IdentityProfile& self,
                               bool fabric_connected,
                               int fabric_devices_seen,
                               int port_index,
                               bool transfer_busy,
                               int64_t last_announce_ms,
                               const std::string& transfer_status) {
    peers_ = peers;
    self_ = self;
    fabric_connected_ = fabric_connected;
    fabric_devices_seen_ = fabric_devices_seen;
    port_index_ = port_index;
    transfer_busy_ = transfer_busy;
    transfer_status_ = transfer_status;
    last_announce_ms_ = last_announce_ms;

    const std::vector<std::string> visible_names = VisiblePeerNames();
    if (LayoutNeedsRebuild(visible_names)) {
        layout_key_.fabric_connected = fabric_connected_;
        layout_key_.fabric_devices_seen = fabric_devices_seen_;
        layout_key_.visible_names = visible_names;
        RebuildList();
        return;
    }

    if (visible_names.empty()) {
        UpdateEmptyState();
    }
    RefreshCountdowns();
}

std::vector<std::string> RosterPanel::VisiblePeerNames() const {
    const std::vector<PeerEntry> visible =
        visible_peers(peers_, fabric_connected_, self_, port_index_);
    std::vector<std::string> names;
    names.reserve(visible.size());
    for (const PeerEntry& peer : visible) {
        names.push_back(peer.display_name);
    }
    return names;
}

bool RosterPanel::LayoutNeedsRebuild(const std::vector<std::string>& visible_names) const {
    return layout_key_.fabric_connected != fabric_connected_
        || layout_key_.fabric_devices_seen != fabric_devices_seen_
        || layout_key_.visible_names != visible_names;
}

std::string RosterPanel::PeerSublineText(const PeerEntry& peer) const {
    std::ostringstream sub;
    sub << receive_status_label(peer.receive_status) << " · Port " << peer.port_index;
    const std::string expires = peer_expires_label(peer.last_seen, std::chrono::steady_clock::now());
    if (!expires.empty()) {
        sub << " · " << expires;
    }
    return sub.str();
}

std::string RosterPanel::AnnounceLabelText() const {
    if (!fabric_connected_ || last_announce_ms_ <= 0) {
        return {};
    }
    const int64_t remaining_ms = next_announce_in_ms(last_announce_ms_, steady_now_ms());
    const int64_t seconds = (remaining_ms + 999) / 1000;
    std::ostringstream out;
    out << "Next announce in " << seconds << "s";
    return out.str();
}

void RosterPanel::UpdateEmptyState() {
    const bool identity_configured = !self_.display_name.empty();
    const bool show_settings_action = !identity_configured && static_cast<bool>(on_open_settings_);

    if (!fabric_connected_) {
        peers_empty_label_->SetLabel(identity_configured
                                         ? "Connect USB to discover other stations"
                                         : "Set your name in Settings, then connect USB");
    } else {
        peers_empty_label_->SetLabel(
            "Listening for other stations — make sure the other app is connected too");
    }

    if (show_settings_action) {
        settings_inline_btn_->Show();
    } else {
        settings_inline_btn_->Hide();
    }
}

void RosterPanel::RebuildList() {
    peers_sizer_->Clear(true);
    peer_rows_.clear();
    drop_zones_.clear();

    if (announce_label_) {
        const std::string announce = AnnounceLabelText();
        if (announce.empty()) {
            announce_label_->Hide();
        } else {
            announce_label_->SetLabel(wxString::FromUTF8(announce.c_str()));
            announce_label_->Show();
        }
    }

    const std::vector<PeerEntry> visible =
        visible_peers(peers_, fabric_connected_, self_, port_index_);

    if (visible.empty()) {
        selected_peer_.clear();
        if (on_peer_selected_) {
            on_peer_selected_(selected_peer_);
        }
        UpdateEmptyState();
        peers_empty_label_->Show();
        peers_sizer_->Show(false);
        countdown_timer_->Stop();
        Layout();
        return;
    }

    peers_empty_label_->Hide();
    settings_inline_btn_->Hide();
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

    for (const PeerEntry& peer : visible) {
        const bool selected = (peer.display_name == selected_peer_);

        auto* row = new PeerRowPanel(peers_container_,
                                     selected ? kRowSelected : kRow,
                                     selected ? kAccent : kBorder,
                                     selected);
        auto* row_sizer = new wxBoxSizer(wxVERTICAL);

        auto* meta_row = new wxBoxSizer(wxHORIZONTAL);
        auto* presence_dot = new wxPanel(row,
                                         wxID_ANY,
                                         wxDefaultPosition,
                                         wxSize(8, 8),
                                         wxBORDER_NONE);
        presence_dot->SetBackgroundColour(kOnlineDot);
        presence_dot->SetMinSize(wxSize(8, 8));
        presence_dot->SetMaxSize(wxSize(8, 8));
        meta_row->Add(presence_dot, 0, wxALIGN_TOP | wxLEFT | wxTOP, 14);
        meta_row->AddSpacer(12);

        auto* name_col = new wxBoxSizer(wxVERTICAL);
        auto* name_label = MakeLabel(row,
                                     peer.display_name,
                                     kText,
                                     14,
                                     wxFONTWEIGHT_BOLD);
        name_col->Add(name_label, 0);
        auto* sub_label = MakeLabel(row,
                                    wxString::FromUTF8(PeerSublineText(peer).c_str()),
                                    kSubText,
                                    11);
        name_col->Add(sub_label, 0, wxTOP, 4);
        meta_row->Add(name_col, 1, wxEXPAND | wxTOP | wxRIGHT, 14);

        row_sizer->Add(meta_row, 0, wxEXPAND);

        auto* drop_zone = new PeerDropZonePanel(this, row, peer.display_name);
        drop_zone->SetTransferState(
            transfer_busy_,
            transfer_busy_ ? wxString::FromUTF8(transfer_status_.c_str()) : wxString());
        row_sizer->Add(drop_zone, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 12);

        row->SetSizer(row_sizer);

        peer_rows_.push_back({peer.display_name, sub_label, peer});
        drop_zones_.push_back(drop_zone);

        const std::string peer_name = peer.display_name;
        auto select_peer = [this, peer_name](wxMouseEvent& event) {
            event.Skip();
            OnPeerRowClicked(peer_name);
        };
        row->Bind(wxEVT_LEFT_UP, select_peer);
        name_label->Bind(wxEVT_LEFT_UP, select_peer);
        sub_label->Bind(wxEVT_LEFT_UP, select_peer);
        presence_dot->Bind(wxEVT_LEFT_UP, select_peer);

        peers_sizer_->Add(row, 0, wxEXPAND | wxBOTTOM, 10);
    }

    if (on_peer_selected_) {
        on_peer_selected_(selected_peer_);
    }

    if (!countdown_timer_->IsRunning()) {
        countdown_timer_->Start(1000);
    }

    peers_container_->Layout();
    Layout();
}

void RosterPanel::RefreshCountdowns() {
    if (announce_label_) {
        const std::string announce = AnnounceLabelText();
        if (announce.empty()) {
            announce_label_->Hide();
        } else {
            announce_label_->SetLabel(wxString::FromUTF8(announce.c_str()));
            announce_label_->Show();
        }
    }

    for (PeerDropZonePanel* zone : drop_zones_) {
        if (zone) {
            zone->SetTransferState(
                transfer_busy_,
                transfer_busy_ ? wxString::FromUTF8(transfer_status_.c_str()) : wxString());
        }
    }

    const auto now = std::chrono::steady_clock::now();
    for (PeerRowWidgets& row : peer_rows_) {
        if (!row.sublabel) {
            continue;
        }
        for (const PeerEntry& peer : peers_) {
            if (peer.display_name == row.display_name) {
                row.peer = peer;
                break;
            }
        }
        row.sublabel->SetLabel(wxString::FromUTF8(PeerSublineText(row.peer).c_str()));
    }
}

void RosterPanel::OnCountdownTimer(wxTimerEvent&) {
    RefreshCountdowns();
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
