#pragma once

#include "connection_panel.h"
#include "identity_profile.h"
#include "transfer_orchestrator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/display.h>

class RosterPanel;
class TransferProgressPanel;

enum class LinkLed { Offline, Announcing, Connected, Transferring };

class MainFrame : public wxFrame {
public:
    explicit MainFrame(const std::string& config_path, int cli_port_index = -1);

private:
    void BuildUi();
    void FitToContent();
    void GrowToFitContent();
    void ApplyOrchestratorState(const OrchestratorUiState& state);
    void UpdateConnectionStatus(const OrchestratorUiState& state);
    void UpdateWindowTitle();
    void RenderConnectionIndicator();
    void SyncLedPulseTimer(bool pulse);
    void OnLedPulseTimer(wxTimerEvent& event);
    void OnShutdownPollTimer(wxTimerEvent& event);
    void OnPeerSelected(const std::string& peer_name);
    void OnSendRequested(const std::vector<std::string>& paths, const std::string& note);
    void OnBrowseFiles();
    void OnBrowseFolder();
    std::vector<std::string> BrowseFilesDialog();
    std::vector<std::string> BrowseFolderDialog();
    std::vector<std::string> PromptForSendPaths();
    void MaybeShowIncomingDialog(const OrchestratorUiState& state);
    void OnSettings();
    void OnSettingsMenu(wxCommandEvent& event);
    void OnUsbDiagnostics();
    void OnEventLog(wxWindow* parent = nullptr);
    void OnEventLogMenu(wxCommandEvent& event);
    void OnResetConnection();
    void BuildMenuBar();
    void OnFirstShow(wxShowEvent& event);
    void OnClose(wxCloseEvent& event);
    bool StartOrchestrator();
    int ResolveFabricPortIndex();

    int cli_port_index_ = -1;
    int port_index_ = -1;
    int content_width_ = 560;
    bool fabric_connected_ = false;
    std::string config_path_;
    IdentityProfile identity_;

    RosterPanel* roster_panel_ = nullptr;
    TransferProgressPanel* progress_panel_ = nullptr;
    wxStaticText* node_name_label_ = nullptr;
    wxStaticText* status_message_label_ = nullptr;
    wxPanel* connection_indicator_ = nullptr;
    wxPanel* connection_card_ = nullptr;
    ConnectionPanel* connection_panel_ = nullptr;
    wxTimer* led_pulse_timer_ = nullptr;
    wxTimer* shutdown_poll_timer_ = nullptr;
    LinkLed link_led_ = LinkLed::Offline;
    bool led_pulse_on_ = false;
    std::unique_ptr<TransferOrchestrator> orchestrator_;
    std::string selected_peer_;
    std::optional<std::string> shown_offer_id_;
    bool orchestrator_started_ = false;
    int modal_depth_ = 0;

    wxDECLARE_EVENT_TABLE();
};
