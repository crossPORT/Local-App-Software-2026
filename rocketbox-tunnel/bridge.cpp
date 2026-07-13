#include "bridge.hpp"
#include "peer_map.hpp"

#include <chrono>
#include <iostream>

TunnelBridge::TunnelBridge(TunDevice& tun, CircuitDialer& dialer, int local_port)
    : tun_(tun), dialer_(dialer), local_port_(local_port) {
  dialer_.on_message([this](const std::vector<uint8_t>& msg) { on_tunnel_message(msg); });
}

TunnelBridge::~TunnelBridge() { stop(); }

void TunnelBridge::stop() { stop_ = true; }

void TunnelBridge::on_tunnel_message(const std::vector<uint8_t>& msg) {
  if (stop_ || msg.empty()) {
    return;
  }
  dialer_.note_activity();
  down_bytes_.fetch_add(msg.size(), std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(write_mu_);
  tun_.write_packet(msg.data(), msg.size());
}

void TunnelBridge::run() {
  std::cerr << "[rocketbox-tunnel] bridging (Ctrl+C to stop)" << std::endl;
  auto last_idle_check = std::chrono::steady_clock::now();
  while (!stop_) {
    auto pkt = tun_.read_packet();
    if (pkt.empty()) {
      if (stop_) {
        break;
      }
      continue;
    }

    const int dest = rocketbox_lan::dest_port_from_ip_packet(pkt.data(), pkt.size());
    if (dest == 0 || dest == local_port_) {
      continue;
    }

    if (!dialer_.ensure(dest)) {
      continue;
    }
    dialer_.note_activity();
    dialer_.send_message(pkt);
    up_bytes_.fetch_add(pkt.size(), std::memory_order_relaxed);

    const auto now = std::chrono::steady_clock::now();
    if (now - last_idle_check > std::chrono::seconds(1)) {
      dialer_.tick_idle();
      last_idle_check = now;
    }
  }
}
