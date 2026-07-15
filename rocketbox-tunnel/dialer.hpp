#pragma once

#include "rocketbox/sdk.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

/** Dial-on-demand: ensure_circuit + framed send/receive on RocketBoxTransport. */
class CircuitDialer {
public:
  using MsgHandler = std::function<void(const std::vector<uint8_t>&)>;

  CircuitDialer(rocketbox::RocketBoxTransport& transport, int local_port);
  ~CircuitDialer();

  CircuitDialer(const CircuitDialer&) = delete;
  CircuitDialer& operator=(const CircuitDialer&) = delete;

  bool ensure(int dest_port);

  /**
   * EP4 connect if needed, then batched framed send (one or more IP packets).
   * Keeps EP4 aimed (HW: one switch to connect, dest=0 only on idle/shutdown).
   */
  bool deliver(int dest_port, const std::vector<uint8_t>& ip_packet);
  bool deliver_batch(int dest_port, const std::vector<std::vector<uint8_t>>& ip_packets);

  void send_message(const std::vector<uint8_t>& ip_packet);
  bool exchange_message(const std::vector<uint8_t>& ip_packet, std::vector<uint8_t>* reply,
                        unsigned timeout_ms);
  void on_message(MsgHandler handler);
  void note_activity();
  void note_inbound_peer(int peer_port);
  void tick_idle();
  void shutdown();
  bool circuit_open() const;

private:
  bool send_framed_batch(int dest_port, const std::vector<uint8_t>& batch_body, bool dial);

  rocketbox::RocketBoxTransport& transport_;
  int local_port_;
  std::mutex mu_;
  int active_peer_port_ = 0;
  std::chrono::steady_clock::time_point last_activity_{};
  MsgHandler on_msg_;
  std::atomic<bool> stop_{false};
  bool logged_bad_batch_{false};
};
