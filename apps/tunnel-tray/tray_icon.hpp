#pragma once

#include "listen_ports.hpp"
#include "tray_ayatana.hpp"
#include "tray_panel.hpp"
#include "tunnel_proc.hpp"

#include <wx/taskbar.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <memory>
#include <string>
#include <vector>

class TunnelTrayIcon : public wxTaskBarIcon {
public:
  TunnelTrayIcon();
  ~TunnelTrayIcon() override;
  void show_panel(unsigned user_time = 0);
  bool set_enabled(bool want_on);

protected:
  wxMenu* CreatePopupMenu() override;

private:
  wxMenu* build_menu();
  void on_left_down(wxTaskBarIconEvent&);
  void on_menu(wxCommandEvent&);
  void on_tick(wxTimerEvent&);
  void on_pulse(wxTimerEvent&);
  void refresh_icon();
  void reload_icons();
  void reassert_icon();
  void persist_settings(bool enabled);
  bool apply_expose();
  void quit_app();
  void open_log();
  void show_about();
  void start_ayatana();
  void write_icon_files();
  tunnel_tray::TunnelConfig config_from_ui() const;
  tunnel_tray::TrayControls controls_now() const;
  void ensure_panel();

  tunnel_tray::TunnelProcess proc_;
  std::unique_ptr<tunnel_tray::TrayPanel> panel_;
  tunnel_tray::AyatanaTray ayatana_;
  wxTimer tick_;
  wxTimer pulse_;
  wxIcon icon_normal_;
  wxIcon icon_dim_;
  std::string icon_file_normal_;
  std::string icon_file_dim_;
  bool pulse_hi_ = true;
  bool traffic_ = false;
  bool dark_theme_ = false;
  int port_ = 1;
  bool usb_ = true;
  bool ep4_dynamic_switch_ = false;
  std::vector<tunnel_tray::Endpoint> expose_;
  wxString last_tip_;
  bool last_up_ = false;
  bool last_traffic_ = false;
  bool last_pulse_ = true;
  bool stop_in_progress_ = false;
};
