#include "dialer.hpp"

#include "dialer_frame.hpp"
#include "peer_map.hpp"
#include "pkt_batch.hpp"

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

bool CircuitDialer::send_framed_batch(int dest_port, const std::vector<uint8_t>& batch_body,
                                      bool dial) {
  if (dest_port < 1 || dest_port > 4 || dest_port == local_port_ || batch_body.empty()) {
    return false;
  }
  const auto framed = frame_batch_body(batch_body);
  for (int attempt = 0; attempt < 2; ++attempt) {
    try {
      if (dial && (attempt > 0 || transport_.switch_dest() != dest_port)) {
        transport_.ensure_circuit(rocketbox_lan::system_id_for_port(dest_port));
      }
      {
        std::lock_guard<std::mutex> lock(mu_);
        active_peer_port_ = dest_port;
        last_activity_ = std::chrono::steady_clock::now();
      }
      transport_.send_bytes(framed);
      note_activity();
      return true;
    } catch (const std::exception& e) {
      std::cerr << "[rocketbox-tunnel] deliver failed dest=" << dest_port
                << " attempt=" << (attempt + 1) << ": " << e.what() << std::endl;
      if (attempt == 0 && dial) {
        try {
          transport_.clear_circuit();
        } catch (...) {
        }
        std::lock_guard<std::mutex> lock(mu_);
        active_peer_port_ = 0;
      } else if (!dial) {
        return false;
      }
    }
  }
  return false;
}

bool CircuitDialer::deliver(int dest_port, const std::vector<uint8_t>& ip_packet) {
  return deliver_batch(dest_port, std::vector<std::vector<uint8_t>>{ip_packet});
}

bool CircuitDialer::deliver_batch(int dest_port,
                                  const std::vector<std::vector<uint8_t>>& ip_packets) {
  if (ip_packets.empty()) {
    return false;
  }
  return send_framed_batch(dest_port, pack_ip_batch(ip_packets), true);
}

void CircuitDialer::send_message(const std::vector<uint8_t>& ip_packet) {
  const auto framed = frame_batch_body(pack_ip_batch({ip_packet}));
  try {
    transport_.send_bytes(framed);
  } catch (const std::exception& e) {
    std::cerr << "[rocketbox-tunnel] send_message failed: " << e.what() << std::endl;
    throw;
  }
  note_activity();
}

bool CircuitDialer::exchange_message(const std::vector<uint8_t>& ip_packet,
                                     std::vector<uint8_t>* reply, unsigned timeout_ms) {
  if (!reply) return false;
  const auto framed = frame_batch_body(pack_ip_batch({ip_packet}));
  std::vector<uint8_t> raw;
  if (!transport_.exchange_bytes(framed, &raw, timeout_ms)) {
    return false;
  }
  if (raw.size() < 4) return false;
  const uint32_t len = pkt_read_u32_be(raw.data());
  if (raw.size() < 4u + len) return false;
  std::vector<std::vector<uint8_t>> pkts;
  if (!unpack_ip_batch(raw.data() + 4, len, &pkts) || pkts.empty()) {
    return false;
  }
  *reply = std::move(pkts.front());
  note_activity();
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
  try {
    transport_.clear_circuit();
  } catch (...) {
  }
}

bool CircuitDialer::circuit_open() const {
  return transport_.switch_dest() != 0;
}
