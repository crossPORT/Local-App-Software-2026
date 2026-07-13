#pragma once

#include "tunnel_proc.hpp"

#include <string>

namespace tunnel_tray {

std::string build_tunnel_arg_tail(const TunnelConfig& cfg, const std::string& bin);

}  // namespace tunnel_tray
