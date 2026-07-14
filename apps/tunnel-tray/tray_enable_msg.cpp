#include "tray_enable_msg.hpp"

#include "tunnel_proc.hpp"
#include "usb_ports_ui.hpp"

#include <wx/string.h>

namespace tunnel_tray {

wxString usb_enable_blocked_message(int /*preferred_port*/) {
  if (count_present_usb_ports() == 0) {
    return wxT("No RocketBox USB cable detected. Plug one in, or switch to Simulation.");
  }
  return wxT("USB cable is in use (RocketBox App or another process).\n"
             "Close the App on that cable, then Enable again.");
}

}  // namespace tunnel_tray
