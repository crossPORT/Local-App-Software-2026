#pragma once

#include "listen_ports.hpp"

#include <vector>

namespace tunnel_tray {

struct TraySettings {
  std::vector<Endpoint> expose;
  int port = 1;
  bool usb = true;
  bool enabled = false;
};

TraySettings load_tray_settings();
void save_tray_settings(const TraySettings& s);

}  // namespace tunnel_tray
