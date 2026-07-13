#pragma once

#include <wx/string.h>

namespace tunnel_tray {

/** User-facing reason Enable is blocked when the cable is missing or busy. */
wxString usb_enable_blocked_message(int preferred_port);

}  // namespace tunnel_tray
