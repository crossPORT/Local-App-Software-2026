#include "host_gateway.hpp"

#include <stdexcept>

/** Windows portproxy/WFP expose — Phase 6 placeholder. */
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
      "Windows --expose is not enabled yet; omit --expose or use Linux");
}
