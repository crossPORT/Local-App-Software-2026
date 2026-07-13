#pragma once

#include <wx/string.h>

namespace tunnel_tray {

inline wxString tray_status_label(bool on, int port) {
  if (!on) return wxT("Status: Off");
  if (port < 1 || port > 4) return wxT("Status: Starting...");
  return wxString::Format(wxT("Status: Connected - Port %d"), port);
}

}  // namespace tunnel_tray
