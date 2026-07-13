#include "host_gateway.hpp"

#include <stdexcept>
#include <string>

/** macOS pf-based expose — Phase 6 placeholder; configure_lan works without this. */
HostGateway::~HostGateway() { remove(); }

void HostGateway::remove() {
  port_ = 0;
  netns_.clear();
  host_veth_.clear();
  ns_veth_.clear();
}

void HostGateway::install(int, const std::string&, const std::vector<ExposeRule>& expose) {
  if (expose.empty()) return;
  throw std::runtime_error(
      "macOS --expose via pf is not enabled yet; omit --expose or use Linux");
}
