#pragma once

#include <string>
#include <vector>

namespace tunnel_tray {

struct TunnelConfig {
  int port = 1;
  std::string transport = "usb";  // usb | sim
  std::string iface;
  std::vector<int> expose_ports;
  bool use_netns = true;
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
  long pid_ = 0;
};

std::string default_tunnel_bin();

}  // namespace tunnel_tray
