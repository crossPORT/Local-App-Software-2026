#include "tunnel_proc.hpp"
#include "platform/stats_paths.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace tunnel_tray {

TunnelRates read_tunnel_rates(int port) {
  TunnelRates r;
  std::ifstream in(tunnel_stats_path(port));
  if (!in) return r;
  std::string line;
  while (std::getline(in, line)) {
    if (line.compare(0, 7, "up_bps=") == 0) {
      r.up_bps = std::stoull(line.substr(7));
    } else if (line.compare(0, 9, "down_bps=") == 0) {
      r.down_bps = std::stoull(line.substr(9));
    } else if (line.compare(0, 13, "display_port=") == 0) {
      r.display_port = std::stoi(line.substr(13));
    } else if (line.compare(0, 7, "serial=") == 0) {
      r.serial = line.substr(7);
    }
  }
  r.ok = true;
  return r;
}

std::string format_rate(uint64_t bps) {
  if (bps >= 1000 * 1000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB/s", bps / (1000.0 * 1000.0));
    return buf;
  }
  if (bps >= 1000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f KB/s", bps / 1000.0);
    return buf;
  }
  return std::to_string(bps) + " B/s";
}

}  // namespace tunnel_tray
