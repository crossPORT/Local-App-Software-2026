#include "usb_plane.hpp"

#include "event_log.h"
#include "usb_protocol.h"

#include <chrono>
#include <string>
#include <thread>

namespace rocketbox {
namespace detail {
namespace {

unsigned listen_header_timeout_ms(bool stream) { return stream ? 10u : 300u; }

bool is_header_timeout(const std::string& err) { return err == "Header read failed"; }

}  // namespace

void UsbPlane::listen_loop() {
  using Clock = std::chrono::steady_clock;
  uint64_t polls = 0, oks = 0, tos = 0, fails = 0;
  auto last_hb = Clock::now();
  auto last_ok = Clock::now();
  bool ever_ok = false;
  event_log(resolved_port_index(), "listen_start", listen_state_string());

  while (!listen_stop_.load(std::memory_order_acquire)) {
    {
      std::unique_lock<std::mutex> lock(pause_mu_);
      pause_cv_.wait(lock, [this] {
        return pause_depth_ == 0 || listen_stop_.load(std::memory_order_acquire);
      });
      if (listen_stop_.load(std::memory_order_acquire)) {
        break;
      }
      listen_in_recv_ = true;
    }

    if (!controller_ || !connected_) {
      {
        std::lock_guard<std::mutex> lock(pause_mu_);
        listen_in_recv_ = false;
        pause_cv_.notify_all();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    std::function<void(const std::vector<uint8_t>&)> cb;
    {
      std::lock_guard<std::mutex> lock(listen_mu_);
      cb = on_msg_;
    }
    if (!cb) {
      {
        std::lock_guard<std::mutex> lock(pause_mu_);
        listen_in_recv_ = false;
        pause_cv_.notify_all();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    std::vector<uint8_t> body;
    const auto t0 = Clock::now();
    auto r = controller_->receive_buffer(port_index(), &body,
                                         listen_header_timeout_ms(stream_mode_),
                                         usb_protocol::kFrameKindPayload);
    {
      std::lock_guard<std::mutex> lock(pause_mu_);
      listen_in_recv_ = false;
      pause_cv_.notify_all();
    }
    ++polls;
    const auto poll_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    if (listen_stop_) {
      break;
    }
    if (r.ok) {
      ++oks;
      ever_ok = true;
      last_ok = Clock::now();
      event_log(resolved_port_index(), "listen_in_ok",
                "bytes=" + std::to_string(body.size()) + " poll_ms=" + std::to_string(poll_ms) +
                    " " + listen_state_string());
      if (!body.empty()) {
        cb(body);
      }
    } else if (is_header_timeout(r.error_message)) {
      ++tos;
    } else {
      ++fails;
      event_log(resolved_port_index(), "listen_in_fail",
                "poll_ms=" + std::to_string(poll_ms) + " err=" + r.error_message + " " +
                    listen_state_string());
    }

    if (Clock::now() - last_hb >= std::chrono::seconds(1)) {
      const auto age_ms =
          ever_ok ? std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - last_ok)
                        .count()
                  : -1;
      event_log(resolved_port_index(), "listen_hb",
                "polls=" + std::to_string(polls) + " ok=" + std::to_string(oks) +
                    " hdr_to=" + std::to_string(tos) + " fail=" + std::to_string(fails) +
                    " last_ok_age_ms=" + std::to_string(age_ms) + " " + listen_state_string());
      last_hb = Clock::now();
    }
  }
  event_log(resolved_port_index(), "listen_exit",
            "polls=" + std::to_string(polls) + " ok=" + std::to_string(oks) +
                " hdr_to=" + std::to_string(tos) + " fail=" + std::to_string(fails) + " " +
                listen_state_string());
}

}  // namespace detail
}  // namespace rocketbox
