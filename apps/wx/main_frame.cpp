#include "main_frame.h"

#include "booth_log.h"
#include "incoming_dialog.h"
#include "peer_roster.h"
#include "roster_panel.h"
#include "settings_dialog.h"
#include "transfer_progress_panel.h"
#include "event_log_dialog.h"
#include "usb_transfer.h"

#include <algorithm>
#include <csignal>
#include <sstream>

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/dcclient.h>

namespace {

const wxColour kBg(0x0f, 0x14, 0x19);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);
const wxColour kError(0xff, 0x6b, 0x6b);
const wxColour kWarn(0xff, 0x9f, 0x43);
const wxColour kOk(0x3d, 0xdb, 0x8a);
const wxColour kHeader(0x1a, 0x23, 0x32);
const wxColour kIconBox(0x12, 0x18, 0x22);

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, const wxColour& fg, int pt = 10) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

wxPanel* MakeConnectionIndicator(wxWindow* parent) {
    constexpr int kSize = 14;
    auto* indicator = new wxPanel(parent,
                                  wxID_ANY,
                                  wxDefaultPosition,
                                  wxSize(kSize, kSize),
                                  wxBORDER_SIMPLE);
    indicator->SetBackgroundColour(kError);
    indicator->SetMinSize(wxSize(kSize, kSize));
    indicator->SetMaxSize(wxSize(kSize, kSize));
    indicator->SetToolTip("Offline");
    return indicator;
}

void SetConnectionIndicator(wxPanel* indicator, const wxColour& colour, const wxString& tooltip) {
    if (!indicator) {
        return;
    }
    indicator->SetBackgroundColour(colour);
    indicator->SetToolTip(tooltip);
    indicator->Refresh();
}

}  // namespace

enum {
    ID_Settings = wxID_HIGHEST + 1,
    ID_LoopbackDev,
    ID_UsbDiagnostics,
    ID_EventLog,
    ID_LedPulseTimer,
    ID_ShutdownPollTimer,
};

// SIGTERM/SIGINT (e.g. `pkill`, Ctrl+C) must run the same graceful shutdown as
// closing the window — otherwise libusb never cancels the in-flight transfer and
// the fabric data endpoint can wedge. The signal handler only sets a flag
// (async-signal-safe); a poll timer on the main thread performs the real Close().
namespace {
volatile std::sig_atomic_t g_shutdown_requested = 0;
void HandleTermSignal(int) {
    g_shutdown_requested = 1;
}
}  // namespace

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_SHOW(MainFrame::OnFirstShow)
    EVT_MENU(ID_Settings, MainFrame::OnSettingsMenu)
    EVT_MENU(ID_EventLog, MainFrame::OnEventLogMenu)
    EVT_TIMER(ID_LedPulseTimer, MainFrame::OnLedPulseTimer)
    EVT_TIMER(ID_ShutdownPollTimer, MainFrame::OnShutdownPollTimer)
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(int port_index, const std::string& config_path)
    : wxFrame(nullptr,
              wxID_ANY,
              "RocketBox Transfer",
              wxDefaultPosition,
              wxDefaultSize)
    , port_index_(port_index)
    , config_path_(config_path) {
    SetBackgroundColour(kBg);

    if (config_path_.empty()) {
        config_path_ = "booth-port" + std::to_string(port_index_) + ".conf";
    }
    load_identity_profile(port_index_, config_path_, identity_);
    if (!identity_.config_path.empty()) {
        config_path_ = identity_.config_path;
    }

    UpdateWindowTitle();
    BuildMenuBar();
    BuildUi();
    roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers), identity_, false, 0, port_index_);

    orchestrator_ = std::make_unique<TransferOrchestrator>(
        port_index_,
        identity_,
        [this](const OrchestratorUiState& state) {
            auto snapshot = std::make_shared<OrchestratorUiState>(state);
            wxTheApp->CallAfter([this, snapshot]() {
                if (!IsBeingDeleted()) {
                    ApplyOrchestratorState(*snapshot);
                }
            });
        });

    Layout();
    const wxSize fit = GetSizer()->ComputeFittingClientSize(this);
    content_width_ = std::max(fit.GetWidth(), 460);
    SetClientSize(wxSize(content_width_, fit.GetHeight()));
    SetMinClientSize(wxSize(content_width_, 0));
    Centre();

    std::signal(SIGTERM, HandleTermSignal);
    std::signal(SIGINT, HandleTermSignal);
    shutdown_poll_timer_ = new wxTimer(this, ID_ShutdownPollTimer);
    shutdown_poll_timer_->Start(200);
}

void MainFrame::OnShutdownPollTimer(wxTimerEvent&) {
    if (g_shutdown_requested) {
        shutdown_poll_timer_->Stop();
        Close(true);
    }
}

void MainFrame::OnFirstShow(wxShowEvent& event) {
    event.Skip();
    if (orchestrator_started_) {
        return;
    }
    orchestrator_started_ = true;

    const wxRect screen = wxDisplay().GetClientArea();
    const int x = screen.GetLeft() + 40 + port_index_ * 520;
    const int y = screen.GetTop() + 40 + port_index_ * 40;
    SetPosition(wxPoint(x, y));
    Iconize(false);
    Raise();
    SetFocus();

    roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers), identity_, false, 0, port_index_);

    if (orchestrator_) {
        // Match booth-cli: skip the wiring probe. Session transfers use the
        // fabric's native bidirectional routing.
        orchestrator_->start(/*run_wiring_probe=*/false);
        booth_log(port_index_, "gui_start", config_path_);
    }
}

void MainFrame::BuildMenuBar() {
    auto* menu_bar = new wxMenuBar();
    auto* file_menu = new wxMenu();
    file_menu->Append(ID_Settings, "Settings\tCtrl+,");
    file_menu->Append(ID_EventLog, "Log\tCtrl+L");

    menu_bar->Append(file_menu, "&File");
    SetMenuBar(menu_bar);
}

void MainFrame::BuildUi() {
    auto* root_panel = new wxPanel(this, wxID_ANY);
    root_panel->SetBackgroundColour(kBg);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxPanel(root_panel, wxID_ANY);
    header->SetBackgroundColour(kHeader);
    auto* header_sizer = new wxBoxSizer(wxVERTICAL);
    auto* title_row = new wxBoxSizer(wxHORIZONTAL);

    auto* brand_label = MakeLabel(header, "RocketBox", kText, 18);
    {
        wxFont font = brand_label->GetFont();
        font.SetWeight(wxFONTWEIGHT_BOLD);
        brand_label->SetFont(font);
    }
    wxClientDC dc(header);
    dc.SetFont(brand_label->GetFont());
    const wxSize brand_extent = dc.GetTextExtent("RocketBox");
    const int brand_h = brand_extent.GetHeight();
    const int icon_box_size = brand_h + 8;

    auto* icon_box = new wxPanel(header,
                                 wxID_ANY,
                                 wxDefaultPosition,
                                 wxSize(icon_box_size, icon_box_size),
                                 wxBORDER_SIMPLE);
    icon_box->SetBackgroundColour(kIconBox);
    icon_box->SetMinSize(wxSize(icon_box_size, icon_box_size));
    icon_box->SetMaxSize(wxSize(icon_box_size, icon_box_size));
    auto* icon_sizer = new wxBoxSizer(wxVERTICAL);
    auto* icon_label = MakeLabel(icon_box, wxString::FromUTF8("🚀"), kText, 14);
    icon_sizer->AddStretchSpacer();
    icon_sizer->Add(icon_label, 0, wxALIGN_CENTER_HORIZONTAL);
    icon_sizer->AddStretchSpacer();
    icon_box->SetSizer(icon_sizer);

    title_row->Add(icon_box, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP, 12);
    title_row->Add(brand_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP, 10);
    title_row->AddStretchSpacer();

    node_name_label_ = MakeLabel(header,
                                 wxString::FromUTF8(identity_.display_name.c_str()),
                                 kMuted,
                                 11);
    title_row->Add(node_name_label_, 0, wxALIGN_CENTER_VERTICAL | wxTOP, 12);

    connection_indicator_ = MakeConnectionIndicator(header);
    title_row->Add(connection_indicator_,
                   0,
                   wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxTOP,
                   8);

    header_sizer->Add(title_row, 0, wxEXPAND);
    RenderConnectionIndicator();

    status_message_label_ = MakeLabel(header, "Offline — plug in fabric to begin", kMuted, 9);
    status_message_label_->Wrap(480);
    header_sizer->Add(status_message_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    roster_panel_ = new RosterPanel(
        header,
        [this](const std::string& peer) { OnPeerSelected(peer); },
        [this](const std::string& peer, const std::vector<std::string>& paths) {
            OnPeerSelected(peer);
            OnSendRequested(paths, "");
        });
    header_sizer->Add(roster_panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    header->SetSizer(header_sizer);
    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

    progress_panel_ = new TransferProgressPanel(root_panel);
    root->Add(progress_panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    led_pulse_timer_ = new wxTimer(this, ID_LedPulseTimer);

    root_panel->SetSizer(root);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(root_panel, 1, wxEXPAND);
    SetSizer(outer);
}

void MainFrame::UpdateWindowTitle() {
    SetTitle(wxString::FromUTF8(("RocketBox — " + identity_.display_name).c_str()));
    if (node_name_label_) {
        node_name_label_->SetLabel(wxString::FromUTF8(identity_.display_name.c_str()));
    }
}

void MainFrame::RenderConnectionIndicator() {
    if (!connection_indicator_) {
        return;
    }
    wxColour colour = kError;
    wxString tooltip = "Offline";
    switch (link_led_) {
        case LinkLed::Offline:
            colour = kError;
            tooltip = "Offline";
            break;
        case LinkLed::Announcing:
            colour = kWarn;
            tooltip = "Announcing…";
            break;
        case LinkLed::Connected:
            colour = kOk;
            tooltip = "Connected to fabric";
            break;
        case LinkLed::Transferring:
            colour = led_pulse_on_ ? kOk : wxColour(0x2a, 0x9d, 0x6f);
            tooltip = "Transfer in progress";
            break;
    }
    SetConnectionIndicator(connection_indicator_, colour, tooltip);
}

void MainFrame::SyncLedPulseTimer(bool pulse) {
    if (!led_pulse_timer_) {
        return;
    }
    if (pulse) {
        if (!led_pulse_timer_->IsRunning()) {
            led_pulse_timer_->Start(500);
        }
    } else {
        led_pulse_timer_->Stop();
        led_pulse_on_ = false;
        RenderConnectionIndicator();
    }
}

void MainFrame::OnLedPulseTimer(wxTimerEvent&) {
    led_pulse_on_ = !led_pulse_on_;
    RenderConnectionIndicator();
}

void MainFrame::UpdateConnectionStatus(const OrchestratorUiState& state) {
    const bool usb_seen =
        state.fabric_port_open || state.fabric_devices_seen > 0;

    if (state.busy && state.fabric_connected) {
        link_led_ = LinkLed::Transferring;
    } else if (state.fabric_connected) {
        link_led_ = LinkLed::Connected;
    } else if (usb_seen) {
        link_led_ = LinkLed::Announcing;
    } else {
        link_led_ = LinkLed::Offline;
    }
    RenderConnectionIndicator();
    SyncLedPulseTimer(state.busy && state.fabric_connected);

    if (!status_message_label_) {
        return;
    }

    if (state.busy) {
        status_message_label_->SetLabel(
            state.status_message.empty() ? "Transfer in progress…" : state.status_message);
        status_message_label_->SetForegroundColour(kAccent);
    } else if (state.fabric_connected) {
        status_message_label_->SetLabel("Connected to fabric");
        status_message_label_->SetForegroundColour(kOk);
    } else if (state.fabric_devices_seen == 0) {
        status_message_label_->SetLabel("Offline — plug in fabric when you're ready");
        status_message_label_->SetForegroundColour(kMuted);
    } else if (port_index_ >= state.fabric_devices_seen) {
        status_message_label_->SetLabel(
            "Loopback needs two cables — this window expects port "
            + std::to_string(port_index_));
        status_message_label_->SetForegroundColour(kWarn);
    } else {
        status_message_label_->SetLabel(
            "USB visible — waiting for fabric handshake to complete");
        status_message_label_->SetForegroundColour(kWarn);
    }
}

void MainFrame::ApplyOrchestratorState(const OrchestratorUiState& state) {
    identity_ = state.identity;
    UpdateWindowTitle();
    UpdateConnectionStatus(state);
    roster_panel_->UpdateRoster(state.roster, state.identity, state.fabric_connected,
                                state.fabric_devices_seen, port_index_, state.busy);

    progress_panel_->ApplyState(state.busy,
                                state.waiting_for_partner,
                                state.fabric_connected,
                                state.bytes_done,
                                state.bytes_total,
                                state.live_mbps,
                                state.status_message,
                                state.notification,
                                state.error_message);
    FitToContent();
    MaybeShowIncomingDialog(state);
}

void MainFrame::FitToContent() {
    if (!GetSizer()) {
        return;
    }
    Layout();
    const wxSize fit = GetSizer()->ComputeFittingClientSize(this);
    const wxSize cur = GetClientSize();
    if (cur.GetHeight() != fit.GetHeight() || cur.GetWidth() != content_width_) {
        SetClientSize(wxSize(content_width_, fit.GetHeight()));
    }
}

void MainFrame::MaybeShowIncomingDialog(const OrchestratorUiState& state) {
    if (!state.pending_offer) {
        shown_offer_id_.reset();
        return;
    }
    const std::string& offer_id = state.pending_offer->message.session_id;
    if (shown_offer_id_ && *shown_offer_id_ == offer_id) {
        return;
    }
    shown_offer_id_ = offer_id;

    IncomingDialog dlg(this, *state.pending_offer, [this](bool accepted) {
        wxTheApp->CallAfter([this, accepted]() {
            if (!orchestrator_) {
                return;
            }
            if (accepted) {
                orchestrator_->accept_pending_offer();
            } else {
                orchestrator_->decline_pending_offer();
            }
        });
    });
    dlg.ShowModal();
}

void MainFrame::OnPeerSelected(const std::string& peer_name) {
    selected_peer_ = peer_name;
}

void MainFrame::OnSendRequested(const std::vector<std::string>& paths,
                                const std::string& note) {
    if (!orchestrator_) {
        return;
    }
    if (orchestrator_->is_busy()) {
        return;
    }

    std::vector<std::string> send_paths = paths;
    if (send_paths.empty()) {
        send_paths = PromptForSendPaths();
        if (send_paths.empty()) {
            return;
        }
    }

    if (selected_peer_.empty()) {
        if (const auto peer = roster_panel_->SelectedPeer()) {
            selected_peer_ = *peer;
        }
    }
    if (selected_peer_.empty()) {
        wxMessageBox("Wait for a peer to appear, then try again.",
                     "No peer",
                     wxOK | wxICON_INFORMATION,
                     this);
        return;
    }
    {
        const OrchestratorUiState snap = orchestrator_->snapshot();
        for (const PeerEntry& peer : snap.roster) {
            if (peer.display_name == selected_peer_ && !peer.online) {
                wxMessageBox(selected_peer_ + " is not connected yet.",
                             "Peer offline",
                             wxOK | wxICON_INFORMATION,
                             this);
                return;
            }
        }
        if (!snap.fabric_connected) {
            wxMessageBox("Fabric is not ready yet — check the status at top right.",
                         "Fabric offline",
                         wxOK | wxICON_INFORMATION,
                         this);
            return;
        }
    }
    if (!orchestrator_->send_to_peer(selected_peer_, send_paths, note)) {
        const OrchestratorUiState snap = orchestrator_->snapshot();
        if (!snap.error_message.empty()) {
            wxMessageBox(wxString::FromUTF8(snap.error_message.c_str()),
                         "Send failed",
                         wxOK | wxICON_ERROR,
                         this);
        }
        return;
    }
}

std::vector<std::string> MainFrame::PromptForSendPaths() {
    enum { ID_ChooseFiles = wxID_HIGHEST + 200, ID_ChooseFolder };

    wxDialog dlg(this,
                 wxID_ANY,
                 "Send files",
                 wxDefaultPosition,
                 wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);
    dlg.SetBackgroundColour(kBg);

    auto* prompt = MakeLabel(&dlg, "What would you like to send?", kText, 11);
    auto* files_btn = new wxButton(&dlg, ID_ChooseFiles, "Choose files…");
    auto* folder_btn = new wxButton(&dlg, ID_ChooseFolder, "Choose folder…");
    auto* cancel_btn = new wxButton(&dlg, wxID_CANCEL, "Cancel");

    auto* btn_row = new wxBoxSizer(wxHORIZONTAL);
    btn_row->Add(cancel_btn, 0, wxRIGHT, 8);
    btn_row->AddStretchSpacer();
    btn_row->Add(files_btn, 0, wxRIGHT, 8);
    btn_row->Add(folder_btn, 0);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(prompt, 0, wxALL, 16);
    sizer->Add(btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    dlg.SetSizer(sizer);
    dlg.Fit();

    int choice = wxID_CANCEL;
    dlg.Bind(wxEVT_BUTTON, [&choice, &dlg](wxCommandEvent& event) {
        choice = event.GetId();
        dlg.EndModal(choice);
    });

    if (dlg.ShowModal() == wxID_CANCEL) {
        return {};
    }
    if (choice == ID_ChooseFiles) {
        return BrowseFilesDialog();
    }
    if (choice == ID_ChooseFolder) {
        return BrowseFolderDialog();
    }
    return {};
}

std::vector<std::string> MainFrame::BrowseFilesDialog() {
    wxFileDialog dlg(this,
                     "Select files to send",
                     wxEmptyString,
                     wxEmptyString,
                     "All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dlg.ShowModal() != wxID_OK) {
        return {};
    }
    wxArrayString paths;
    dlg.GetPaths(paths);
    std::vector<std::string> staged;
    for (const wxString& path : paths) {
        staged.push_back(path.ToStdString());
    }
    return staged;
}

std::vector<std::string> MainFrame::BrowseFolderDialog() {
    wxDirDialog dlg(this, "Select folder to send", wxEmptyString, wxDD_DEFAULT_STYLE);
    if (dlg.ShowModal() != wxID_OK) {
        return {};
    }
    return {dlg.GetPath().ToStdString()};
}

void MainFrame::OnBrowseFiles() {
    const std::vector<std::string> staged = BrowseFilesDialog();
    if (!staged.empty()) {
        OnSendRequested(staged, "");
    }
}

void MainFrame::OnBrowseFolder() {
    const std::vector<std::string> staged = BrowseFolderDialog();
    if (!staged.empty()) {
        OnSendRequested(staged, "");
    }
}

void MainFrame::OnSettingsMenu(wxCommandEvent&) {
    OnSettings();
}

void MainFrame::OnSettings() {
    SettingsDevActions dev;
    dev.on_loopback = [this]() { OnLoopbackDev(); };
    dev.on_diagnostics = [this]() { OnUsbDiagnostics(); };
    dev.on_event_log = [this]() { OnEventLog(); };

    SettingsDialog dlg(this, orchestrator_->identity(), [this](const IdentityProfile& profile) {
        if (orchestrator_) {
            orchestrator_->set_identity(profile);
            identity_ = profile;
            config_path_ = profile.config_path;
            UpdateWindowTitle();
            const OrchestratorUiState snap = orchestrator_->snapshot();
            roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers),
                                        identity_,
                                        snap.fabric_connected,
                                        snap.fabric_devices_seen,
                                        port_index_);
        }
    }, dev);
    dlg.ShowModal();
}

void MainFrame::OnLoopbackDev() {
    if (!orchestrator_ || orchestrator_->is_busy()) {
        return;
    }
    wxFileDialog dlg(this,
                     "Loopback test file",
                     wxEmptyString,
                     wxEmptyString,
                     "All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        orchestrator_->run_loopback_test(dlg.GetPath().ToStdString());
    }
}

void MainFrame::OnUsbDiagnostics() {
    std::ostringstream text;
    text << "Fabric port index: " << port_index_ << "\n"
         << "Config: " << (config_path_.empty() ? "(none)" : config_path_) << "\n"
         << "usbfs limit: " << usbfs_limit_mb() << " MB (kernel)\n"
         << "In-flight depth: " << inflight_queue_depth() << " chunks\n"
         << "Payload timeout: " << payload_timeout_ms() << " ms\n"
         << "Event log: " << booth_log_path() << "\n"
         << "(View → Event Log or Settings → Event log…)";
    wxMessageBox(wxString::FromUTF8(text.str().c_str()),
                 "USB diagnostics",
                 wxOK | wxICON_INFORMATION,
                 this);
}

void MainFrame::OnEventLogMenu(wxCommandEvent&) {
    OnEventLog();
}

void MainFrame::OnEventLog() {
    if (event_log_dialog_) {
        event_log_dialog_->Raise();
        event_log_dialog_->Show();
        return;
    }
    event_log_dialog_ = new EventLogDialog(this, port_index_);
    event_log_dialog_->Bind(wxEVT_DESTROY, [this](wxWindowDestroyEvent&) {
        event_log_dialog_ = nullptr;
    });
    event_log_dialog_->Show();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (shutdown_poll_timer_ && shutdown_poll_timer_->IsRunning()) {
        shutdown_poll_timer_->Stop();
    }
    if (led_pulse_timer_ && led_pulse_timer_->IsRunning()) {
        led_pulse_timer_->Stop();
    }
    if (orchestrator_) {
        orchestrator_->stop();
        orchestrator_.reset();
    }
    event.Skip();
}
