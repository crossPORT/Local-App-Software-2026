#pragma once

#include "identity_profile.h"

#include <functional>
#include <wx/wx.h>

struct SettingsDevActions {
    std::function<void()> on_diagnostics;
    std::function<void(wxWindow* dialog_parent)> on_event_log;
};

class SettingsDialog : public wxDialog {
public:
    using SaveCallback = std::function<void(const IdentityProfile& profile)>;

    SettingsDialog(wxWindow* parent,
                   const IdentityProfile& profile,
                   SaveCallback on_save,
                   const SettingsDevActions& dev_actions = {});

private:
    void OnSave(wxCommandEvent& event);
    void OnPickFolder(wxCommandEvent& event);
    void OnDiagnostics(wxCommandEvent& event);
    void OnEventLog(wxCommandEvent& event);

    wxTextCtrl* name_field_ = nullptr;
    wxTextCtrl* team_field_ = nullptr;
    wxChoice* receive_choice_ = nullptr;
    wxTextCtrl* folder_field_ = nullptr;
    wxCheckBox* demo_check_ = nullptr;
    IdentityProfile profile_;
    SaveCallback on_save_;
    SettingsDevActions dev_actions_;
};
