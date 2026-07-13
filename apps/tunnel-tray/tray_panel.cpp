#include "tray_panel.hpp"

#include "expose_list_ui.hpp"
#include "tray_enable_msg.hpp"
#include "tunnel_log_ui.hpp"
#include "usb_ports_ui.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace tunnel_tray {
namespace {

wxString status_label(bool on, int port) {
  if (!on) return wxT("Status: Off");
  if (port < 1 || port > 4) return wxT("Status: Starting...");
  return wxString::Format(wxT("Status: Connected - Port %d"), port);
}

}  // namespace

TrayPanel::TrayPanel(wxWindow* parent, EnableFn set_enabled, VoidFn on_expose, VoidFn on_quit)
    : wxFrame(parent, wxID_ANY, wxT("RocketBox Tunnel"), wxDefaultPosition, wxSize(560, 620),
              wxDEFAULT_FRAME_STYLE & ~wxMAXIMIZE_BOX),
      set_enabled_(std::move(set_enabled)),
      on_expose_(std::move(on_expose)),
      on_quit_(std::move(on_quit)) {
  build_ui();
  Bind(wxEVT_CLOSE_WINDOW, &TrayPanel::on_close, this);
  Hide();
}

void TrayPanel::build_ui() {
  auto* root = new wxBoxSizer(wxVERTICAL);
  status_ = new wxStaticText(this, wxID_ANY, status_label(false, 1));
  root->Add(status_, 0, wxALL, 12);

  enable_ = new wxCheckBox(this, wxID_ANY, wxT("Enable tunnel"));
  root->Add(enable_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

  cable_lbl_ = new wxStaticText(this, wxID_ANY, wxT("Cable"));
  root->Add(cable_lbl_, 0, wxLEFT | wxRIGHT, 12);
  port_ = new wxChoice(this, wxID_ANY);
  root->Add(port_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  root->Add(new wxStaticText(this, wxID_ANY, wxT("Transport")), 0, wxLEFT | wxRIGHT, 12);
  usb_ = new wxRadioButton(this, wxID_ANY, wxT("USB hardware"), wxDefaultPosition, wxDefaultSize,
                           wxRB_GROUP);
  sim_ = new wxRadioButton(this, wxID_ANY, wxT("Simulation"));
  root->Add(usb_, 0, wxLEFT | wxRIGHT, 12);
  root->Add(sim_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

  root->Add(new wxStaticText(this, wxID_ANY, wxT("Expose services")), 0, wxLEFT | wxRIGHT, 12);
  list_ = new wxCheckListBox(this, wxID_ANY);
  root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

  auto* bulk = new wxBoxSizer(wxHORIZONTAL);
  auto* all = new wxButton(this, wxID_ANY, wxT("Select all"));
  auto* none = new wxButton(this, wxID_ANY, wxT("Clear all"));
  auto* apply = new wxButton(this, wxID_ANY, wxT("Apply"));
  auto* open_log = new wxButton(this, wxID_ANY, wxT("Open log"));
  bulk->Add(all, 0, wxRIGHT, 8);
  bulk->Add(none, 0, wxRIGHT, 8);
  bulk->Add(apply, 0, wxRIGHT, 8);
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

  usb_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { refill_ports(); });
  sim_->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { refill_ports(); });
  enable_->Bind(wxEVT_CHECKBOX, &TrayPanel::on_enable, this);
  all->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    for (unsigned i = 0; i < list_->GetCount(); ++i) list_->Check(static_cast<int>(i), true);
  });
  none->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    for (unsigned i = 0; i < list_->GetCount(); ++i) list_->Check(static_cast<int>(i), false);
  });
  apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    read_expose();
    if (on_expose_) on_expose_();
    status_->SetLabel(status_label(enable_->GetValue(), ctrls_.port));
  });
  open_log->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { show_tunnel_log(this); });
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { hide_panel(); });
  quit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    hide_panel();
    if (on_quit_) on_quit_();
  });
}

void TrayPanel::refill_ports() {
  const bool is_usb = usb_->GetValue();
  cable_lbl_->SetLabel(is_usb ? wxT("Cable") : wxT("Port"));
  ctrls_.port = fill_port_choice(port_, ctrls_.port, !enable_->GetValue(), is_usb, &port_ids_);
  ctrls_.usb = is_usb;
}

void TrayPanel::sync_ctrls() {
  const int p = selected_display_port(port_, port_ids_);
  if (p > 0) ctrls_.port = p;
  ctrls_.usb = usb_->GetValue();
}

void TrayPanel::read_expose() {
  std::vector<Endpoint> next;
  for (unsigned i = 0; i < list_->GetCount() && i < rows_.size(); ++i) {
    if (list_->IsChecked(static_cast<int>(i))) next.push_back(rows_[i].ep);
  }
  ctrls_.expose = std::move(next);
}

void TrayPanel::sync_from_host(const TrayControls& ctrls, bool running) {
  ctrls_ = ctrls;
  running_ = running;
  enable_->SetValue(running);
  usb_->SetValue(ctrls.usb);
  sim_->SetValue(!ctrls.usb);
  usb_->Enable(!running);
  sim_->Enable(!running);
  refill_ports();
  rows_ = build_expose_rows(ctrls_.expose);
  fill_expose_list(list_, rows_, ctrls_.expose);
  status_->SetLabel(status_label(running, ctrls_.port));
}

void TrayPanel::show_raise(const TrayControls& ctrls, bool running) {
  sync_from_host(ctrls, running);
  Show(true);
  Raise();
  SetFocus();
}

void TrayPanel::hide_panel() {
  sync_ctrls();
  read_expose();
  Hide();
}

void TrayPanel::on_close(wxCloseEvent& ev) {
  if (ev.CanVeto()) {
    hide_panel();
    ev.Veto();
    return;
  }
  hide_panel();
}

void TrayPanel::on_enable(wxCommandEvent& ev) {
  sync_ctrls();
  read_expose();
  const bool want = ev.IsChecked();
  if (want && ctrls_.usb) {
    if (ctrls_.port <= 0) ctrls_.port = sole_available_display_port();
    if (ctrls_.port <= 0 || !display_port_available(ctrls_.port)) {
      enable_->SetValue(false);
      wxMessageBox(usb_enable_blocked_message(ctrls_.port), wxT("RocketBox Tunnel"),
                   wxOK | wxICON_ERROR);
      return;
    }
  }
  if (want && !ctrls_.usb && (ctrls_.port < 1 || ctrls_.port > 4)) {
    enable_->SetValue(false);
    wxMessageBox(wxT("Pick a Port for Simulation."), wxT("RocketBox Tunnel"), wxOK | wxICON_ERROR);
    return;
  }
  if (!set_enabled_ || !set_enabled_(want)) {
    enable_->SetValue(!want);
    const bool on = !want;
    refill_ports();
    usb_->Enable(!on);
    sim_->Enable(!on);
    status_->SetLabel(status_label(on, ctrls_.port));
    return;
  }
  running_ = want;
  refill_ports();
  usb_->Enable(!want);
  sim_->Enable(!want);
  status_->SetLabel(status_label(want, ctrls_.port));
}

}  // namespace tunnel_tray
