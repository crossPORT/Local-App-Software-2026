#include "tray_start_msg.hpp"

#include "tunnel_log.hpp"
#include "tunnel_proc.hpp"

#include <fstream>
#include <string>

namespace tunnel_tray {

std::string start_failed_msg(const TunnelConfig& cfg) {
  try {
    std::ifstream in(rocketbox_tunnel_log_path());
    std::string line, last_err;
    while (std::getline(in, line)) {
      const auto p = line.find("error: ");
      if (p != std::string::npos) last_err = line.substr(p + 7);
    }
    if (!last_err.empty()) return last_err;
  } catch (...) {
  }
  if (cfg.transport == "sim") {
    return "Simulation failed - is simulated-hardware running on :1772? (Open log)";
  }
  return "tunnel failed to start (no USB cable, auth cancelled, or see log)";
}

}  // namespace tunnel_tray
