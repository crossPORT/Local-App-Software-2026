#include "bridge.hpp"
#include "fabric_source.hpp"
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
  const int src = rocketbox_lan::src_port_from_ip_packet(msg.data(), msg.size());
  if (src > 0) {
    dialer_.note_inbound_peer(src);
    // Remember peer for the bridge thread to EP4-switch (cannot ensure here — listen thread).
    pending_peer_.store(src, std::memory_order_relaxed);
  }
  dialer_.note_activity();
  down_bytes_.fetch_add(msg.size(), std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(write_mu_);
  tun_.write_packet(msg.data(), msg.size());
}

void TunnelBridge::run() {
  std::cerr << "[rocketbox-tunnel] bridging (Ctrl+C to stop)" << std::endl;
  auto last_idle_check = std::chrono::steady_clock::now();
  bool logged_src_fix = false;
  while (!stop_) {
    const int want = pending_peer_.exchange(0, std::memory_order_relaxed);
    if (want > 0) (void)dialer_.ensure(want);

    auto pkt = tun_.read_packet();
    if (pkt.empty()) {
      const auto now = std::chrono::steady_clock::now();
      if (now - last_idle_check > std::chrono::seconds(1)) {
        dialer_.tick_idle();
        last_idle_check = now;
      }
      continue;
    }

    if (force_fabric_source(pkt, local_port_) && !logged_src_fix) {
      std::cerr << "[rocketbox-tunnel] rewrote non-fabric source to 10.64.0." << local_port_
                << std::endl;
      logged_src_fix = true;
    }

    const int dest = rocketbox_lan::dest_port_from_ip_packet(pkt.data(), pkt.size());
    if (dest == 0 || dest == local_port_) {
      continue;
    }

    if (!dialer_.ensure(dest)) {
      std::cerr << "[rocketbox-tunnel] drop packet: ensure failed dest=" << dest << std::endl;
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
