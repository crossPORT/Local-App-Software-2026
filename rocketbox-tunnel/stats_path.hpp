#pragma once

#include <string>

/** Directory and file for tunnel-<port>.stats (OS-specific). */
std::string rocketbox_tunnel_stats_dir();
std::string rocketbox_tunnel_stats_path(int display_port);
