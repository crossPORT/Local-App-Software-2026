#include "tunnel_proc.hpp"
#include "helper_launch.hpp"
#include "tunnel_hup.hpp"

#include <string>

namespace tunnel_tray {

bool TunnelProcess::reload(const TunnelConfig& cfg, std::string& error) {
  int port = cfg.port;
  if (!live_tunnel_holds_port(port)) {
    const int live = live_tunnel_port();
    if (live > 0) port = live;
  }
  if (port < 1 || port > 4) {
    error = "no Port to reload expose";
    return false;
  }
  write_expose_from_endpoints(port, cfg.expose);
  if (!ensure_helper_elevated(error)) return false;

  long pid = pid_;
  if (pid <= 0) {
    const auto rates = read_tunnel_rates(port);
    pid = rates.pid;
  }
  if (!signal_tunnel_hup(pid, error)) return false;
  if (!helper_managed_) {
    pid_ = pid;
    port_ = port;
  } else {
    port_ = port;
  }
  return true;
}

}  // namespace tunnel_tray
