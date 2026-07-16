#include "bridge.hpp"

#include "icmp_ping.hpp"
#include "peer_map.hpp"
#include "tunnel_log.hpp"

#include <chrono>
#include <utility>

void TunnelBridge::queue_icmp_reply(std::vector<uint8_t> reply) {
  std::lock_guard<std::mutex> lock(icmp_mu_);
  icmp_pending_.push_back(std::move(reply));
}

void TunnelBridge::flush_icmp_replies() {
  std::vector<std::vector<uint8_t>> pending;
  {
    std::lock_guard<std::mutex> lock(icmp_mu_);
    pending.swap(icmp_pending_);
  }
  for (auto& reply : pending) {
    if (stop_ || reply.empty()) {
      continue;
    }
    const int dest = rocketbox_lan::dest_port_from_ip_packet(reply.data(), reply.size());
    if (dest == 0 || dest == local_port_) {
      rocketbox_tunnel_log("icmp_reply_drop bad dest=" + std::to_string(dest));
      continue;
    }
    const auto t0 = std::chrono::steady_clock::now();
    rocketbox_tunnel_log("icmp_reply_begin dest=" + std::to_string(dest) +
                         " bytes=" + std::to_string(reply.size()));
    const bool ok = dialer_.deliver(dest, reply);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    rocketbox_tunnel_log(std::string("icmp_reply_end dest=") + std::to_string(dest) +
                         " ok=" + (ok ? "1" : "0") + " elapsed_ms=" + std::to_string(ms));
    if (!ok) {
      rocketbox_tunnel_log("drop ICMP reply: deliver failed dest=" + std::to_string(dest));
      continue;
    }
    up_bytes_.fetch_add(reply.size(), std::memory_order_relaxed);
    if (!logged_icmp_reply_) {
      rocketbox_tunnel_log("ICMP echo reply (userspace) to 10.64.0." + std::to_string(dest));
      logged_icmp_reply_ = true;
    }
  }
}
