#include "main_frame.h"

#include "app_icon.h"
#include "booth_log.h"
#include "connection_panel.h"
#include "fabric_device_picker.h"
#include "session_handshake.h"
#include "incoming_dialog.h"
#include "link_status.h"
#include "peer_roster.h"
#include "roster_panel.h"
#include "settings_dialog.h"
#include "transfer_progress_panel.h"
#include "event_log_dialog.h"
#include "usb_transfer.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <initializer_list>
#include <sstream>
#include <thread>

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>

#include <libusb-1.0/libusb.h>

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
const wxColour kCard(0x12, 0x18, 0x22);
const wxColour kBorder(0x24, 0x30, 0x42);

wxColour StatusColourToWx(StatusColour colour) {
    switch (colour) {
        case StatusColour::Accent:
            return kAccent;
        case StatusColour::Ok:
            return kOk;
        case StatusColour::Warn:
            return kWarn;
        case StatusColour::Error:
            return kError;
        case StatusColour::Muted:
        default:
            return kMuted;
    }
}

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, const wxColour& fg, int pt = 10) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    label->SetFont(font);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

class IconButton : public wxPanel {
public:
    explicit IconButton(wxWindow* parent, wxWindowID id)
        : wxPanel(parent, id, wxDefaultPosition, wxSize(32, 32), wxBORDER_NONE) {
        SetMinSize(wxSize(32, 32));
        SetMaxSize(wxSize(32, 32));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kHeader);
        SetCursor(wxCursor(wxCURSOR_HAND));
        Bind(wxEVT_PAINT, &IconButton::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(kHeader));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
        dc.SetPen(wxPen(kBorder));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, 8);

        dc.SetPen(wxPen(kMuted));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        const int cx = sz.x / 2;
        const int cy = sz.y / 2;
        dc.DrawCircle(cx, cy, 6);
        for (int i = 0; i < 8; ++i) {
            const double angle = i * 3.141592653589793 / 4.0;
            const int x1 = cx + static_cast<int>(7.0 * std::cos(angle));
            const int y1 = cy + static_cast<int>(7.0 * std::sin(angle));
            const int x2 = cx + static_cast<int>(10.0 * std::cos(angle));
            const int y2 = cy + static_cast<int>(10.0 * std::sin(angle));
            dc.DrawLine(x1, y1, x2, y2);
        }
        dc.DrawCircle(cx, cy, 2);
    }
};

class RoundedPanel : public wxPanel {
public:
    RoundedPanel(wxWindow* parent,
                 const wxColour& fill,
                 const wxColour& border,
                 int radius)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , fill_(fill)
        , border_(border)
        , radius_(radius) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &RoundedPanel::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) {
            return;
        }
        dc.SetPen(wxPen(border_));
        dc.SetBrush(wxBrush(fill_));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, radius_);
    }

    wxColour fill_;
    wxColour border_;
    int radius_;
};

class ConnectionLedPanel : public wxPanel {
public:
    ConnectionLedPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(14, 14), wxBORDER_NONE) {
        SetMinSize(wxSize(14, 14));
        SetMaxSize(wxSize(14, 14));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ConnectionLedPanel::OnPaint, this);
    }

    void SetLedColour(const wxColour& colour) {
        if (colour_ != colour) {
            colour_ = colour;
            Refresh();
        }
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetPen(wxPen(wxColour(255, 255, 255, 38)));
        dc.SetBrush(wxBrush(colour_));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
    }

    wxColour colour_{kError};
};

ConnectionLedPanel* MakeConnectionIndicator(wxWindow* parent) {
    auto* indicator = new ConnectionLedPanel(parent);
    indicator->SetToolTip("Offline");
    return indicator;
}

void SetConnectionIndicator(ConnectionLedPanel* indicator,
                             const wxColour& colour,
                             const wxString& tooltip) {
    if (!indicator) {
        return;
    }
    indicator->SetLedColour(colour);
    indicator->SetToolTip(tooltip);
}

}  // namespace

enum {
    ID_Settings = wxID_HIGHEST + 1,
    ID_SettingsBtn,
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

MainFrame::MainFrame(const std::string& config_path, int cli_port_index)
    : wxFrame(nullptr,
              wxID_ANY,
              "RocketBox App",
              wxDefaultPosition,
              wxDefaultSize)
    , cli_port_index_(cli_port_index)
    , config_path_(config_path) {
    SetBackgroundColour(kBg);

    const int config_port = cli_port_index_ >= 0 ? cli_port_index_ : 0;
    load_identity_profile(config_port, config_path_, identity_);
    if (!identity_.config_path.empty()) {
        config_path_ = identity_.config_path;
    }

    UpdateWindowTitle();
    BuildUi();
    ApplyRocketBoxFrameIcon(this);
    roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers),
                                identity_,
                                false,
                                0,
                                0,
                                false,
                                0);

    Layout();
    const wxSize fit = GetSizer()->ComputeFittingClientSize(this);
    content_width_ = std::max(fit.GetWidth(), 560);
    SetClientSize(wxSize(content_width_, fit.GetHeight()));
    SetMinClientSize(wxSize(560, fit.GetHeight()));
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
    SetPosition(wxPoint(screen.GetLeft() + 40, screen.GetTop() + 40));
    Iconize(false);
    Raise();
    SetFocus();

    if (!StartOrchestrator()) {
        const int devices = CountFabricDevices();
        last_fabric_devices_seen_ = devices;
        if (connection_panel_) {
            connection_panel_->ApplyState(
                false,
                -1,
                devices,
                {},
                false,
                0.0,
                0.0,
                0.0,
                0,
                0,
                {},
                devices > 0 ? "USB cable detected — choose it when prompted."
                            : "No USB cable selected — plug in your device and click Connect USB.");
        }
        FitToContent();
        return;
    }

    roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers),
                                identity_,
                                false,
                                0,
                                port_index_,
                                false,
                                0);

    FitToContent();

    if (std::getenv("ROCKETBOX_SMOKE_SETTINGS")) {
        CallAfter([this]() {
            OnSettings();
            Close(true);
        });
    }
}

int MainFrame::ResolveFabricPortIndex() {
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        return -1;
    }
    const std::vector<FabricUsbDevice> devices = list_fabric_devices(ctx);
    libusb_exit(ctx);

    if (devices.empty()) {
        return -1;
    }
    if (cli_port_index_ >= 0) {
        if (cli_port_index_ >= static_cast<int>(devices.size())) {
            return -1;
        }
        return cli_port_index_;
    }
    if (devices.size() == 1) {
        return 0;
    }
    return pick_fabric_port_index(this, devices);
}

int MainFrame::CountFabricDevices() const {
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        return 0;
    }
    const std::vector<FabricUsbDevice> devices = list_fabric_devices(ctx);
    libusb_exit(ctx);
    return static_cast<int>(devices.size());
}

bool MainFrame::StartOrchestrator() {
    if (orchestrator_) {
        return true;
    }

    port_index_ = ResolveFabricPortIndex();
    if (port_index_ < 0) {
        booth_log(0, "gui_start", "no fabric device selected");
        return false;
    }

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

    orchestrator_->start(/*run_wiring_probe=*/false);
    booth_log(port_index_, "gui_start", config_path_);
    return true;
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
    root_panel->SetBackgroundColour(kHeader);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxPanel(root_panel, wxID_ANY);
    header->SetBackgroundColour(kHeader);
    auto* header_sizer = new wxBoxSizer(wxVERTICAL);
    auto* title_row = new wxBoxSizer(wxHORIZONTAL);

    auto* icon_box = new RoundedPanel(header, kIconBox, kIconBox, 8);
    icon_box->SetMinSize(wxSize(36, 36));
    icon_box->SetMaxSize(wxSize(36, 36));
    auto* icon_sizer = new wxBoxSizer(wxVERTICAL);
    auto* icon_label = MakeLabel(icon_box, wxString::FromUTF8("🚀"), kText, 14);
    icon_sizer->AddStretchSpacer();
    icon_sizer->Add(icon_label, 0, wxALIGN_CENTER_HORIZONTAL);
    icon_sizer->AddStretchSpacer();
    icon_box->SetSizer(icon_sizer);

    auto* brand_block = new wxBoxSizer(wxVERTICAL);
    auto* brand_row = new wxBoxSizer(wxHORIZONTAL);

    auto* brand_label = MakeLabel(header, "RocketBox App", kText, 14);
    {
        wxFont font = brand_label->GetFont();
        font.SetWeight(wxFONTWEIGHT_BOLD);
        brand_label->SetFont(font);
    }
    brand_row->Add(brand_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    node_name_label_ = MakeLabel(header,
                                 wxString::FromUTF8(identity_.display_name.c_str()),
                                 kMuted,
                                 12);
    brand_row->Add(node_name_label_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    connection_indicator_ = MakeConnectionIndicator(header);
    brand_row->Add(connection_indicator_, 0, wxALIGN_CENTER_VERTICAL);

    brand_block->Add(brand_row, 0, wxEXPAND);
    status_message_label_ = MakeLabel(header, "Plug in your USB cable", kWarn, 11);
    brand_block->Add(status_message_label_, 0, wxTOP, 8);

    title_row->Add(icon_box, 0, wxALIGN_TOP | wxLEFT | wxTOP, 12);
    title_row->Add(brand_block, 1, wxEXPAND | wxLEFT | wxTOP, 10);

    auto* settings_btn = new IconButton(header, ID_SettingsBtn);
    settings_btn->SetToolTip("Settings");
    settings_btn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { OnSettings(); });
    title_row->Add(settings_btn, 0, wxALIGN_TOP | wxRIGHT | wxTOP, 12);

    header_sizer->Add(title_row, 0, wxEXPAND);
    RenderConnectionIndicator();

    connection_card_ = new RoundedPanel(header, kCard, kBorder, 10);
    auto* card_sizer = new wxBoxSizer(wxVERTICAL);
    connection_panel_ = new ConnectionPanel(connection_card_);
    connection_panel_->SetActionHandlers([this]() { OnConnectUsb(); },
                                         [this]() { OnDisconnectUsb(); });
    connection_panel_->SetLayoutChangedHandler([this]() { GrowToFitContent(); });
    card_sizer->Add(connection_panel_, 0, wxEXPAND | wxALL, 18);
    connection_card_->SetSizer(card_sizer);
    header_sizer->Add(connection_card_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    roster_panel_ = new RosterPanel(
        header,
        [this](const std::string& peer) { OnPeerSelected(peer); },
        [this](const std::string& peer, const std::vector<std::string>& paths) {
            OnPeerSelected(peer);
            OnSendRequested(paths, "");
        },
        [this]() { OnSettings(); });
    header_sizer->Add(roster_panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);
    header_sizer->AddSpacer(6);
    header->SetSizer(header_sizer);
    root->Add(header, 0, wxEXPAND);

    progress_panel_ = new TransferProgressPanel(root_panel);
    progress_panel_->SetResetHandler([this]() { OnResetConnection(); });
    root->AddStretchSpacer(1);
    root->Add(progress_panel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    led_pulse_timer_ = new wxTimer(this, ID_LedPulseTimer);

    root_panel->SetSizer(root);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(root_panel, 1, wxEXPAND);
    SetSizer(outer);
}

void MainFrame::UpdateWindowTitle() {
    SetTitle(wxString::FromUTF8(("RocketBox App — " + identity_.display_name).c_str()));
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
            tooltip = "Device connected";
            break;
        case LinkLed::Transferring:
            colour = led_pulse_on_ ? kOk : wxColour(0x2a, 0x9d, 0x6f);
            tooltip = "Transfer in progress";
            break;
    }
    SetConnectionIndicator(static_cast<ConnectionLedPanel*>(connection_indicator_),
                           colour,
                           tooltip);
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

    const bool identity_configured = !state.identity.display_name.empty();
    const bool peers_configured = identity_configured && !state.roster.empty();
    const StatusLine status = status_line_native(
        state.fabric_connected,
        state.fabric_devices_seen,
        port_index_,
        state.busy,
        state.waiting_for_partner,
        state.status_message,
        identity_configured,
        peers_configured);

    if (status_message_label_) {
        status_message_label_->SetLabel(wxString::FromUTF8(status.text.c_str()));
        status_message_label_->SetForegroundColour(StatusColourToWx(status.colour));
    }

    if (connection_panel_) {
        const int fabric_port = state.fabric_port_index >= 0 ? state.fabric_port_index : port_index_;
        connection_panel_->ApplyState(state.fabric_connected,
                                      fabric_port,
                                      state.fabric_devices_seen,
                                      state.fabric_device_label,
                                      state.busy,
                                      state.live_mbps,
                                      state.booth_display_mib_s,
                                      state.result_mbps,
                                      state.last_announce_ms,
                                      state.fabric_activity_seq,
                                      state.status_message,
                                      state.error_message);
    }
}

void MainFrame::ApplyOrchestratorState(const OrchestratorUiState& state) {
    const bool was_connected = fabric_connected_;
    fabric_connected_ = state.fabric_connected;
    last_fabric_devices_seen_ = state.fabric_devices_seen;

    identity_ = state.identity;
    if (state.fabric_port_index >= 0) {
        port_index_ = state.fabric_port_index;
    }

    if (modal_depth_ > 0) {
        MaybeShowIncomingDialog(state);
        return;
    }

    UpdateWindowTitle();
    UpdateConnectionStatus(state);
    roster_panel_->UpdateRoster(state.roster,
                                state.identity,
                                state.fabric_connected,
                                state.fabric_devices_seen,
                                port_index_,
                                state.busy || state.waiting_for_partner,
                                state.last_announce_ms,
                                (state.busy || state.waiting_for_partner) ? state.status_message
                                                                          : "");

    const bool has_peers = !state.roster.empty();
    progress_panel_->ApplyState(state.busy,
                                state.waiting_for_partner,
                                state.fabric_connected,
                                has_peers,
                                state.bytes_done,
                                state.bytes_total,
                                state.live_mbps,
                                state.peak_mbps,
                                state.result_mbps,
                                state.booth_display_mib_s,
                                state.transfer_label,
                                state.status_message,
                                state.notification,
                                state.error_message);
    MaybeShowIncomingDialog(state);
    if (!state.busy && !state.pending_offer && shown_offer_id_
        && state.error_message.find("Accept failed") != std::string::npos) {
        shown_offer_id_.reset();
    }
    if (!fabric_connected_ && was_connected) {
        SetMinClientSize(wxSize(content_width_, 0));
        FitToContent();
        return;
    }
    GrowToFitContent();
    CallAfter([this]() {
        if (!IsBeingDeleted()) {
            GrowToFitContent();
        }
    });
}

void MainFrame::GrowToFitContent() {
    if (!GetSizer()) {
        return;
    }
    Layout();
    wxSize fit = GetSizer()->ComputeFittingClientSize(this);
    // wx often under-counts height right after a sizer item is shown; relayout once.
    Layout();
    fit = GetSizer()->ComputeFittingClientSize(this);

    const int width = std::max(content_width_, fit.GetWidth());
    const int height = fit.GetHeight();
    SetClientSize(wxSize(width, height));

    const wxSize min = GetMinClientSize();
    SetMinClientSize(wxSize(std::max(min.GetWidth(), width), height));
}

void MainFrame::FitToContent() {
    if (!GetSizer()) {
        return;
    }
    Layout();
    const wxSize fit = GetSizer()->ComputeFittingClientSize(this);
    const int width = std::max(content_width_, fit.GetWidth());
    SetClientSize(wxSize(width, fit.GetHeight()));
}

void MainFrame::MaybeShowIncomingDialog(const OrchestratorUiState& state) {
    if (!state.pending_offer) {
        return;
    }
    const std::string& offer_id = state.pending_offer->message.session_id;
    if (shown_offer_id_ && *shown_offer_id_ == offer_id) {
        return;
    }
    shown_offer_id_ = offer_id;

    IncomingDialog dlg(this,
                       *state.pending_offer,
                       static_cast<int>(handshake_timing_from_identity(state.identity).accept_dialog_sec),
                       [this](bool accepted) {
        if (!orchestrator_) {
            return;
        }
        if (accepted) {
            orchestrator_->accept_pending_offer();
            shown_offer_id_.reset();
        } else {
            shown_offer_id_.reset();
            orchestrator_->decline_pending_offer();
        }
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
            wxMessageBox("Device is not ready yet — check the status at top right.",
                         "Device offline",
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
    auto* files_btn = new wxButton(&dlg, ID_ChooseFiles, "Choose files...");
    auto* folder_btn = new wxButton(&dlg, ID_ChooseFolder, "Choose folder...");
    auto* cancel_btn = new wxButton(&dlg, wxID_CANCEL, "Cancel");
    for (wxButton* btn : {files_btn, folder_btn, cancel_btn}) {
        btn->SetBackgroundColour(kCard);
        btn->SetForegroundColour(kText);
        const wxSize best = btn->GetBestSize();
        btn->SetMinSize(wxSize(std::max(best.GetWidth() + 12, 88), std::max(best.GetHeight(), 28)));
    }

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
    dev.on_diagnostics = [this]() { OnUsbDiagnostics(); };
    dev.on_event_log = [this](wxWindow* parent) { OnEventLog(parent); };

    ++modal_depth_;
    SettingsDialog dlg(this, identity_, [this](const IdentityProfile& profile) {
        identity_ = profile;
        config_path_ = profile.config_path;
        UpdateWindowTitle();
        if (orchestrator_) {
            orchestrator_->set_identity(profile);
            const OrchestratorUiState snap = orchestrator_->snapshot();
            roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers),
                                        identity_,
                                        snap.fabric_connected,
                                        snap.fabric_devices_seen,
                                        port_index_,
                                        snap.busy,
                                        snap.last_announce_ms);
        }
    }, dev);
    dlg.ShowModal();
    --modal_depth_;
    if (orchestrator_) {
        ApplyOrchestratorState(orchestrator_->snapshot());
    }
}

void MainFrame::OnUsbDiagnostics() {
    std::ostringstream text;
    text << "Device port index: "
         << (port_index_ >= 0 ? std::to_string(port_index_) : "(not connected)") << "\n"
         << "Config: " << (config_path_.empty() ? "(none)" : config_path_) << "\n"
         << "usbfs limit: " << usbfs_limit_mb() << " MB (kernel)\n"
         << "In-flight depth: " << inflight_queue_depth() << " chunks\n"
         << "Payload timeout: " << payload_timeout_ms() << " ms\n"
         << "Handshake: accept_gap="
         << handshake_timing_from_identity(identity_).accept_ready_gap_ms
         << "ms reply_delay="
         << handshake_timing_from_identity(identity_).accept_reply_delay_ms
         << "ms accept_timeout="
         << handshake_timing_from_identity(identity_).accept_timeout_sec
         << "s ready_timeout="
         << handshake_timing_from_identity(identity_).ready_timeout_sec
         << "s\n"
         << "Event log: " << booth_log_path() << "\n"
         << "(View → Event Log or Settings → Event log…)";
    wxMessageBox(wxString::FromUTF8(text.str().c_str()),
                 "USB diagnostics",
                 wxOK | wxICON_INFORMATION,
                 this);
}

void MainFrame::OnResetConnection() {
    if (!orchestrator_) {
        return;
    }
    std::thread([this]() {
        orchestrator_->reset_connection();
    }).detach();
}

void MainFrame::OnConnectUsb() {
    if (orchestrator_) {
        return;
    }
    if (!StartOrchestrator()) {
        const int devices = CountFabricDevices();
        last_fabric_devices_seen_ = devices;
        status_message_label_->SetLabel(
            devices > 0 ? "USB cable detected — choose it when prompted."
                        : "Plug in your USB cable");
        status_message_label_->SetForegroundColour(kWarn);
        RenderConnectionIndicator();
        if (connection_panel_) {
            connection_panel_->ApplyState(false,
                                          -1,
                                          devices,
                                          {},
                                          false,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0,
                                          0,
                                          {},
                                          {});
        }
        GrowToFitContent();
        return;
    }
}

void MainFrame::OnDisconnectUsb() {
    if (!orchestrator_) {
        return;
    }
    if (orchestrator_->is_busy()) {
        wxMessageBox("Finish the current transfer before disconnecting.",
                     "RocketBox",
                     wxOK | wxICON_INFORMATION,
                     this);
        return;
    }
    orchestrator_->stop();
    orchestrator_.reset();
    fabric_connected_ = false;
    link_led_ = LinkLed::Offline;
    status_message_label_->SetLabel("Disconnected");
    status_message_label_->SetForegroundColour(kWarn);
    RenderConnectionIndicator();
    roster_panel_->UpdateRoster(peer_entries_from_config(identity_.peers),
                                identity_,
                                false,
                                last_fabric_devices_seen_,
                                -1,
                                false,
                                0);
    if (connection_panel_) {
        connection_panel_->ApplyState(false,
                                      -1,
                                      last_fabric_devices_seen_,
                                      {},
                                      false,
                                      0.0,
                                      0.0,
                                      0.0,
                                      0,
                                      0,
                                      {},
                                      {});
    }
    progress_panel_->ApplyState(false,
                                false,
                                false,
                                false,
                                0,
                                0,
                                0.0,
                                0.0,
                                0.0,
                                0.0,
                                {},
                                {},
                                {},
                                {});
    GrowToFitContent();
}

void MainFrame::OnEventLogMenu(wxCommandEvent&) {
    OnEventLog(this);
}

void MainFrame::OnEventLog(wxWindow* parent) {
    wxWindow* owner = parent ? parent : this;
    EventLogDialog dlg(owner, std::max(port_index_, 0));
    dlg.ShowModal();
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
