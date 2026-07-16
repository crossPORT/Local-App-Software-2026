#include "bridge.hpp"

#include "fabric_source.hpp"
#include "icmp_ping.hpp"
#include "peer_map.hpp"
#include "pkt_batch.hpp"
#include "tunnel_log.hpp"

#include <chrono>
#include <iostream>
#include <utility>

namespace {

constexpr int kIdleTunWaitMs = 20;  // was 250 — kept ICMP replies waiting on TUN poll

}  // namespace

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
    pending_peer_.store(src, std::memory_order_relaxed);
  }
  dialer_.note_activity();
  down_bytes_.fetch_add(msg.size(), std::memory_order_relaxed);

  if (rocketbox_icmp::is_echo_request(msg.data(), msg.size(), local_port_)) {
    auto reply = rocketbox_icmp::make_echo_reply(msg.data(), msg.size());
    if (reply.empty()) return;
    const int dest = rocketbox_lan::dest_port_from_ip_packet(reply.data(), reply.size());
    if (dest == 0 || dest == local_port_ || !dialer_.deliver(dest, reply)) {
      rocketbox_tunnel_log("drop ICMP reply: deliver failed dest=" + std::to_string(dest));
      return;
    }
    up_bytes_.fetch_add(reply.size(), std::memory_order_relaxed);
    if (!logged_icmp_reply_) {
      rocketbox_tunnel_log("ICMP echo reply (userspace) to 10.64.0." + std::to_string(dest));
      logged_icmp_reply_ = true;
    }
    return;
  }

  std::lock_guard<std::mutex> lock(write_mu_);
  tun_.write_packet(msg.data(), msg.size());
}

void TunnelBridge::flush_batch() {
  if (batch_.empty()) return;
  const int dest = batch_dest_;
  auto pkts = std::move(batch_);
  const std::size_t bytes = batch_bytes_;
  batch_.clear();
  batch_bytes_ = 0;
  batch_dest_ = 0;
  if (!dialer_.deliver_batch(dest, pkts)) {
    std::cerr << "[rocketbox-tunnel] drop batch: deliver failed dest=" << dest
              << " pkts=" << pkts.size() << std::endl;
    return;
  }
  up_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void TunnelBridge::queue_packet(int dest, std::vector<uint8_t> pkt) {
  // ICMP: never coalesce — 2 ms batch + USB pause dominate ping RTT.
  if (rocketbox_icmp::is_icmp(pkt.data(), pkt.size())) {
    flush_batch();
    if (!dialer_.deliver(dest, pkt)) {
      std::cerr << "[rocketbox-tunnel] drop ICMP: deliver failed dest=" << dest << std::endl;
      return;
    }
    up_bytes_.fetch_add(pkt.size(), std::memory_order_relaxed);
    return;
  }
  if (batch_dest_ != 0 && batch_dest_ != dest) {
    flush_batch();
  }
  if (!batch_.empty() && batch_bytes_ + pkt.size() > kBatchMaxBytes) {
    flush_batch();
  }
  if (batch_.empty()) {
    batch_dest_ = dest;
    batch_start_ = std::chrono::steady_clock::now();
  }
  batch_bytes_ += pkt.size();
  batch_.push_back(std::move(pkt));
  if (batch_bytes_ >= kBatchFlushBytes || batch_bytes_ >= kBatchMaxBytes) {
    flush_batch();
  }
}

void TunnelBridge::run() {
  std::cerr << "[rocketbox-tunnel] bridging (Ctrl+C to stop)" << std::endl;
  auto last_idle_check = std::chrono::steady_clock::now();
  bool logged_src_fix = false;
  try {
    while (!stop_) {
      const int want = pending_peer_.exchange(0, std::memory_order_relaxed);
      if (want > 0) (void)dialer_.ensure(want);

      const int wait_ms = batch_.empty() ? kIdleTunWaitMs : kBatchFlushMs;
      auto pkt = tun_.read_packet(wait_ms);
      if (pkt.empty()) {
        if (!batch_.empty()) {
          flush_batch();
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - last_idle_check > std::chrono::seconds(1)) {
          dialer_.tick_idle();
          last_idle_check = now;
        }
        continue;
      }

      for (;;) {
        if (force_fabric_source(pkt, local_port_) && !logged_src_fix) {
          std::cerr << "[rocketbox-tunnel] rewrote non-fabric source to 10.64.0." << local_port_
                    << std::endl;
          logged_src_fix = true;
        }
        const int dest = rocketbox_lan::dest_port_from_ip_packet(pkt.data(), pkt.size());
        if (dest != 0 && dest != local_port_) {
          queue_packet(dest, std::move(pkt));
        }
        pkt = tun_.read_packet(0);
        if (pkt.empty()) break;
      }

      if (!batch_.empty()) {
        const auto age = std::chrono::steady_clock::now() - batch_start_;
        if (age >= std::chrono::milliseconds(kBatchFlushMs) || batch_bytes_ >= kBatchFlushBytes) {
          flush_batch();
        }
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - last_idle_check > std::chrono::seconds(1)) {
        dialer_.tick_idle();
        last_idle_check = now;
      }
    }
    flush_batch();
  } catch (const std::exception& e) {
    rocketbox_tunnel_log(std::string("bridge abort: ") + e.what());
    stop_ = true;
  } catch (...) {
    rocketbox_tunnel_log("bridge abort: unknown");
    stop_ = true;
  }
}
