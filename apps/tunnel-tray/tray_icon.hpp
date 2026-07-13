#pragma once

#include "listen_ports.hpp"
#include "tunnel_proc.hpp"

#include <wx/taskbar.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <vector>

class TunnelTrayIcon : public wxTaskBarIcon {
public:
  TunnelTrayIcon();
  void show_panel();

protected:
  wxMenu* CreatePopupMenu() override { return nullptr; }

private:
  void on_left_down(wxTaskBarIconEvent&);
  void on_tick(wxTimerEvent&);
  void on_pulse(wxTimerEvent&);
  void refresh_icon();
  void reload_icons();
  void persist_settings(bool enabled);
  bool set_enabled(bool want_on);
  void apply_expose();
  tunnel_tray::TunnelConfig config_from_ui() const;

  tunnel_tray::TunnelProcess proc_;
  wxTimer tick_;
  wxTimer pulse_;
  wxIcon icon_normal_;
  wxIcon icon_dim_;
  bool pulse_hi_ = true;
  bool traffic_ = false;
  bool dark_theme_ = false;
  bool panel_open_ = false;
  int port_ = 1;
  bool usb_ = true;
  std::vector<tunnel_tray::Endpoint> expose_;
};
