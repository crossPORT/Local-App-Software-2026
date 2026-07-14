#include "tunnel_hup.hpp"

#include "expose_file.hpp"
#include "expose_spec.hpp"
#include "helper/protocol.hpp"
#include "helper_launch.hpp"

#include <cerrno>
#include <cstring>
#include <map>
#include <unistd.h>

namespace tunnel_tray {
namespace {

std::vector<ExposeRule> endpoints_to_rules(const std::vector<Endpoint>& expose) {
  std::map<int, ExposeRule> by_port;
  for (const Endpoint& e : expose) {
    if (e.port <= 0 || e.port > 65535) continue;
    auto it = by_port.find(e.port);
    if (it == by_port.end()) {
      ExposeRule r;
      r.port = e.port;
      r.tcp = e.proto == Proto::Tcp;
      r.udp = e.proto == Proto::Udp;
      by_port.emplace(e.port, r);
    } else if (e.proto == Proto::Tcp) {
      it->second.tcp = true;
    } else {
      it->second.udp = true;
    }
  }
  std::vector<ExposeRule> out;
  out.reserve(by_port.size());
  for (auto& kv : by_port) out.push_back(kv.second);
  return out;
}

bool helper_hup(const std::string& cmd) {
  std::string reply, herr;
  return tunnel_helper::send_command(cmd, reply, herr) && reply.rfind("OK", 0) == 0;
}

}  // namespace

void write_expose_from_endpoints(int display_port, const std::vector<Endpoint>& expose) {
  write_expose_file(display_port, endpoints_to_rules(expose));
}

bool signal_tunnel_hup(long pid, std::string& error) {
  if (helper_hup("HUP")) return true;
  if (pid > 0 && helper_hup("HUP " + std::to_string(pid))) return true;

  std::string elev_err;
  if (ensure_helper_elevated(elev_err)) {
    if (helper_hup("HUP")) return true;
    if (pid > 0 && helper_hup("HUP " + std::to_string(pid))) return true;
  }

  if (pid <= 0) {
    error = elev_err.empty() ? "no tunnel pid to reload" : elev_err;
    return false;
  }
#if defined(SIGHUP)
  if (::kill(static_cast<pid_t>(pid), SIGHUP) == 0) return true;
  if (errno != EPERM) {
    error = std::string("kill -HUP failed: ") + std::strerror(errno);
    return false;
  }
#endif
  error = elev_err.empty() ? "could not SIGHUP tunnel (start helper once)" : elev_err;
  return false;
}

}  // namespace tunnel_tray
