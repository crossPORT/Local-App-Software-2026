#include "bridge.hpp"
#include "peer_map.hpp"

#include <chrono>
#include <iostream>
#include <thread>

TunnelBridge::TunnelBridge(TunDevice& tun, CircuitDialer& dialer, int local_port)
    : tun_(tun), dialer_(dialer), local_port_(local_port) {
  dialer_.on_connection([this](std::shared_ptr<rocketbox::Connection> c) { attach_conn(std::move(c)); });
}

TunnelBridge::~TunnelBridge() { stop(); }

void TunnelBridge::stop() { stop_ = true; }

void TunnelBridge::attach_conn(std::shared_ptr<rocketbox::Connection> conn) {
  if (!conn) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(conn_mu_);
    conn_ = conn;
  }
  conn->OnMessageReceived([this](const std::vector<uint8_t>& msg) { on_fabric_message(msg); });
}

void TunnelBridge::on_fabric_message(const std::vector<uint8_t>& msg) {
  if (stop_ || msg.empty()) {
    return;
  }
  dialer_.note_activity();
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

    const int dest = fabric_lan::dest_port_from_ip_packet(pkt.data(), pkt.size());
    if (dest == 0 || dest == local_port_) {
      continue;
    }

    auto conn = dialer_.ensure(dest);
    if (!conn || conn->GetState() != "open") {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(conn_mu_);
      conn_ = conn;
    }
    dialer_.note_activity();
    conn->SendMessage(pkt);

    const auto now = std::chrono::steady_clock::now();
    if (now - last_idle_check > std::chrono::seconds(1)) {
      dialer_.tick_idle();
      last_idle_check = now;
    }
  }
}
