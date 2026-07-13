#pragma once

#include "bridge.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

/** Publishes ↑/↓ rates to /run/rocketbox/tunnel-<port>.stats while bridging. */
class TunnelStatsPublisher {
public:
  TunnelStatsPublisher(int port, TunnelBridge& bridge, std::string serial = {});
  ~TunnelStatsPublisher();

  TunnelStatsPublisher(const TunnelStatsPublisher&) = delete;
  TunnelStatsPublisher& operator=(const TunnelStatsPublisher&) = delete;

private:
  void loop();
  void write_file(uint64_t up_bps, uint64_t down_bps, uint64_t up_tot, uint64_t down_tot);
  void unlink_file();

  int port_;
  std::string serial_;
  TunnelBridge& bridge_;
  std::atomic<bool> stop_{false};
  std::thread thr_;
};
