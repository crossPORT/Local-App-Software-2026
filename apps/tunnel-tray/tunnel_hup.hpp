#pragma once

#include "listen_ports.hpp"

#include <string>
#include <vector>

namespace tunnel_tray {

/** Write /tmp/rocketbox/tunnel-N.expose from tray endpoints. */
void write_expose_from_endpoints(int display_port, const std::vector<Endpoint>& expose);

/** SIGHUP tunnel pid (pkexec if needed). */
bool signal_tunnel_hup(long pid, std::string& error);

}  // namespace tunnel_tray
