#include "tray_panel.hpp"

#include "expose_list_ui.hpp"
#include "tray_enable_msg.hpp"
#include "tray_panel_status.hpp"
#include "usb_ports_ui.hpp"

#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/stattext.h>

namespace tunnel_tray {

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
  refill_ports();
  rows_ = build_expose_rows(ctrls_.expose);
  fill_expose_list(list_, rows_, ctrls_.expose);
  status_->SetLabel(tray_status_label(running, ctrls_.port));
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
    running_ = on;
    refill_ports();
    status_->SetLabel(tray_status_label(on, ctrls_.port));
    return;
  }
  running_ = want;
  refill_ports();
  status_->SetLabel(tray_status_label(want, ctrls_.port));
}

}  // namespace tunnel_tray
