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
  bool ep4_dynamic_switch = false;
  bool high_priority = true;
  std::string tunnel_bin;  // empty = auto-detect
};

class TunnelProcess {
public:
  ~TunnelProcess() { stop(); }

  bool start(const TunnelConfig& cfg, std::string& error);
  /** Write expose file and SIGHUP a live tunnel (adopt orphan pid if needed). */
  bool reload(const TunnelConfig& cfg, std::string& error);
  void stop();
  bool running() const;
  long pid() const { return pid_; }

private:
  bool child_exited(int* status_out) const;
  bool tunnel_ready(const TunnelConfig& cfg) const;

  mutable long pid_ = 0;
  mutable bool helper_managed_ = false;
  int port_ = 1;
  std::vector<long> pids_at_start_;
};

std::string default_tunnel_bin();

struct TunnelRates {
  uint64_t up_bps = 0;
  uint64_t down_bps = 0;
  int display_port = 0;
  long pid = 0;
  std::string serial;
  bool ok = false;
};

/** PIDs currently advertised in tunnel-*.stats (for start-wait guards). */
std::vector<long> snapshot_stats_pids();

/** True when stats show a new bridging pid not present in `before`. */
bool stats_ready_new_pid(int prefer_port, const std::vector<long>& before, int* port_out = nullptr);

/** Read /run/rocketbox/tunnel-<port>.stats written by rocketbox-tunnel. */
TunnelRates read_tunnel_rates(int port);

/** True if stats name a still-living tunnel process for this port. */
bool live_tunnel_holds_port(int display_port);

/** First Port 1–4 with a live tunnel, or 0. */
int live_tunnel_port();

std::string format_rate(uint64_t bps);

}  // namespace tunnel_tray
