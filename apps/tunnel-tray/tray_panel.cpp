#include "tray_panel.hpp"

#include "expose_list_ui.hpp"
#include "tunnel_log_ui.hpp"
#include "usb_ports_ui.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace tunnel_tray {
namespace {

wxString status_label(bool on, int port) {
  if (!on) return wxT("Status: Off");
  return wxString::Format(wxT("Status: Connected · Port %d"), port);
}

}  // namespace

bool show_tray_panel(wxWindow* parent, TrayControls& ctrls, bool running,
                     const std::function<bool(bool enable)>& set_enabled,
                     const std::function<void()>& on_expose_applied) {
  wxDialog dlg(parent, wxID_ANY, wxT("RocketBox Tunnel"), wxDefaultPosition, wxSize(440, 560));
  auto* root = new wxBoxSizer(wxVERTICAL);

  auto* status = new wxStaticText(&dlg, wxID_ANY, status_label(running, ctrls.port));
  root->Add(status, 0, wxALL, 8);

  auto* enable = new wxCheckBox(&dlg, wxID_ANY, wxT("Enable tunnel"));
  enable->SetValue(running);
  root->Add(enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto* cable_lbl = new wxStaticText(&dlg, wxID_ANY, ctrls.usb ? wxT("Cable") : wxT("Port"));
  root->Add(cable_lbl, 0, wxLEFT | wxRIGHT, 8);
  auto* port = new wxChoice(&dlg, wxID_ANY);
  ctrls.port = fill_port_choice(port, ctrls.port, !running, ctrls.usb);
  root->Add(port, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  root->Add(new wxStaticText(&dlg, wxID_ANY, wxT("Transport")), 0, wxLEFT | wxRIGHT, 8);
  auto* usb = new wxRadioButton(&dlg, wxID_ANY, wxT("USB hardware"), wxDefaultPosition,
                                wxDefaultSize, wxRB_GROUP);
  auto* sim = new wxRadioButton(&dlg, wxID_ANY, wxT("Simulation"));
  usb->SetValue(ctrls.usb);
  sim->SetValue(!ctrls.usb);
  usb->Enable(!running);
  sim->Enable(!running);
  root->Add(usb, 0, wxLEFT | wxRIGHT, 8);
  root->Add(sim, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto refill_ports = [&]() {
    const bool is_usb = usb->GetValue();
    cable_lbl->SetLabel(is_usb ? wxT("Cable") : wxT("Port"));
    ctrls.port = fill_port_choice(port, ctrls.port, !enable->GetValue(), is_usb);
  };
  usb->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent&) { refill_ports(); });
  sim->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent&) { refill_ports(); });

  root->Add(new wxStaticText(&dlg, wxID_ANY, wxT("Expose services")), 0, wxLEFT | wxRIGHT, 8);
  auto* list = new wxCheckListBox(&dlg, wxID_ANY);
  auto rows = build_expose_rows(ctrls.expose);
  fill_expose_list(list, rows, ctrls.expose);
  root->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

  auto* bulk = new wxBoxSizer(wxHORIZONTAL);
  auto* all = new wxButton(&dlg, wxID_ANY, wxT("Select all"));
  auto* none = new wxButton(&dlg, wxID_ANY, wxT("Clear all"));
  auto* apply = new wxButton(&dlg, wxID_ANY, wxT("Apply"));
  auto* open_log = new wxButton(&dlg, wxID_ANY, wxT("Open log"));
  bulk->Add(all, 0, wxRIGHT, 6);
  bulk->Add(none, 0, wxRIGHT, 6);
  bulk->Add(apply, 0, wxRIGHT, 6);
  bulk->Add(open_log, 0);
  root->Add(bulk, 0, wxALL, 8);

  auto read_expose = [&]() {
    std::vector<Endpoint> next;
    for (unsigned i = 0; i < list->GetCount() && i < rows.size(); ++i) {
      if (list->IsChecked(static_cast<int>(i))) next.push_back(rows[i].ep);
    }
    ctrls.expose = std::move(next);
  };
  auto sync_ctrls = [&]() {
    const int p = selected_display_port(port);
    if (p > 0) ctrls.port = p;
    ctrls.usb = usb->GetValue();
  };

  all->Bind(wxEVT_BUTTON, [list](wxCommandEvent&) {
    for (unsigned i = 0; i < list->GetCount(); ++i) list->Check(static_cast<int>(i), true);
  });
  none->Bind(wxEVT_BUTTON, [list](wxCommandEvent&) {
    for (unsigned i = 0; i < list->GetCount(); ++i) list->Check(static_cast<int>(i), false);
  });
  apply->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    read_expose();
    on_expose_applied();
    status->SetLabel(status_label(enable->GetValue(), ctrls.port));
  });
  open_log->Bind(wxEVT_BUTTON, [&dlg](wxCommandEvent&) { show_tunnel_log(&dlg); });

  enable->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& ev) {
    sync_ctrls();
    read_expose();
    const bool want = ev.IsChecked();
    if (want && ctrls.usb) {
      if (ctrls.port <= 0) {
        ctrls.port = sole_available_display_port();
      }
      if (ctrls.port <= 0 || !display_port_available(ctrls.port)) {
        enable->SetValue(false);
        wxMessageBox(wxT("Plug in a RocketBox USB cable (and close RocketBox App on it), "
                         "or switch to Simulation."),
                     wxT("RocketBox Tunnel"), wxOK | wxICON_ERROR);
        return;
      }
    }
    if (want && !ctrls.usb && (ctrls.port < 1 || ctrls.port > 4)) {
      enable->SetValue(false);
      wxMessageBox(wxT("Pick a Port for Simulation."), wxT("RocketBox Tunnel"),
                   wxOK | wxICON_ERROR);
      return;
    }
    if (!set_enabled(want)) {
      enable->SetValue(!want);
      const bool on = !want;
      refill_ports();
      usb->Enable(!on);
      sim->Enable(!on);
      status->SetLabel(status_label(on, ctrls.port));
      return;
    }
    refill_ports();
    usb->Enable(!want);
    sim->Enable(!want);
    status->SetLabel(status_label(want, ctrls.port));
  });

  auto* btns = new wxBoxSizer(wxHORIZONTAL);
  auto* close = new wxButton(&dlg, wxID_OK, wxT("Close"));
  auto* quit = new wxButton(&dlg, wxID_ANY, wxT("Quit"));
  btns->AddStretchSpacer(1);
  btns->Add(close, 0, wxRIGHT, 6);
  btns->Add(quit, 0);
  root->Add(btns, 0, wxEXPAND | wxALL, 8);
  dlg.SetSizer(root);

  bool quit_app = false;
  quit->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    quit_app = true;
    dlg.EndModal(wxID_CANCEL);
  });
  close->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    sync_ctrls();
    read_expose();
    dlg.EndModal(wxID_OK);
  });

  dlg.ShowModal();
  return quit_app;
}

}  // namespace tunnel_tray
