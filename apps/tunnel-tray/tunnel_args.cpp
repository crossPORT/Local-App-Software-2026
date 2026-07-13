#include "tunnel_args.hpp"

#include <sstream>

namespace tunnel_tray {

std::string build_tunnel_arg_tail(const TunnelConfig& cfg, const std::string& bin) {
  std::ostringstream oss;
  oss << bin;
  // USB: Port comes from cable serial unless disambiguating (--port).
  if (cfg.transport != "usb" || cfg.port > 0) {
    oss << " --port " << cfg.port;
  }
  oss << " --transport " << cfg.transport;
  if (!cfg.iface.empty()) oss << " --iface " << cfg.iface;
  if (!cfg.use_netns) oss << " --no-netns";
  if (!cfg.expose.empty()) {
    oss << " --expose ";
    for (size_t i = 0; i < cfg.expose.size(); ++i) {
      if (i) oss << ',';
      oss << endpoint_token(cfg.expose[i]);
    }
  }
  return oss.str();
}

}  // namespace tunnel_tray
