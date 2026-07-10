#pragma once

#include "tunnel_proc.hpp"

#include <wx/taskbar.h>
#include <wx/timer.h>
#include <wx/wx.h>

class TunnelTrayIcon : public wxTaskBarIcon {
public:
  TunnelTrayIcon();

protected:
  wxMenu* CreatePopupMenu() override;

private:
  void on_start(wxCommandEvent&);
  void on_stop(wxCommandEvent&);
  void on_quit(wxCommandEvent&);
  void on_port(wxCommandEvent&);
  void on_transport(wxCommandEvent&);
  void on_tick(wxTimerEvent&);
  void refresh_icon();
  tunnel_tray::TunnelConfig config_from_ui() const;

  tunnel_tray::TunnelProcess proc_;
  wxTimer timer_;
  int port_ = 1;
  bool usb_ = true;
};

enum {
  ID_START = 10001,
  ID_STOP,
  ID_QUIT,
  ID_PORT_1,
  ID_PORT_2,
  ID_PORT_3,
  ID_PORT_4,
  ID_TRANS_USB,
  ID_TRANS_SIM,
};
