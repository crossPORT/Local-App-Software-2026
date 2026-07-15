#include "dialer.hpp"

#include "peer_map.hpp"
#include "pkt_batch.hpp"

#include "usb_transfer.h"

#include <iostream>

CircuitDialer::CircuitDialer(rocketbox::RocketBoxTransport& transport, int local_port)
    : transport_(transport), local_port_(local_port) {
  last_activity_ = std::chrono::steady_clock::now();
  transport_.on_data_message([this](const std::vector<uint8_t>& body) {
    if (stop_ || body.size() < 4) {
      return;
    }
    const uint32_t len = pkt_read_u32_be(body.data());
    if (body.size() < 4u + len) {
      return;
    }
    std::vector<std::vector<uint8_t>> pkts;
    if (!unpack_ip_batch(body.data() + 4, len, &pkts)) {
      if (!logged_bad_batch_) {
        std::cerr << "[rocketbox-tunnel] drop malformed IP batch\n";
        logged_bad_batch_ = true;
      }
      return;
    }
    MsgHandler h;
    {
      std::lock_guard<std::mutex> lock(mu_);
      h = on_msg_;
      last_activity_ = std::chrono::steady_clock::now();
    }
    if (!h) {
      return;
    }
    for (const auto& pkt : pkts) {
      h(pkt);
    }
  });
  transport_.ensure_listening();
}

CircuitDialer::~CircuitDialer() { shutdown(); }

void CircuitDialer::shutdown() {
  stop_ = true;
  try {
    transport_.clear_circuit();
  } catch (...) {
  }
  std::lock_guard<std::mutex> lock(mu_);
  active_peer_port_ = 0;
}

bool CircuitDialer::ensure(int dest_port) {
  if (dest_port < 1 || dest_port > 4 || dest_port == local_port_) {
    return false;
  }
  if (transport_.switch_dest() == dest_port) {
    std::lock_guard<std::mutex> lock(mu_);
    active_peer_port_ = dest_port;
    last_activity_ = std::chrono::steady_clock::now();
    return true;
  }
  try {
    transport_.ensure_circuit(rocketbox_lan::system_id_for_port(dest_port));
  } catch (const std::exception& e) {
    std::cerr << "[rocketbox-tunnel] ensure_circuit failed: " << e.what() << std::endl;
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  active_peer_port_ = dest_port;
  last_activity_ = std::chrono::steady_clock::now();
  return true;
}

void CircuitDialer::on_message(MsgHandler handler) {
  std::lock_guard<std::mutex> lock(mu_);
  on_msg_ = std::move(handler);
}

void CircuitDialer::note_activity() {
  std::lock_guard<std::mutex> lock(mu_);
  last_activity_ = std::chrono::steady_clock::now();
}

void CircuitDialer::note_inbound_peer(int peer_port) {
  if (peer_port < 1 || peer_port > 4 || peer_port == local_port_) return;
  note_activity();
}

void CircuitDialer::tick_idle() {
  int peer = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (active_peer_port_ == 0 || stop_) {
      return;
    }
    const auto idle = std::chrono::steady_clock::now() - last_activity_;
    if (idle < std::chrono::seconds(rocketbox_lan::kIdleDisconnectSec)) {
      return;
    }
    peer = active_peer_port_;
    active_peer_port_ = 0;
  }
  std::cerr << "[rocketbox-tunnel] idle disconnect peer port " << peer << std::endl;
  // With EP4 gated off, clear_circuit only churns software dest — skip it.
  if (!ep4_dynamic_switch_enabled()) {
    return;
  }
  try {
    transport_.clear_circuit();
  } catch (...) {
  }
}

bool CircuitDialer::circuit_open() const {
  return transport_.switch_dest() != 0;
}
