#include "dialer.hpp"

#include "dialer_frame.hpp"
#include "peer_map.hpp"
#include "pkt_batch.hpp"
#include "tunnel_log.hpp"

#include "usb_transfer.h"

#include <chrono>
#include <iostream>

bool CircuitDialer::send_framed_batch(int dest_port, const std::vector<uint8_t>& batch_body,
                                      bool dial) {
  if (dest_port < 1 || dest_port > 4 || dest_port == local_port_ || batch_body.empty()) {
    return false;
  }
  const auto framed = frame_batch_body(batch_body);
  for (int attempt = 0; attempt < 2; ++attempt) {
    const auto t0 = std::chrono::steady_clock::now();
    try {
      if (dial && (attempt > 0 || transport_.switch_dest() != dest_port)) {
        transport_.ensure_circuit(rocketbox_lan::system_id_for_port(dest_port));
      }
      {
        std::lock_guard<std::mutex> lock(mu_);
        active_peer_port_ = dest_port;
        last_activity_ = std::chrono::steady_clock::now();
      }
      rocketbox_tunnel_log("deliver_begin dest=" + std::to_string(dest_port) +
                          " attempt=" + std::to_string(attempt + 1) +
                          " bytes=" + std::to_string(framed.size()) +
                          " dial=" + std::to_string(dial ? 1 : 0));
      transport_.send_bytes(framed);
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
      rocketbox_tunnel_log("deliver_end dest=" + std::to_string(dest_port) +
                           " attempt=" + std::to_string(attempt + 1) +
                           " ok=1 elapsed_ms=" + std::to_string(ms));
      note_activity();
      return true;
    } catch (const std::exception& e) {
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
      rocketbox_tunnel_log("deliver_end dest=" + std::to_string(dest_port) +
                           " attempt=" + std::to_string(attempt + 1) +
                           " ok=0 elapsed_ms=" + std::to_string(ms) + " err=" + e.what());
      if (attempt == 0 && dial) {
        if (ep4_dynamic_switch_enabled()) {
          try {
            transport_.clear_circuit();
          } catch (...) {
          }
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
    rocketbox_tunnel_log(std::string("send_message failed: ") + e.what());
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
