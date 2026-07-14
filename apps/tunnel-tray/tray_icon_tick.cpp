#include "tray_icon.hpp"
#include "tray_icons.hpp"
#include "tray_theme.hpp"
#include "tunnel_proc.hpp"

#include "rocketbox_version.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>

namespace {
constexpr uint64_t kPulseThresholdBps = 1000;

wxString tooltip_for(int port, bool up) {
  wxString tip = wxString::Format("RocketBox Tunnel %s - Port %d",
                                  ROCKETBOX_RELEASE_TAG_STR, port);
  if (!up) return tip + " - stopped";
  const auto rates = tunnel_tray::read_tunnel_rates(port);
  if (!rates.ok || rates.up_bps + rates.down_bps == 0) return tip + " - idle";
  return tip + wxString::Format(" - up %s down %s",
                                tunnel_tray::format_rate(rates.up_bps).c_str(),
                                tunnel_tray::format_rate(rates.down_bps).c_str());
}
}  // namespace

void TunnelTrayIcon::write_icon_files() {
  wxFileName dir(wxStandardPaths::Get().GetTempDir(), wxEmptyString);
  dir.AppendDir(wxT("rocketbox-tray"));
  if (!dir.DirExists()) dir.Mkdir(0755, wxPATH_MKDIR_FULL);
  const wxString normal = dir.GetPath() + wxFileName::GetPathSeparator() + wxT("normal.png");
  const wxString dim = dir.GetPath() + wxFileName::GetPathSeparator() + wxT("dim.png");
  const bool dark = tunnel_tray::desktop_prefers_dark();
  if (tunnel_tray::save_tray_png(normal, dark, false)) icon_file_normal_ = normal.ToStdString();
  if (tunnel_tray::save_tray_png(dim, dark, true)) icon_file_dim_ = dim.ToStdString();
}

void TunnelTrayIcon::reload_icons() {
  icon_normal_ = tunnel_tray::load_brand_icon();
  icon_dim_ = tunnel_tray::make_dim_icon(icon_normal_);
  last_tip_.clear();
  if (ayatana_.active()) write_icon_files();
}

void TunnelTrayIcon::reassert_icon() {
  if (!ayatana_.active()) RemoveIcon();
  last_tip_.clear();
  refresh_icon();
}

void TunnelTrayIcon::on_tick(wxTimerEvent&) {
  const bool dark = tunnel_tray::desktop_prefers_dark();
  if (dark != dark_theme_) {
    dark_theme_ = dark;
    reload_icons();
  }
  if (stop_in_progress_) {
    if (panel_ && panel_->is_shown()) panel_->sync_running(false, port_);
    refresh_icon();
    return;
  }
  const bool up = proc_.running();
  if (up) {
    const int live = tunnel_tray::live_tunnel_port();
    if (live > 0 && live != port_) port_ = live;
  }
  if (up != last_up_) {
    if (!up) persist_settings(false);
    else persist_settings(true);
  }
  // Keep Enable + Status in lockstep with live helper/stats (orphan-safe).
  if (panel_ && panel_->is_shown()) {
    if (up != last_up_) panel_->sync_from_host(controls_now(), up);
    else panel_->sync_running(up, port_);
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
  const bool up = proc_.running();
  const bool dim = !up || (traffic_ && !pulse_hi_);
  const wxString tip = tooltip_for(port_, up);
  if (tip == last_tip_ && up == last_up_ && traffic_ == last_traffic_ && pulse_hi_ == last_pulse_) {
    return;
  }
  if (up != last_up_ && ayatana_.active()) ayatana_.refresh_menu();
  last_tip_ = tip;
  last_up_ = up;
  last_traffic_ = traffic_;
  last_pulse_ = pulse_hi_;
  if (ayatana_.active()) {
    const std::string& path = dim ? icon_file_dim_ : icon_file_normal_;
    if (!path.empty()) ayatana_.set_icon(path, tip.ToStdString());
    return;
  }
  if (!wxTaskBarIcon::IsAvailable()) return;
  const wxIcon& icon = dim ? icon_dim_ : icon_normal_;
  if (!icon.IsOk()) return;
  SetIcon(icon, tip);
}
