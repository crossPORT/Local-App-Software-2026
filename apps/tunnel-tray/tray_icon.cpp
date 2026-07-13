#include "tray_icon.hpp"
#include "tray_icons.hpp"
#include "tray_panel.hpp"
#include "tray_settings.hpp"
#include "tray_theme.hpp"
#include "tunnel_log.hpp"
#include "usb_ports_ui.hpp"
#include <wx/msgdlg.h>
#include <wx/utils.h>

namespace {
constexpr uint64_t kPulseThresholdBps = 1000;
wxString tooltip_for(int port, bool up) {
  wxString tip = wxString::Format("RocketBox Tunnel - Port %d", port);
  if (!up) return tip + " - stopped";
  const auto rates = tunnel_tray::read_tunnel_rates(port);
  if (!rates.ok || rates.up_bps + rates.down_bps == 0) return tip + " - idle";
  return tip + wxString::Format(" - up %s down %s",
                                tunnel_tray::format_rate(rates.up_bps).c_str(),
                                tunnel_tray::format_rate(rates.down_bps).c_str());
}
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
  tick_.Start(1000);
  if (settings.enabled) set_enabled(true);
  refresh_icon();
}

void TunnelTrayIcon::on_left_down(wxTaskBarIconEvent&) { show_panel(); }

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
        else port_ = 0;  // tunnel auto-detects single cable
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
  if (panel_open_) return;
  panel_open_ = true;
  tunnel_tray::TrayControls ctrls;
  ctrls.port = port_;
  ctrls.usb = usb_;
  ctrls.expose = expose_;
  const bool quit = tunnel_tray::show_tray_panel(
      nullptr, ctrls, proc_.running(),
      [this, &ctrls](bool enable) {
        port_ = ctrls.port;
        usb_ = ctrls.usb;
        expose_ = ctrls.expose;
        return set_enabled(enable);
      },
      [this, &ctrls]() {
        expose_ = ctrls.expose;
        apply_expose();
      });
  port_ = ctrls.port;
  usb_ = ctrls.usb;
  expose_ = ctrls.expose;
  persist_settings(proc_.running());
  panel_open_ = false;
  if (quit) {
    proc_.stop();
    persist_settings(false);
    wxTheApp->ExitMainLoop();
  }
  refresh_icon();
}

void TunnelTrayIcon::reload_icons() {
  icon_normal_ = tunnel_tray::load_brand_icon();
  icon_dim_ = tunnel_tray::make_dim_icon(icon_normal_);
}

void TunnelTrayIcon::on_tick(wxTimerEvent&) {
  const bool dark = tunnel_tray::desktop_prefers_dark();
  if (dark != dark_theme_) {
    dark_theme_ = dark;
    reload_icons();
  }
  traffic_ = false;
  if (proc_.running()) {
    const auto rates = tunnel_tray::read_tunnel_rates(port_);
    if (rates.ok && rates.up_bps + rates.down_bps >= kPulseThresholdBps) traffic_ = true;
  }
  if (traffic_ && !pulse_.IsRunning()) {
    pulse_hi_ = true;
    pulse_.Start(220);
  } else if (!traffic_ && pulse_.IsRunning()) {
    pulse_.Stop();
  }
  refresh_icon();
}

void TunnelTrayIcon::on_pulse(wxTimerEvent&) {
  pulse_hi_ = !pulse_hi_;
  refresh_icon();
}

void TunnelTrayIcon::refresh_icon() {
  const bool up = proc_.running();
  const wxIcon& icon = !up ? icon_dim_ : (traffic_ && !pulse_hi_ ? icon_dim_ : icon_normal_);
  SetIcon(icon, tooltip_for(port_, up));
}
