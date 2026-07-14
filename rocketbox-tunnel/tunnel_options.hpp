#pragma once

#include "expose_spec.hpp"
#include "rocketbox/sdk.h"

#include <string>
#include <vector>

struct TunnelOptions {
  int port = 0;
  int ping_peer = 0;
  std::string iface;
#if defined(__linux__)
  bool use_netns = true;
#else
  bool use_netns = false;
#endif
  rocketbox::TransportMode transport = rocketbox::TransportMode::Usb;
  std::vector<ExposeRule> expose;
};

const char* tunnel_transport_name(rocketbox::TransportMode t);
void tunnel_usage(const char* argv0);
/** Returns false if --help / --version. Throws on bad args. */
bool tunnel_parse_args(int argc, char** argv, TunnelOptions& out);
