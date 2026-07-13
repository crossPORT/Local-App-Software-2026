#pragma once

#include "listen_ports.hpp"

#include <wx/window.h>

#include <functional>
#include <vector>

namespace tunnel_tray {

struct TrayControls {
  int port = 1;
  bool usb = true;
  std::vector<Endpoint> expose;
};

/**
 * Modal control panel for the tray icon. Edits ctrls in place.
 * set_enabled starts/stops the tunnel; returns false on failure.
 * Returns true if the user chose Quit.
 */
bool show_tray_panel(wxWindow* parent, TrayControls& ctrls, bool running,
                     const std::function<bool(bool)>& set_enabled,
                     const std::function<void()>& on_expose_applied);

}  // namespace tunnel_tray
