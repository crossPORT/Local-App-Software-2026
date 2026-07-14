#include "host_gateway.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string anchor_name(int port) { return "rocketbox" + std::to_string(port); }

std::string rules_path(int port) {
  return "/tmp/rocketbox-expose-" + std::to_string(port) + ".pf";
}

void write_pf_rules(int port, const std::string& iface, const std::vector<ExposeRule>& expose) {
  const std::string tip = "10.64.0." + std::to_string(port);
  std::ofstream out(rules_path(port), std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write pf rules file");
  const std::string on = iface.empty() ? "" : (" on " + iface);
  out << "pass in quick" << on << " proto icmp from 10.64.0.0/24 to " << tip << "\n";
  out << "pass out quick" << on << " proto icmp from " << tip << " to 10.64.0.0/24\n";
  for (const ExposeRule& r : expose) {
    if (r.port <= 0 || r.port > 65535) continue;
    const std::string ps = std::to_string(r.port);
    if (r.tcp) {
      out << "pass in quick" << on << " proto tcp from 10.64.0.0/24 to " << tip << " port " << ps
          << "\n";
    }
    if (r.udp) {
      out << "pass in quick" << on << " proto udp from 10.64.0.0/24 to " << tip << " port " << ps
          << "\n";
    }
  }
  out << "block in quick" << on << " proto tcp from 10.64.0.0/24 to " << tip << "\n";
  out << "block in quick" << on << " proto udp from 10.64.0.0/24 to " << tip << "\n";
}

void load_anchor(int port) {
  const std::string a = anchor_name(port);
  const std::string cmd = "pfctl -a " + a + " -f " + rules_path(port) + " 2>/dev/null";
  if (::system(cmd.c_str()) != 0) {
    throw std::runtime_error("pfctl load failed for anchor " + a + " (run elevated)");
  }
}

void flush_anchor(int port) {
  if (port < 1 || port > 4) return;
  const std::string a = anchor_name(port);
  (void)::system(("pfctl -a " + a + " -F all 2>/dev/null").c_str());
  std::remove(rules_path(port).c_str());
}

}  // namespace

HostGateway::~HostGateway() { remove(); }

void HostGateway::remove() {
  if (port_ > 0) flush_anchor(port_);
  port_ = 0;
  netns_.clear();
  host_veth_.clear();
  ns_veth_.clear();
  platform_ = nullptr;
}

void HostGateway::install(int local_port, const std::string& iface,
                          const std::vector<ExposeRule>& expose) {
  remove();
  if (local_port < 1 || local_port > 4) return;
  port_ = local_port;
  netns_ = iface;
  write_pf_rules(local_port, iface, expose);
  load_anchor(local_port);
}

void HostGateway::set_expose(const std::vector<ExposeRule>& expose) {
  if (port_ <= 0) return;
  write_pf_rules(port_, netns_, expose);
  load_anchor(port_);
}
