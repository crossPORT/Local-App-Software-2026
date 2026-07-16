#pragma once

#include "dialer.hpp"
#include "tun_device.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

/** Bridges TUN ↔ dial-on-demand RocketBox circuits (batched IP frames). */
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
  void flush_icmp();
  void flush_batch();
  void queue_packet(int dest, std::vector<uint8_t> pkt);

  TunDevice& tun_;
  CircuitDialer& dialer_;
  int local_port_;
  std::mutex write_mu_;
  std::mutex icmp_mu_;
  std::atomic<bool> stop_{false};
  std::atomic<int> pending_peer_{0};
  std::atomic<bool> icmp_ready_{false};
  std::atomic<uint64_t> up_bytes_{0};
  std::atomic<uint64_t> down_bytes_{0};
  bool logged_icmp_reply_{false};
  std::vector<uint8_t> pending_icmp_;
  int pending_icmp_dest_ = 0;

  int batch_dest_ = 0;
  std::vector<std::vector<uint8_t>> batch_;
  std::size_t batch_bytes_ = 0;
  std::chrono::steady_clock::time_point batch_start_{};
};
