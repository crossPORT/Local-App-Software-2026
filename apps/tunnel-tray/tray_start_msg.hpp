#pragma once

#include <string>

namespace tunnel_tray {

struct TunnelConfig;

std::string start_failed_msg(const TunnelConfig& cfg);

}  // namespace tunnel_tray
