#include "tray_icon.hpp"
#include "tray_settings.hpp"
#include "tray_theme.hpp"
#include "tunnel_log.hpp"
#include "tunnel_log_ui.hpp"
#include "usb_ports_ui.hpp"

#include <wx/msgdlg.h>
#include <wx/utils.h>

namespace {
enum {
  ID_TRAY_ENABLE = wxID_HIGHEST + 1,
  ID_TRAY_DISABLE,
  ID_TRAY_LOG,
};
}  // namespace

TunnelTrayIcon::TunnelTrayIcon() {
  const auto settings = tunnel_tray::load_tray_settings();
  expose_ = settings.expose;
  port_ = settings.port;
  usb_ = settings.usb;
  dark_theme_ = tunnel_tray::desktop_prefers_dark();
  reload_icons();
  tick_.Bind(wxEVT_TIMER, &TunnelTrayIcon::on_tick, this);
  pulse_.Bind(wxEVT_TIMER, &TunnelTrayIcon::on_pulse, this);
  Bind(wxEVT_TASKBAR_LEFT_DOWN, &TunnelTrayIcon::on_left_down, this);
  Bind(wxEVT_MENU, &TunnelTrayIcon::on_menu, this);
  tick_.Start(1000);
  if (settings.enabled) set_enabled(true);
  refresh_icon();
}

TunnelTrayIcon::~TunnelTrayIcon() {
  if (panel_) {
    panel_->Destroy();
    panel_.release();
  }
}

void TunnelTrayIcon::ensure_panel() {
  if (panel_) return;
  panel_ = std::make_unique<tunnel_tray::TrayPanel>(
      nullptr,
      [this](bool enable) {
        if (panel_) {
          const auto c = panel_->controls();
          port_ = c.port;
          usb_ = c.usb;
          expose_ = c.expose;
        }
        return set_enabled(enable);
      },
      [this]() {
        if (panel_) {
          const auto c = panel_->controls();
          port_ = c.port;
          usb_ = c.usb;
          expose_ = c.expose;
        }
        apply_expose();
      },
      [this]() { quit_app(); });
}

wxMenu* TunnelTrayIcon::CreatePopupMenu() {
  auto* m = new wxMenu();
  m->Append(wxID_OPEN, wxT("Open control panel"));
  if (proc_.running()) m->Append(ID_TRAY_DISABLE, wxT("Disable tunnel"));
  else m->Append(ID_TRAY_ENABLE, wxT("Enable tunnel"));
  m->Append(ID_TRAY_LOG, wxT("Open log"));
  m->AppendSeparator();
  m->Append(wxID_EXIT, wxT("Quit tray"));
  return m;
}

void TunnelTrayIcon::on_left_down(wxTaskBarIconEvent&) { show_panel(); }

void TunnelTrayIcon::on_menu(wxCommandEvent& ev) {
  switch (ev.GetId()) {
    case wxID_OPEN:
      show_panel();
      break;
    case ID_TRAY_ENABLE:
      set_enabled(true);
      break;
    case ID_TRAY_DISABLE:
      set_enabled(false);
      break;
    case ID_TRAY_LOG:
      open_log();
      break;
    case wxID_EXIT:
      quit_app();
      break;
    default:
      break;
  }
}

void TunnelTrayIcon::open_log() { tunnel_tray::show_tunnel_log(nullptr); }

void TunnelTrayIcon::quit_app() {
  // Only stop the tunnel this tray started (pid / helper-managed).
  proc_.stop();
  persist_settings(false);
  if (panel_) {
    panel_->Destroy();
    panel_.release();
  }
  RemoveIcon();
  wxTheApp->ExitMainLoop();
}

tunnel_tray::TrayControls TunnelTrayIcon::controls_now() const {
  tunnel_tray::TrayControls c;
  c.port = port_;
  c.usb = usb_;
  c.expose = expose_;
  return c;
}

tunnel_tray::TunnelConfig TunnelTrayIcon::config_from_ui() const {
  tunnel_tray::TunnelConfig cfg;
  cfg.port = port_;
  cfg.transport = usb_ ? "usb" : "sim";
  cfg.expose = expose_;
  return cfg;
}

void TunnelTrayIcon::persist_settings(bool enabled) {
  tunnel_tray::TraySettings s;
  s.expose = expose_;
  s.port = port_;
  s.usb = usb_;
  s.enabled = enabled;
  tunnel_tray::save_tray_settings(s);
}

bool TunnelTrayIcon::set_enabled(bool want_on) {
  if (want_on) {
    if (usb_) {
      if (port_ < 1 || port_ > 4 || !tunnel_tray::display_port_available(port_)) {
        const int sole = tunnel_tray::sole_available_display_port();
        if (sole > 0) port_ = sole;
        else port_ = 0;
      }
    }
    std::string err;
    rocketbox_tunnel_log(std::string("[tray] enable ") + (usb_ ? "usb" : "sim") +
                         (port_ ? " Port " + std::to_string(port_) : " (auto Port)"));
    if (!proc_.start(config_from_ui(), err)) {
      rocketbox_tunnel_log("[tray] enable failed: " + err);
      wxMessageBox(err, "RocketBox Tunnel", wxOK | wxICON_ERROR);
      persist_settings(false);
      return false;
    }
    if (port_ <= 0) {
      for (int p = 1; p <= 4; ++p) {
        const auto rates = tunnel_tray::read_tunnel_rates(p);
        if (rates.ok && rates.display_port > 0) {
          port_ = rates.display_port;
          break;
        }
        if (rates.ok) {
          port_ = p;
          break;
        }
      }
    }
    rocketbox_tunnel_log("[tray] tunnel running");
  } else {
    rocketbox_tunnel_log("[tray] disable");
    proc_.stop();
  }
  persist_settings(want_on);
  refresh_icon();
  if (panel_ && panel_->is_shown()) panel_->sync_from_host(controls_now(), proc_.running());
  return true;
}

void TunnelTrayIcon::apply_expose() {
  persist_settings(proc_.running());
  if (!proc_.running()) return;
  proc_.stop();
  std::string err;
  if (!proc_.start(config_from_ui(), err)) {
    wxMessageBox(err, "RocketBox Tunnel", wxOK | wxICON_ERROR);
    persist_settings(false);
  }
  refresh_icon();
}

void TunnelTrayIcon::show_panel() {
  ensure_panel();
  panel_->show_raise(controls_now(), proc_.running());
}
