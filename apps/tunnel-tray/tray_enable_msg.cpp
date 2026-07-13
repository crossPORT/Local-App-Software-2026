#include "tray_enable_msg.hpp"

#include "tunnel_proc.hpp"
#include "usb_ports_ui.hpp"

#include <wx/string.h>

namespace tunnel_tray {

wxString usb_enable_blocked_message(int preferred_port) {
  if (count_present_usb_ports() == 0) {
    return wxT("No RocketBox USB cable detected. Plug one in, or switch to Simulation.");
  }
  if (preferred_port >= 1 && live_tunnel_holds_port(preferred_port)) {
    return wxString::Format(
        wxT("Port %d is already running a tunnel. Quit that tray, or:\n"
            "  sudo pkill -f rocketbox-tunnel\n"
            "then Enable again."),
        preferred_port);
  }
  if (live_tunnel_holds_port(1) || live_tunnel_holds_port(2) || live_tunnel_holds_port(3) ||
      live_tunnel_holds_port(4)) {
    return wxT("A RocketBox tunnel already holds the USB cable (CLI would say busy).\n"
               "Stop it first:\n  sudo pkill -f rocketbox-tunnel");
  }
  return wxT("USB cable is in use (RocketBox App or another process).\n"
             "Close the App on that cable, then Enable again.");
}

}  // namespace tunnel_tray
