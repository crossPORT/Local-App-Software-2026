#pragma once

#include "identity_profile.h"

#include <wx/wx.h>

struct SettingsRateChecks {
    wxCheckBox* display_rate = nullptr;
    wxCheckBox* ep4_switch = nullptr;
};

/** Display-rate + EP4 toggles (Advanced). */
SettingsRateChecks AddSettingsRateChecks(wxWindow* panel, wxSizer* root,
                                         const IdentityProfile& profile, int wrap_width);
void ApplySettingsRateChecks(const SettingsRateChecks& checks, IdentityProfile& profile);
