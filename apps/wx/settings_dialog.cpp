#include "settings_dialog.h"

#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>

enum { ID_SaveSettings = wxID_HIGHEST + 300, ID_PickFolder, ID_LoopbackDev, ID_UsbDiagnostics, ID_EventLog };

SettingsDialog::SettingsDialog(wxWindow* parent,
                               const IdentityProfile& profile,
                               SaveCallback on_save,
                               const SettingsDevActions& dev_actions)
    : wxDialog(parent, wxID_ANY, "Settings", wxDefaultPosition, wxSize(420, 430))
    , profile_(profile)
    , on_save_(std::move(on_save))
    , dev_actions_(dev_actions) {
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto add_row = [&](const wxString& label, wxWindow* field) {
        root->Add(new wxStaticText(this, wxID_ANY, label), 0, wxLEFT | wxRIGHT | wxTOP, 10);
        root->Add(field, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    };

    name_field_ = new wxTextCtrl(this, wxID_ANY, profile.display_name);
    team_field_ = new wxTextCtrl(this, wxID_ANY, profile.team);
    receive_choice_ = new wxChoice(this, wxID_ANY);
    receive_choice_->Append("Auto-accept (open)");
    receive_choice_->Append("Ask first");
    receive_choice_->Append("Busy (treated as open in v1)");
    receive_choice_->SetSelection(profile.receive_status == ReceiveStatus::Open ? 0
                                 : profile.receive_status == ReceiveStatus::Busy ? 2
                                                                                 : 1);
    folder_field_ = new wxTextCtrl(this, wxID_ANY, profile.receive_folder);

    add_row("Display name", name_field_);
    add_row("Team", team_field_);
    add_row("Incoming files", receive_choice_);
    auto* receive_hint = new wxStaticText(
        this,
        wxID_ANY,
        "Receive is always on. This controls whether incoming transfers\n"
        "auto-save or show an accept/reject prompt first.");
    receive_hint->Wrap(380);
    root->Add(receive_hint, 0, wxLEFT | wxRIGHT, 10);
    root->Add(new wxStaticText(this, wxID_ANY, "Receive folder"), 0, wxLEFT | wxRIGHT | wxTOP, 10);

    auto* folder_row = new wxBoxSizer(wxHORIZONTAL);
    auto* browse_folder = new wxButton(this, ID_PickFolder, "Browse…");
    folder_row->Add(folder_field_, 1, wxRIGHT, 6);
    folder_row->Add(browse_folder, 0);
    root->Add(folder_row, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* tune_hint = new wxStaticText(
        this,
        wxID_ANY,
        "Advanced tuning (transfer_timeout_ms, usb_inflight_mb) lives in the\n"
        "config file — see booth-portN.conf comments or Developer → USB diagnostics.");
    tune_hint->Wrap(380);
    root->Add(tune_hint, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    root->Add(new wxStaticText(this, wxID_ANY, "Advanced"),
              0, wxLEFT | wxRIGHT | wxTOP, 14);
    auto* adv_hint = new wxStaticText(this, wxID_ANY, "Engineering tools — not needed for normal transfers.");
    adv_hint->Wrap(380);
    root->Add(adv_hint, 0, wxLEFT | wxRIGHT, 6);
    auto* adv_row = new wxBoxSizer(wxHORIZONTAL);
    auto* loopback_btn = new wxButton(this, ID_LoopbackDev, "Loopback…");
    auto* diag_btn = new wxButton(this, ID_UsbDiagnostics, "Diagnostics");
    auto* log_btn = new wxButton(this, ID_EventLog, "Event log…");
    adv_row->Add(loopback_btn, 1, wxRIGHT, 4);
    adv_row->Add(diag_btn, 1, wxRIGHT, 4);
    adv_row->Add(log_btn, 1);
    root->Add(adv_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    auto* save = new wxButton(this, ID_SaveSettings, "Save");
    root->Add(save, 0, wxALIGN_RIGHT | wxALL, 12);

    Bind(wxEVT_BUTTON, &SettingsDialog::OnSave, this, ID_SaveSettings);
    Bind(wxEVT_BUTTON, &SettingsDialog::OnPickFolder, this, ID_PickFolder);
    Bind(wxEVT_BUTTON, &SettingsDialog::OnLoopback, this, ID_LoopbackDev);
    Bind(wxEVT_BUTTON, &SettingsDialog::OnDiagnostics, this, ID_UsbDiagnostics);
    Bind(wxEVT_BUTTON, &SettingsDialog::OnEventLog, this, ID_EventLog);
    SetSizer(root);
}

void SettingsDialog::OnLoopback(wxCommandEvent&) {
    if (dev_actions_.on_loopback) {
        dev_actions_.on_loopback();
    }
}

void SettingsDialog::OnDiagnostics(wxCommandEvent&) {
    if (dev_actions_.on_diagnostics) {
        dev_actions_.on_diagnostics();
    }
}

void SettingsDialog::OnEventLog(wxCommandEvent&) {
    if (dev_actions_.on_event_log) {
        dev_actions_.on_event_log();
    }
}

void SettingsDialog::OnPickFolder(wxCommandEvent&) {
    wxDirDialog dlg(this, "Choose receive folder", folder_field_->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        folder_field_->SetValue(dlg.GetPath());
    }
}

void SettingsDialog::OnSave(wxCommandEvent&) {
    profile_.display_name = name_field_->GetValue().ToStdString();
    profile_.team = team_field_->GetValue().ToStdString();
    const int receive_sel = receive_choice_->GetSelection();
    profile_.receive_status = receive_sel == 0 ? ReceiveStatus::Open
                              : receive_sel == 2 ? ReceiveStatus::Busy
                                                 : ReceiveStatus::AskFirst;
    profile_.receive_folder = folder_field_->GetValue().ToStdString();
    if (on_save_) {
        on_save_(profile_);
    }
    EndModal(wxID_OK);
}
