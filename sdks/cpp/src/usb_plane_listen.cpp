#include "usb_plane.hpp"

#include "event_log.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace rocketbox {
namespace detail {

std::string UsbPlane::listen_state_string() {
  int depth = 0;
  bool in_recv = false;
  bool listening = false;
  {
    std::lock_guard<std::mutex> lock(pause_mu_);
    depth = pause_depth_;
    in_recv = listen_in_recv_;
    listening = listen_thread_.joinable();
  }
  std::string s = "pause_depth=" + std::to_string(depth) +
                  " in_recv=" + std::string(in_recv ? "1" : "0") +
                  " listening=" + std::string(listening ? "1" : "0") +
                  " stream=" + std::string(stream_mode_ ? "1" : "0") +
                  " out_seq=" + std::to_string(out_seq_.load(std::memory_order_relaxed));
  if (controller_) {
    s += " switch_dest=" + std::to_string(controller_->last_switch_dest());
    s += " stream_open=" + std::string(controller_->stream_device_open() ? "1" : "0");
  }
  return s;
}

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
  const auto t0 = std::chrono::steady_clock::now();
  bool in_recv = false;
  int depth = 0;
  {
    std::unique_lock<std::mutex> lock(pause_mu_);
    in_recv = listen_in_recv_;
    ++pause_depth_;
    depth = pause_depth_;
    pause_cv_.wait(lock, [this] {
      return !listen_in_recv_ || listen_stop_.load(std::memory_order_acquire);
    });
  }
  const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - t0)
                           .count();
  event_log(resolved_port_index(), "listen_pause",
            "wait_ms=" + std::to_string(wait_ms) + " in_recv_was=" + (in_recv ? "1" : "0") +
                " depth=" + std::to_string(depth) + " " + listen_state_string());
}

void UsbPlane::resume_listen_for_usb() {
  if (!listen_thread_.joinable()) {
    return;
  }
  int depth = 0;
  {
    std::lock_guard<std::mutex> lock(pause_mu_);
    if (pause_depth_ > 0) {
      --pause_depth_;
    }
    depth = pause_depth_;
    if (pause_depth_ == 0) {
      pause_cv_.notify_all();
    }
  }
  event_log(resolved_port_index(), "listen_resume",
            "depth=" + std::to_string(depth) + " " + listen_state_string());
  ensure_listening();
}

void UsbPlane::wait_listen_in_armed(unsigned max_ms) {
  if (!stream_mode_ || !listen_thread_.joinable() || max_ms == 0) {
    return;
  }
  const auto t0 = std::chrono::steady_clock::now();
  bool armed = false;
  {
    std::unique_lock<std::mutex> lock(pause_mu_);
    pause_cv_.wait_for(lock, std::chrono::milliseconds(max_ms), [this] {
      return listen_stop_.load(std::memory_order_acquire) ||
             (pause_depth_ == 0 && listen_in_recv_);
    });
    armed = !listen_stop_.load(std::memory_order_acquire) && pause_depth_ == 0 &&
            listen_in_recv_;
  }
  const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - t0)
                           .count();
  event_log(resolved_port_index(), "listen_armed",
            "wait_ms=" + std::to_string(wait_ms) + " armed=" + (armed ? "1" : "0") + " " +
                listen_state_string());
}

void UsbPlane::run_exclusive(const std::function<void()>& fn) {
  ListenUsbPause pause(*this);
  fn();
}

}  // namespace detail
}  // namespace rocketbox
