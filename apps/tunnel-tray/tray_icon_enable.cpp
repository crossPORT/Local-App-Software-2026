#include "tray_icon.hpp"
#include "tray_gnome.hpp"
#include "tray_settings.hpp"
#include "tunnel_log.hpp"
#include "usb_ports_ui.hpp"

#include <wx/msgdlg.h>

tunnel_tray::TrayControls TunnelTrayIcon::controls_now() const {
  tunnel_tray::TrayControls c;
  c.port = port_;
  c.usb = usb_;
  c.ep4_dynamic_switch = ep4_dynamic_switch_;
  c.high_priority = high_priority_;
  c.expose = expose_;
  return c;
}

tunnel_tray::TunnelConfig TunnelTrayIcon::config_from_ui() const {
  tunnel_tray::TunnelConfig cfg;
  cfg.port = port_;
  cfg.transport = usb_ ? "usb" : "sim";
  cfg.expose = expose_;
  cfg.ep4_dynamic_switch = ep4_dynamic_switch_;
  cfg.high_priority = high_priority_;
  return cfg;
}

void TunnelTrayIcon::persist_settings(bool enabled) {
  tunnel_tray::TraySettings s;
  s.expose = expose_;
  s.port = port_;
  s.usb = usb_;
  s.enabled = enabled;
  s.ep4_dynamic_switch = ep4_dynamic_switch_;
  s.high_priority = high_priority_;
  tunnel_tray::save_tray_settings(s);
}

bool TunnelTrayIcon::set_enabled(bool want_on) {
  if (want_on) {
    if (usb_) {
      if (port_ < 1 || port_ > 4 ||
          (!tunnel_tray::display_port_available(port_) &&
           !tunnel_tray::live_tunnel_holds_port(port_))) {
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
    stop_in_progress_ = true;
    proc_.stop();
    stop_in_progress_ = false;
  }
  persist_settings(want_on);
  refresh_icon();
  if (panel_ && panel_->is_shown()) {
    panel_->sync_from_host(controls_now(), proc_.running());
#if defined(ROCKETBOX_TRAY_HAS_GTK)
    tunnel_tray::suppress_window_attention(panel_->GetHandle());
    // Polkit/pkexec return can set DEMANDS_ATTENTION a beat later — clear again.
    auto* p = panel_.get();
    panel_->CallAfter([p] {
      if (p) tunnel_tray::suppress_window_attention(p->GetHandle());
    });
#endif
  }
  return true;
}

bool TunnelTrayIcon::apply_expose() {
  const bool live =
      proc_.running() || (port_ >= 1 && port_ <= 4 && tunnel_tray::live_tunnel_holds_port(port_));
  persist_settings(live);
  if (!live) return true;
  std::string err;
  if (!proc_.reload(config_from_ui(), err)) {
    wxMessageBox(err, "RocketBox Tunnel", wxOK | wxICON_ERROR);
    return false;
  }
  rocketbox_tunnel_log("[tray] expose reloaded");
  refresh_icon();
  if (panel_ && panel_->is_shown()) {
    panel_->sync_from_host(controls_now(), proc_.running());
#if defined(ROCKETBOX_TRAY_HAS_GTK)
    tunnel_tray::suppress_window_attention(panel_->GetHandle());
#endif
  }
  return true;
}
