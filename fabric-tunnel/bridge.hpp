#pragma once

#include "dialer.hpp"
#include "tun_device.hpp"

#include <atomic>
#include <memory>
#include <mutex>

/** Bridges TUN ↔ dial-on-demand RocketBox circuits (message-framed IP). */
class TunnelBridge {
public:
  TunnelBridge(TunDevice& tun, CircuitDialer& dialer, int local_port);
  ~TunnelBridge();

  TunnelBridge(const TunnelBridge&) = delete;
  TunnelBridge& operator=(const TunnelBridge&) = delete;

  void run();
  void stop();

private:
  void on_fabric_message(const std::vector<uint8_t>& msg);
  void attach_conn(std::shared_ptr<rocketbox::Connection> conn);

  TunDevice& tun_;
  CircuitDialer& dialer_;
  int local_port_;
  std::mutex write_mu_;
  std::mutex conn_mu_;
  std::shared_ptr<rocketbox::Connection> conn_;
  std::atomic<bool> stop_{false};
};
