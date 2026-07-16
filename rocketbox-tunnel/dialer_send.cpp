#include "dialer.hpp"

#include "dialer_frame.hpp"
#include "peer_map.hpp"
#include "pkt_batch.hpp"

#include "usb_transfer.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace {

/** After a failed OUT, wait before retry so the peer can finish payload
 *  fail / clear_halt instead of eating the next frame as leftover payload. */
constexpr int kDeliverRetryBackoffMs = 150;

}  // namespace

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
        if (ep4_dynamic_switch_enabled()) {
          try {
            transport_.clear_circuit();
          } catch (...) {
          }
        }
        {
          std::lock_guard<std::mutex> lock(mu_);
          active_peer_port_ = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kDeliverRetryBackoffMs));
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
