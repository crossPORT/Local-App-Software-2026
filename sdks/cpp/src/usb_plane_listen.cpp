#include "usb_plane.hpp"

#include "usb_protocol.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace rocketbox {
namespace detail {
namespace {

unsigned listen_header_timeout_ms(bool stream) {
  // Short polls so pause/switch can take the IN lock quickly after IN timeout.
  return stream ? 10u : 300u;
}

}  // namespace

void UsbPlane::on_data_message(std::function<void(const std::vector<uint8_t>&)> cb) {
  {
    std::lock_guard<std::mutex> lock(listen_mu_);
    on_msg_ = std::move(cb);
  }
  if (connected_ && on_msg_) {
    start_listen();
  }
}

void UsbPlane::ensure_listening() {
  if (connected_) {
    start_listen();
  }
}

void UsbPlane::start_listen() {
  if (listen_thread_.joinable()) {
    return;
  }
  listen_stop_ = false;
  {
    std::lock_guard<std::mutex> lock(pause_mu_);
    pause_depth_ = 0;
    listen_in_recv_ = false;
  }
  listen_thread_ = std::thread([this] {
    try {
      listen_loop();
    } catch (const std::exception& e) {
      std::cerr << "[rocketbox] listen thread exited: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "[rocketbox] listen thread exited: unknown" << std::endl;
    }
    std::lock_guard<std::mutex> lock(pause_mu_);
    listen_in_recv_ = false;
    pause_depth_ = 0;
    pause_cv_.notify_all();
  });
}

void UsbPlane::stop_listen() {
  listen_stop_ = true;
  {
    std::lock_guard<std::mutex> lock(pause_mu_);
    pause_depth_ = 0;
    pause_cv_.notify_all();
  }
  if (listen_thread_.joinable()) {
    listen_thread_.join();
  }
}

void UsbPlane::pause_listen_for_usb() {
  if (!listen_thread_.joinable()) {
    return;
  }
  std::unique_lock<std::mutex> lock(pause_mu_);
  ++pause_depth_;
  // Wait only until the current IN finishes (or stop) — no timed polling.
  pause_cv_.wait(lock, [this] {
    return !listen_in_recv_ || listen_stop_.load(std::memory_order_acquire);
  });
}

void UsbPlane::resume_listen_for_usb() {
  if (!listen_thread_.joinable()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(pause_mu_);
    if (pause_depth_ > 0) {
      --pause_depth_;
    }
    if (pause_depth_ == 0) {
      pause_cv_.notify_all();
    }
  }
  // Repost IN as soon as exclusion ends (peer may already be answering).
  ensure_listening();
}

void UsbPlane::run_exclusive(const std::function<void()>& fn) {
  ListenUsbPause pause(*this);
  fn();
}

void UsbPlane::listen_loop() {
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
    auto r = controller_->receive_buffer(port_index(), &body,
                                         listen_header_timeout_ms(stream_mode_),
                                         usb_protocol::kFrameKindPayload);
    {
      std::lock_guard<std::mutex> lock(pause_mu_);
      listen_in_recv_ = false;
      pause_cv_.notify_all();
    }
    if (listen_stop_ || !r.ok) {
      continue;
    }
    if (!body.empty()) {
      cb(body);
    }
  }
}

}  // namespace detail
}  // namespace rocketbox
