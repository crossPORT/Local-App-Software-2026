#pragma once

#include <string>
#include <vector>

/**
 * Host↔fabric gateway: routes + optional TCP/UDP port publish (DNAT allowlist).
 * Default expose list is empty — no host services on the fabric IP.
 */
class HostGateway {
public:
  HostGateway() = default;
  ~HostGateway();

  HostGateway(const HostGateway&) = delete;
  HostGateway& operator=(const HostGateway&) = delete;

  void install(int local_port, const std::string& netns, const std::vector<int>& expose_ports);
  void remove();

  const std::string& host_veth() const { return host_veth_; }

private:
  int port_ = 0;
  std::string netns_;
  std::string host_veth_;
  std::string ns_veth_;
};
