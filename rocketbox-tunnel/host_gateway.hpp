#pragma once

#include "expose_spec.hpp"

#include <string>
#include <vector>

/**
 * Tunnel expose filter: ICMP on fabric IP always; TCP/UDP allowlist only.
 * Linux: netns + DNAT. Windows: WFP. macOS: pf. Second arg is netns (Linux) or iface.
 */
class HostGateway {
public:
  HostGateway() = default;
  ~HostGateway();

  HostGateway(const HostGateway&) = delete;
  HostGateway& operator=(const HostGateway&) = delete;

  void install(int local_port, const std::string& netns_or_iface,
               const std::vector<ExposeRule>& expose);
  /** Rebuild allowlist without tearing down the tunnel path. */
  void set_expose(const std::vector<ExposeRule>& expose);
  void remove();

  const std::string& host_veth() const { return host_veth_; }

private:
  int port_ = 0;
  std::string netns_;
  std::string host_veth_;
  std::string ns_veth_;
  void* platform_ = nullptr;  // OS filter state (WFP / pf)
};
