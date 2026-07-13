#pragma once

#include "listen_ports.hpp"

#include <vector>

namespace tunnel_tray {

/** SMB / SSH / HTTP presets shown at top of expose list. */
std::vector<Endpoint> expose_presets();

}  // namespace tunnel_tray
