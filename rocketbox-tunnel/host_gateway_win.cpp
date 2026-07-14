#include "host_gateway.hpp"
#include "host_gateway_wfp.hpp"

#include <vector>

HostGateway::~HostGateway() { remove(); }

void HostGateway::remove() {
  if (platform_) {
    wfp_expose_close(static_cast<WfpExposeSession*>(platform_));
    platform_ = nullptr;
  }
  port_ = 0;
  netns_.clear();
  host_veth_.clear();
  ns_veth_.clear();
}

void HostGateway::install(int local_port, const std::string& iface,
                          const std::vector<ExposeRule>& expose) {
  remove();
  if (local_port < 1 || local_port > 4) return;
  port_ = local_port;
  netns_ = iface;
  auto* s = wfp_expose_open(local_port);
  platform_ = s;
  wfp_expose_apply(s, expose);
}

void HostGateway::set_expose(const std::vector<ExposeRule>& expose) {
  if (!platform_ || port_ <= 0) return;
  wfp_expose_apply(static_cast<WfpExposeSession*>(platform_), expose);
}
