#pragma once

#include "rocketbox/sdk.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

/**
 * Dial-on-demand: one circuit at a time to fabric peer for 10.64.0.K.
 * Incoming circuits are accepted and become the active link.
 */
class CircuitDialer {
public:
  using ConnHandler = std::function<void(std::shared_ptr<rocketbox::Connection>)>;

  CircuitDialer(rocketbox::Session& session, int local_port);
  ~CircuitDialer();

  CircuitDialer(const CircuitDialer&) = delete;
  CircuitDialer& operator=(const CircuitDialer&) = delete;

  /** Ensure circuit to dest fabric port (1..4). Returns null on failure. */
  std::shared_ptr<rocketbox::Connection> ensure(int dest_port);

  void on_connection(ConnHandler handler);
  void note_activity();
  void tick_idle();
  void shutdown();

private:
  void bind_conn(std::shared_ptr<rocketbox::Connection> conn, int peer_port);
  void clear_conn(const std::string& reason);

  rocketbox::Session& session_;
  int local_port_;
  std::mutex mu_;
  std::shared_ptr<rocketbox::Connection> conn_;
  int active_peer_port_ = 0;
  std::chrono::steady_clock::time_point last_activity_{};
  ConnHandler on_conn_;
  std::atomic<bool> stop_{false};
};
