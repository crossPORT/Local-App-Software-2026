#include "tunnel_hup.hpp"

#include "expose_file.hpp"
#include "expose_spec.hpp"

#include <map>

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

}  // namespace

void write_expose_from_endpoints(int display_port, const std::vector<Endpoint>& expose) {
  write_expose_file(display_port, endpoints_to_rules(expose));
}

}  // namespace tunnel_tray
