#pragma once

#include "dialer.hpp"
#include "tun_device.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

/** Bridges TUN ↔ dial-on-demand RocketBox circuits (message-framed IP). */
class TunnelBridge {
public:
  TunnelBridge(TunDevice& tun, CircuitDialer& dialer, int local_port);
  ~TunnelBridge();

  TunnelBridge(const TunnelBridge&) = delete;
  TunnelBridge& operator=(const TunnelBridge&) = delete;

  void run();
  void stop();

  uint64_t upstream_bytes() const { return up_bytes_.load(std::memory_order_relaxed); }
  uint64_t downstream_bytes() const { return down_bytes_.load(std::memory_order_relaxed); }

private:
  void on_tunnel_message(const std::vector<uint8_t>& msg);

  TunDevice& tun_;
  CircuitDialer& dialer_;
  int local_port_;
  std::mutex write_mu_;
  std::atomic<bool> stop_{false};
  std::atomic<uint64_t> up_bytes_{0};
  std::atomic<uint64_t> down_bytes_{0};
};
