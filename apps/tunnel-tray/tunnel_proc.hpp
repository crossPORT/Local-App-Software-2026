#pragma once

#include "listen_ports.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tunnel_tray {

struct TunnelConfig {
  int port = 1;
  std::string transport = "usb";  // usb | sim
  std::string iface;
  std::vector<Endpoint> expose;
#if defined(_WIN32) || defined(__APPLE__)
  bool use_netns = false;
#else
  bool use_netns = true;
#endif
  std::string tunnel_bin;  // empty = auto-detect
};

class TunnelProcess {
public:
  ~TunnelProcess() { stop(); }

  bool start(const TunnelConfig& cfg, std::string& error);
  void stop();
  bool running() const;
  long pid() const { return pid_; }

private:
  bool child_exited(int* status_out) const;
  static bool tunnel_ready(const TunnelConfig& cfg);

  mutable long pid_ = 0;
  mutable bool helper_managed_ = false;
  int port_ = 1;
};

std::string default_tunnel_bin();

struct TunnelRates {
  uint64_t up_bps = 0;
  uint64_t down_bps = 0;
  int display_port = 0;
  std::string serial;
  bool ok = false;
};

/** Read /run/rocketbox/tunnel-<port>.stats written by rocketbox-tunnel. */
TunnelRates read_tunnel_rates(int port);

std::string format_rate(uint64_t bps);

}  // namespace tunnel_tray
