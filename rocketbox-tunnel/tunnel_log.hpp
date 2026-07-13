#pragma once

#include <string>

/** Default: /tmp/rocketbox/tunnel.log (survives pkexec root); override ROCKETBOX_TUNNEL_LOG. */
std::string rocketbox_tunnel_log_path();

/** stderr + append to log file. Never throws. */
void rocketbox_tunnel_log(const std::string& line);
