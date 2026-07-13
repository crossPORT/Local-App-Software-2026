#include "tray_icon.hpp"
#include "tray_icons.hpp"
#include "tray_theme.hpp"
#include "tunnel_proc.hpp"

#include <wx/wx.h>

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

void TunnelTrayIcon::reload_icons() {
  icon_normal_ = tunnel_tray::load_brand_icon();
  icon_dim_ = tunnel_tray::make_dim_icon(icon_normal_);
  last_tip_.clear();
}

void TunnelTrayIcon::reassert_icon() {
  RemoveIcon();
  last_tip_.clear();
  refresh_icon();
}

void TunnelTrayIcon::on_tick(wxTimerEvent&) {
  const bool dark = tunnel_tray::desktop_prefers_dark();
  if (dark != dark_theme_) {
    dark_theme_ = dark;
    reload_icons();
  }
  const bool up = proc_.running();
  if (up && (port_ < 1 || port_ > 4)) {
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
  if (up != last_up_) {
    if (!up) persist_settings(false);
    if (panel_ && panel_->is_shown()) panel_->sync_from_host(controls_now(), up);
  }
  traffic_ = false;
  if (up) {
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
  if (!wxTaskBarIcon::IsAvailable()) return;
  const bool up = proc_.running();
  const wxIcon& icon = !up ? icon_dim_ : (traffic_ && !pulse_hi_ ? icon_dim_ : icon_normal_);
  if (!icon.IsOk()) return;
  const wxString tip = tooltip_for(port_, up);
  if (tip == last_tip_ && up == last_up_ && traffic_ == last_traffic_ && pulse_hi_ == last_pulse_) {
    return;
  }
  last_tip_ = tip;
  last_up_ = up;
  last_traffic_ = traffic_;
  last_pulse_ = pulse_hi_;
  SetIcon(icon, tip);
}
