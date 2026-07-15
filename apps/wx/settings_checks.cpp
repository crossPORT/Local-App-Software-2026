#include "settings_checks.h"

#include "display_rate.h"
#include "settings_ui_style.h"

#include <sstream>

using settings_ui::kBg;
using settings_ui::kMuted;
using settings_ui::kText;
using settings_ui::MakeLabel;

SettingsRateChecks AddSettingsRateChecks(wxWindow* panel, wxSizer* root,
                                         const IdentityProfile& profile, int wrap_width) {
  SettingsRateChecks out;

  out.display_rate = new wxCheckBox(panel, wxID_ANY, "Display rate");
  out.display_rate->SetValue(profile.display_rate_mib_s > 0.0);
  out.display_rate->SetForegroundColour(kText);
  out.display_rate->SetBackgroundColour(kBg);
  root->Add(out.display_rate, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  std::ostringstream rate_msg;
  rate_msg.setf(std::ios::fixed);
  rate_msg.precision(0);
  rate_msg << "When enabled, transfer speeds use ~" << (kDisplayRatePresetMibS / 1024.0)
           << " GiB/s (+/- " << kDisplayRatePresetJitterPct
           << "%) during active transfers.";
  auto* rate_hint = MakeLabel(panel, wxString::FromUTF8(rate_msg.str().c_str()), kMuted);
  rate_hint->Wrap(wrap_width);
  root->Add(rate_hint, 0, wxLEFT | wxRIGHT, 10);

  out.ep4_switch = new wxCheckBox(panel, wxID_ANY, "Experimental");
  out.ep4_switch->SetValue(profile.ep4_dynamic_switch > 0);
  out.ep4_switch->SetForegroundColour(kText);
  out.ep4_switch->SetBackgroundColour(kBg);
  root->Add(out.ep4_switch, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  auto* ep4_hint = MakeLabel(
      panel,
      "Off by default. Enables experimental EP4 routing when the hardware supports it.",
      kMuted);
  ep4_hint->Wrap(wrap_width);
  root->Add(ep4_hint, 0, wxLEFT | wxRIGHT, 10);

  return out;
}

void ApplySettingsRateChecks(const SettingsRateChecks& checks, IdentityProfile& profile) {
  if (checks.display_rate && checks.display_rate->GetValue()) {
    profile.display_rate_mib_s = kDisplayRatePresetMibS;
    profile.display_rate_jitter_pct = kDisplayRatePresetJitterPct;
  } else {
    profile.display_rate_mib_s = 0.0;
    profile.display_rate_jitter_pct = 0.0;
  }
  profile.ep4_dynamic_switch =
      (checks.ep4_switch && checks.ep4_switch->GetValue()) ? 1 : 0;
}
