#include "tray_panel.hpp"
#include "tray_panel_status.hpp"
#include "tunnel_log_ui.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace tunnel_tray {

void TrayPanel::build_ui() {
  auto* root = new wxBoxSizer(wxVERTICAL);
  status_ = new wxStaticText(this, wxID_ANY, tray_status_label(false, 1));
  root->Add(status_, 0, wxALL, 12);

  enable_ = new wxCheckBox(this, wxID_ANY, wxT("Enable tunnel"));
  root->Add(enable_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

  cable_lbl_ = new wxStaticText(this, wxID_ANY, wxT("Cable"));
  root->Add(cable_lbl_, 0, wxLEFT | wxRIGHT, 12);
  port_ = new wxChoice(this, wxID_ANY);
  root->Add(port_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  auto* transport_lbl = new wxStaticText(this, wxID_ANY, wxT("Transport"));
  root->Add(transport_lbl, 0, wxLEFT | wxRIGHT, 12);
  usb_ = new wxRadioButton(this, wxID_ANY, wxT("USB hardware"), wxDefaultPosition, wxDefaultSize,
                           wxRB_GROUP);
  sim_ = new wxRadioButton(this, wxID_ANY, wxT("Simulation"));
  root->Add(usb_, 0, wxLEFT | wxRIGHT, 12);
  root->Add(sim_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

  ep4_switch_ = new wxCheckBox(this, wxID_ANY, wxT("Experimental"));
  root->Add(ep4_switch_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

  auto* expose_lbl = new wxStaticText(this, wxID_ANY, wxT("Expose services"));
  root->Add(expose_lbl, 0, wxLEFT | wxRIGHT, 12);
  list_ = new wxCheckListBox(this, wxID_ANY);
  root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);
  auto* fw_note = new wxStaticText(
      this, wxID_ANY,
      wxT("OS firewall may need rules so ICMP and exposed services pass through the tunnel."));
  fw_note->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  root->Add(fw_note, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  auto* bulk = new wxBoxSizer(wxHORIZONTAL);
  auto* all = new wxButton(this, wxID_ANY, wxT("Select all"));
  auto* none = new wxButton(this, wxID_ANY, wxT("Clear all"));
  apply_ = new wxButton(this, wxID_ANY, wxT("Apply"));
  apply_->Enable(false);
  auto* open_log = new wxButton(this, wxID_ANY, wxT("Open log"));
  bulk->Add(all, 0, wxRIGHT, 8);
  bulk->Add(none, 0, wxRIGHT, 8);
  bulk->Add(apply_, 0, wxRIGHT, 8);
  bulk->Add(open_log, 0);
  root->Add(bulk, 0, wxEXPAND | wxALL, 12);

  auto* btns = new wxBoxSizer(wxHORIZONTAL);
  auto* close = new wxButton(this, wxID_CLOSE, wxT("Close"));
  auto* quit = new wxButton(this, wxID_EXIT, wxT("Quit tray"));
  btns->AddStretchSpacer(1);
  btns->Add(close, 0, wxRIGHT, 8);
  btns->Add(quit, 0);
  root->Add(btns, 0, wxEXPAND | wxALL, 12);
  SetSizer(root);
  SetMinSize(wxSize(560, 620));

  // MSW frames default to BTNFACE grey; use window colour so the form isn't washed out.
  const wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  SetBackgroundColour(bg);
  for (wxWindow* w : {static_cast<wxWindow*>(status_), static_cast<wxWindow*>(cable_lbl_),
                       static_cast<wxWindow*>(transport_lbl), static_cast<wxWindow*>(expose_lbl),
                       static_cast<wxWindow*>(fw_note), static_cast<wxWindow*>(enable_),
                       static_cast<wxWindow*>(usb_), static_cast<wxWindow*>(sim_),
                       static_cast<wxWindow*>(ep4_switch_)}) {
    w->SetBackgroundColour(bg);
  }

  usb_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) {
    if (running_) {
      usb_->SetValue(ctrls_.usb);
      sim_->SetValue(!ctrls_.usb);
      return;
    }
    refill_ports();
  });
  sim_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) {
    if (running_) {
      usb_->SetValue(ctrls_.usb);
      sim_->SetValue(!ctrls_.usb);
      return;
    }
    refill_ports();
  });
  enable_->Bind(wxEVT_CHECKBOX, &TrayPanel::on_enable, this);
  ep4_switch_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
    if (syncing_) return;
    ctrls_.ep4_dynamic_switch = ep4_switch_->GetValue();
  });
  list_->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent&) { update_apply_enabled(); });
  all->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    for (unsigned i = 0; i < list_->GetCount(); ++i) list_->Check(static_cast<int>(i), true);
    update_apply_enabled();
  });
  none->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    for (unsigned i = 0; i < list_->GetCount(); ++i) list_->Check(static_cast<int>(i), false);
    update_apply_enabled();
  });
  apply_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    read_expose();
    if (on_expose_ && on_expose_()) {
      applied_expose_ = ctrls_.expose;
      update_apply_enabled();
    }
    status_->SetLabel(tray_status_label(enable_->GetValue(), ctrls_.port));
  });
  open_log->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { show_tunnel_log(this); });
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { hide_panel(); });
  quit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    hide_panel();
    if (on_quit_) on_quit_();
  });
}

}  // namespace tunnel_tray
