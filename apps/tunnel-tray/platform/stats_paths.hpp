#pragma once

#include <string>

namespace tunnel_tray {

/** Directory for tunnel-*.stats (Linux /run/rocketbox; else temp/App Support). */
std::string tunnel_stats_dir();

std::string tunnel_stats_path(int display_port);

}  // namespace tunnel_tray
