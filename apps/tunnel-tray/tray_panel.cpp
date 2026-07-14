#include "tray_panel.hpp"

#include "expose_list_ui.hpp"
#include "tray_enable_msg.hpp"
#include "tray_gnome.hpp"
#include "tray_panel_status.hpp"
#include "tunnel_proc.hpp"
#include "usb_ports_ui.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/stattext.h>

#if defined(ROCKETBOX_TRAY_HAS_GTK)
#include <gtk/gtk.h>
#endif

namespace tunnel_tray {

TrayPanel::TrayPanel(wxWindow* parent, EnableFn set_enabled, ApplyFn on_expose, VoidFn on_quit)
    : wxFrame(parent, wxID_ANY, wxT("RocketBox Tunnel"), wxDefaultPosition, wxSize(560, 620),
              wxCAPTION | wxRESIZE_BORDER | wxCLIP_CHILDREN | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR),
      set_enabled_(std::move(set_enabled)),
      on_expose_(std::move(on_expose)),
      on_quit_(std::move(on_quit)) {
  build_ui();
  Bind(wxEVT_CLOSE_WINDOW, &TrayPanel::on_close, this);
#if defined(ROCKETBOX_TRAY_HAS_GTK)
  // Utility / no taskbar; strip CSD close; never demand attention (no GNOME toasts).
  if (GtkWidget* w = static_cast<GtkWidget*>(GetHandle())) {
    if (GTK_IS_WINDOW(w)) {
      gtk_window_set_type_hint(GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_UTILITY);
      gtk_window_set_deletable(GTK_WINDOW(w), FALSE);
      suppress_window_attention(w);
    }
  }
#endif
  Hide();
}

void TrayPanel::refill_ports() {
  const bool is_usb = usb_->GetValue();
  cable_lbl_->SetLabel(is_usb ? wxT("Cable") : wxT("Port"));
  // Lock Port while tunnel is up; Enable checkbox stays clickable.
  ctrls_.port = fill_port_choice(port_, ctrls_.port, !running_, is_usb, &port_ids_);
  ctrls_.usb = is_usb;
  usb_->Enable(!running_);
  sim_->Enable(!running_);
  enable_->Enable(true);
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

void TrayPanel::update_apply_enabled() {
  if (!apply_) return;
  read_expose();
  apply_->Enable(!expose_selection_equal(ctrls_.expose, applied_expose_));
}

void TrayPanel::sync_from_host(const TrayControls& ctrls, bool running) {
  syncing_ = true;
  ctrls_ = ctrls;
  applied_expose_ = ctrls.expose;
  running_ = running;
  enable_->SetValue(running);
  usb_->SetValue(ctrls.usb);
  sim_->SetValue(!ctrls.usb);
  refill_ports();
  rows_ = build_expose_rows(ctrls_.expose);
  fill_expose_list(list_, rows_, ctrls_.expose);
  status_->SetLabel(tray_status_label(running, ctrls_.port));
  update_apply_enabled();
  syncing_ = false;
}

void TrayPanel::sync_running(bool running) {
  syncing_ = true;
  running_ = running;
  if (enable_->GetValue() != running) enable_->SetValue(running);
  status_->SetLabel(tray_status_label(running, ctrls_.port));
  refill_ports();
  syncing_ = false;
}

bool TrayPanel::enable_checked() const { return enable_ && enable_->GetValue(); }

void TrayPanel::show_raise(const TrayControls& ctrls, bool running, unsigned user_time) {
  sync_from_host(ctrls, running);
  CentreOnScreen();
#if defined(ROCKETBOX_TRAY_HAS_GTK)
  // Map only — never present/raise/focus. GNOME toasts “is ready” on focus steal.
  (void)user_time;
  Show(true);
  if (GtkWidget* w = static_cast<GtkWidget*>(GetHandle())) {
    if (GTK_IS_WINDOW(w)) {
      suppress_window_attention(w);
      gtk_widget_show(w);
    }
  }
#else
  Show(true);
  Raise();
  SetFocus();
  (void)user_time;
#endif
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
  if (syncing_) return;
  sync_ctrls();
  read_expose();
  const bool want = ev.IsChecked();
  if (want && ctrls_.usb) {
    if (ctrls_.port <= 0) ctrls_.port = sole_available_display_port();
    if (ctrls_.port <= 0 ||
        (!display_port_available(ctrls_.port) && !live_tunnel_holds_port(ctrls_.port))) {
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
