#include "tray_icon.hpp"

#include <wx/icon.h>
#include <wx/msgdlg.h>

namespace {

wxIcon make_dot_icon(unsigned char r, unsigned char g, unsigned char b) {
  wxImage img(16, 16);
  img.InitAlpha();
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const int dx = x - 7;
      const int dy = y - 7;
      const bool on = dx * dx + dy * dy <= 36;
      img.SetRGB(x, y, on ? r : 0, on ? g : 0, on ? b : 0);
      img.SetAlpha(x, y, on ? 255 : 0);
    }
  }
  wxIcon icon;
  icon.CopyFromBitmap(wxBitmap(img));
  return icon;
}

}  // namespace

TunnelTrayIcon::TunnelTrayIcon() {
  timer_.Bind(wxEVT_TIMER, &TunnelTrayIcon::on_tick, this);
  timer_.Start(1000);
  refresh_icon();
}

wxMenu* TunnelTrayIcon::CreatePopupMenu() {
  auto* menu = new wxMenu;
  const bool up = proc_.running();
  menu->Append(ID_START, "Start tunnel")->Enable(!up);
  menu->Append(ID_STOP, "Stop tunnel")->Enable(up);
  menu->AppendSeparator();

  auto* ports = new wxMenu;
  for (int p = 1; p <= 4; ++p) {
    auto* item = ports->AppendRadioItem(ID_PORT_1 + (p - 1), wxString::Format("Port %d (10.64.0.%d)", p, p));
    if (p == port_) item->Check();
    item->Enable(!up);
  }
  menu->AppendSubMenu(ports, "Fabric port");

  auto* trans = new wxMenu;
  auto* usb = trans->AppendRadioItem(ID_TRANS_USB, "USB hardware");
  auto* sim = trans->AppendRadioItem(ID_TRANS_SIM, "Simulation");
  usb->Check(usb_);
  sim->Check(!usb_);
  usb->Enable(!up);
  sim->Enable(!up);
  menu->AppendSubMenu(trans, "Transport");

  menu->AppendSeparator();
  menu->Append(ID_QUIT, "Quit");

  Bind(wxEVT_MENU, &TunnelTrayIcon::on_start, this, ID_START);
  Bind(wxEVT_MENU, &TunnelTrayIcon::on_stop, this, ID_STOP);
  Bind(wxEVT_MENU, &TunnelTrayIcon::on_quit, this, ID_QUIT);
  Bind(wxEVT_MENU, &TunnelTrayIcon::on_port, this, ID_PORT_1, ID_PORT_4);
  Bind(wxEVT_MENU, &TunnelTrayIcon::on_transport, this, ID_TRANS_USB, ID_TRANS_SIM);
  return menu;
}

tunnel_tray::TunnelConfig TunnelTrayIcon::config_from_ui() const {
  tunnel_tray::TunnelConfig cfg;
  cfg.port = port_;
  cfg.transport = usb_ ? "usb" : "sim";
  return cfg;
}

void TunnelTrayIcon::on_start(wxCommandEvent&) {
  std::string err;
  if (!proc_.start(config_from_ui(), err)) {
    wxMessageBox(err, "RocketBox Tunnel", wxOK | wxICON_ERROR);
  }
  refresh_icon();
}

void TunnelTrayIcon::on_stop(wxCommandEvent&) {
  proc_.stop();
  refresh_icon();
}

void TunnelTrayIcon::on_quit(wxCommandEvent&) {
  proc_.stop();
  wxTheApp->ExitMainLoop();
}

void TunnelTrayIcon::on_port(wxCommandEvent& ev) {
  port_ = 1 + (ev.GetId() - ID_PORT_1);
  refresh_icon();
}

void TunnelTrayIcon::on_transport(wxCommandEvent& ev) {
  usb_ = (ev.GetId() == ID_TRANS_USB);
  refresh_icon();
}

void TunnelTrayIcon::on_tick(wxTimerEvent&) { refresh_icon(); }

void TunnelTrayIcon::refresh_icon() {
  const bool up = proc_.running();
  SetIcon(make_dot_icon(up ? 40 : 120, up ? 180 : 120, up ? 80 : 120),
          wxString::Format("RocketBox Tunnel — port %d (%s) — %s", port_,
                           usb_ ? "usb" : "sim", up ? "running" : "stopped"));
}
