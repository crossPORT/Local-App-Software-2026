#include "host_gateway.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kIp = "/sbin/ip";
constexpr const char* kIpt = "/usr/sbin/iptables";
constexpr const char* kSys = "/usr/sbin/sysctl";

void run_or_throw(const std::string& cmd) {
  if (::system(cmd.c_str()) != 0) {
    throw std::runtime_error("command failed: " + cmd);
  }
}

void run_ignore(const std::string& cmd) { (void)::system(cmd.c_str()); }

std::string nx(const std::string& netns, const std::string& cmd) {
  return std::string(kIp) + " netns exec " + netns + " " + cmd;
}

}  // namespace

HostGateway::~HostGateway() { remove(); }

void HostGateway::remove() {
  if (port_ <= 0) {
    return;
  }
  for (int k = 1; k <= 4; ++k) {
    if (k == port_) {
      continue;
    }
    run_ignore(std::string(kIp) + " route del 10.64.0." + std::to_string(k) +
               "/32 2>/dev/null");
  }
  if (!host_veth_.empty()) {
    run_ignore(std::string(kIp) + " link del " + host_veth_ + " 2>/dev/null");
  }
  port_ = 0;
  netns_.clear();
  host_veth_.clear();
  ns_veth_.clear();
}

void HostGateway::install(int local_port, const std::string& netns,
                          const std::vector<int>& expose_ports) {
  remove();
  port_ = local_port;
  netns_ = netns;
  host_veth_ = "rbh" + std::to_string(local_port);
  ns_veth_ = "rbg" + std::to_string(local_port);

  const std::string host_addr = "10.65." + std::to_string(local_port) + ".1";
  const std::string ns_addr = "10.65." + std::to_string(local_port) + ".2";
  const std::string fabric_ip = "10.64.0." + std::to_string(local_port);
  const std::string tun = "rb" + std::to_string(local_port);

  run_ignore(std::string(kIp) + " link del " + host_veth_ + " 2>/dev/null");
  run_or_throw(std::string(kIp) + " link add " + host_veth_ + " type veth peer name " + ns_veth_);
  run_or_throw(std::string(kIp) + " link set " + ns_veth_ + " netns " + netns_);
  run_or_throw(std::string(kIp) + " addr add " + host_addr + "/30 dev " + host_veth_);
  run_or_throw(std::string(kIp) + " link set " + host_veth_ + " up");
  run_ignore(std::string(kSys) + " -w net.ipv4.conf." + host_veth_ + ".rp_filter=0");

  run_or_throw(nx(netns_, std::string(kIp) + " addr add " + ns_addr + "/30 dev " + ns_veth_));
  run_or_throw(nx(netns_, std::string(kIp) + " link set " + ns_veth_ + " up"));
  run_ignore(nx(netns_, std::string(kSys) + " -w net.ipv4.ip_forward=1"));
  run_ignore(nx(netns_, std::string(kSys) + " -w net.ipv4.conf.all.rp_filter=0"));

  run_or_throw(nx(netns_, std::string(kIp) + " route replace default via " + host_addr + " dev " +
                             ns_veth_));
  run_ignore(nx(netns_, std::string(kIpt) + " -t nat -F"));
  run_ignore(nx(netns_, std::string(kIpt) + " -F FORWARD"));
  run_or_throw(nx(netns_, std::string(kIpt) + " -t nat -A POSTROUTING -o " + tun +
                             " -j SNAT --to-source " + fabric_ip));
  run_or_throw(nx(netns_, std::string(kIpt) + " -A FORWARD -j ACCEPT"));

  // Allowlist only — empty means no host services on the fabric IP.
  for (int p : expose_ports) {
    if (p <= 0 || p > 65535) {
      continue;
    }
    const std::string ps = std::to_string(p);
    run_or_throw(nx(netns_, std::string(kIpt) + " -t nat -A PREROUTING -d " + fabric_ip +
                               " -p tcp --dport " + ps + " -j DNAT --to-destination " + host_addr +
                               ":" + ps));
    run_or_throw(nx(netns_, std::string(kIpt) + " -t nat -A PREROUTING -d " + fabric_ip +
                               " -p udp --dport " + ps + " -j DNAT --to-destination " + host_addr +
                               ":" + ps));
    run_or_throw(nx(netns_, std::string(kIpt) + " -t nat -A OUTPUT -d " + fabric_ip +
                               " -p tcp --dport " + ps + " -j DNAT --to-destination " + host_addr +
                               ":" + ps));
    run_or_throw(nx(netns_, std::string(kIpt) + " -t nat -A OUTPUT -d " + fabric_ip +
                               " -p udp --dport " + ps + " -j DNAT --to-destination " + host_addr +
                               ":" + ps));
  }

  for (int k = 1; k <= 4; ++k) {
    if (k == local_port) {
      continue;
    }
    run_or_throw(std::string(kIp) + " route replace 10.64.0." + std::to_string(k) + "/32 via " +
                 ns_addr + " dev " + host_veth_);
  }
}
