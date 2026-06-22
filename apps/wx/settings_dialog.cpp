#include "settings_dialog.h"

#include "booth_display.h"

#include <algorithm>
#include <sstream>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>

namespace {

const wxColour kBg(0x0f, 0x14, 0x19);
const wxColour kField(0x12, 0x18, 0x22);
const wxColour kText(0xf0, 0xf4, 0xf8);
const wxColour kMuted(0x88, 0x99, 0xaa);
const wxColour kAccent(0x00, 0xd4, 0xaa);

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, const wxColour& fg = kText) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    label->SetForegroundColour(fg);
    label->SetBackgroundColour(parent->GetBackgroundColour());
    return label;
}

void StyleField(wxTextCtrl* field) {
    field->SetBackgroundColour(kField);
    field->SetForegroundColour(kText);
}

void StyleButton(wxButton* button, bool primary = false) {
    button->SetBackgroundColour(primary ? kAccent : kField);
    button->SetForegroundColour(primary ? kBg : kText);
    const wxSize best = button->GetBestSize();
    button->SetMinSize(wxSize(std::max(best.GetWidth() + 12, 72), std::max(best.GetHeight(), 28)));
}

wxString ProfileText(const std::string& value) {
    return wxString::FromUTF8(value.c_str());
}

}  // namespace

enum {
    ID_SettingsSave = wxID_HIGHEST + 500,
    ID_SettingsPickFolder,
    ID_SettingsDiagnostics,
    ID_SettingsEventLog,
};

SettingsDialog::SettingsDialog(wxWindow* parent,
                               const IdentityProfile& profile,
                               SaveCallback on_save,
                               const SettingsDevActions& dev_actions)
    : wxDialog(parent,
               wxID_ANY,
               "Settings",
               wxDefaultPosition,
               wxSize(460, 580),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , profile_(profile)
    , on_save_(std::move(on_save))
    , dev_actions_(dev_actions) {
    SetBackgroundColour(kBg);

    auto* panel = new wxPanel(this, wxID_ANY);
    panel->SetBackgroundColour(kBg);

    constexpr int kWrapWidth = 400;
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto add_row = [&](const wxString& label, wxWindow* field) {
        root->Add(MakeLabel(panel, label), 0, wxLEFT | wxRIGHT | wxTOP, 10);
        root->Add(field, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    };

    name_field_ = new wxTextCtrl(panel, wxID_ANY, ProfileText(profile.display_name));
    team_field_ = new wxTextCtrl(panel, wxID_ANY, ProfileText(profile.team));
    receive_choice_ = new wxChoice(panel, wxID_ANY);
    receive_choice_->Append("Auto-accept (open)");
    receive_choice_->Append("Ask first");
    receive_choice_->Append("Busy (treated as open in v1)");
    receive_choice_->SetSelection(profile.receive_status == ReceiveStatus::Open ? 0
                                 : profile.receive_status == ReceiveStatus::Busy ? 2
                                                                                 : 1);
    folder_field_ = new wxTextCtrl(panel, wxID_ANY, ProfileText(profile.receive_folder));

    StyleField(name_field_);
    StyleField(team_field_);
    StyleField(folder_field_);

    add_row("Display name", name_field_);
    add_row("Team", team_field_);
    add_row("Incoming files", receive_choice_);

    auto* receive_hint = MakeLabel(
        panel,
        "Receive is always on. This controls whether incoming transfers auto-save or "
        "show an accept/reject prompt first.",
        kMuted);
    receive_hint->Wrap(kWrapWidth);
    root->Add(receive_hint, 0, wxLEFT | wxRIGHT | wxTOP, 6);

    root->Add(MakeLabel(panel, "Receive folder"), 0, wxLEFT | wxRIGHT | wxTOP, 10);

    auto* folder_row = new wxBoxSizer(wxHORIZONTAL);
    auto* browse_folder = new wxButton(panel, ID_SettingsPickFolder, "Browse...");
    StyleButton(browse_folder);
    folder_row->Add(folder_field_, 1, wxRIGHT, 6);
    folder_row->Add(browse_folder, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(folder_row, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    booth_display_check_ = new wxCheckBox(panel, wxID_ANY, "Booth display speed");
    booth_display_check_->SetValue(profile.booth_display_mib_s > 0.0);
    booth_display_check_->SetForegroundColour(kText);
    booth_display_check_->SetBackgroundColour(kBg);
    root->Add(booth_display_check_, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    std::ostringstream booth_msg;
    booth_msg.setf(std::ios::fixed);
    booth_msg.precision(0);
    booth_msg << "When enabled, transfer speeds use ~" << (kBoothDisplayPresetMibS / 1024.0)
             << " GiB/s (+/- " << kBoothDisplayPresetJitterPct
             << "%) during active transfers.";
    auto* booth_hint = MakeLabel(panel, wxString::FromUTF8(booth_msg.str().c_str()), kMuted);
    booth_hint->Wrap(kWrapWidth);
    root->Add(booth_hint, 0, wxLEFT | wxRIGHT, 10);

    auto* tune_hint = MakeLabel(
        panel,
        "Advanced tuning keys can be set in a config file passed via --config, or in "
        "Developer → USB diagnostics).",
        kMuted);
    tune_hint->Wrap(kWrapWidth);
    root->Add(tune_hint, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    root->Add(MakeLabel(panel, "Advanced"), 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto* adv_row = new wxBoxSizer(wxHORIZONTAL);
    auto* diag_btn = new wxButton(panel, ID_SettingsDiagnostics, "Diagnostics");
    auto* log_btn = new wxButton(panel, ID_SettingsEventLog, "Event log...");
    StyleButton(diag_btn);
    StyleButton(log_btn);
    adv_row->Add(diag_btn, 1, wxRIGHT, 4);
    adv_row->Add(log_btn, 1);
    root->Add(adv_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

    auto* save = new wxButton(panel, ID_SettingsSave, "Save");
    StyleButton(save, true);
    root->Add(save, 0, wxALIGN_RIGHT | wxALL, 12);

    panel->SetSizer(root);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(panel, 1, wxEXPAND | wxALL, 8);
    SetSizer(outer);

    SetMinSize(wxSize(440, 480));
    CentreOnParent();

    save->Bind(wxEVT_BUTTON, &SettingsDialog::OnSave, this);
    browse_folder->Bind(wxEVT_BUTTON, &SettingsDialog::OnPickFolder, this);
    diag_btn->Bind(wxEVT_BUTTON, &SettingsDialog::OnDiagnostics, this);
    log_btn->Bind(wxEVT_BUTTON, &SettingsDialog::OnEventLog, this);
}

void SettingsDialog::OnDiagnostics(wxCommandEvent&) {
    if (dev_actions_.on_diagnostics) {
        dev_actions_.on_diagnostics();
    }
}

void SettingsDialog::OnEventLog(wxCommandEvent&) {
    if (dev_actions_.on_event_log) {
        dev_actions_.on_event_log(this);
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
    if (booth_display_check_->GetValue()) {
        profile_.booth_display_mib_s = kBoothDisplayPresetMibS;
        profile_.booth_display_jitter_pct = kBoothDisplayPresetJitterPct;
    } else {
        profile_.booth_display_mib_s = 0.0;
        profile_.booth_display_jitter_pct = 0.0;
    }
    if (on_save_) {
        on_save_(profile_);
    }
    EndModal(wxID_OK);
}
