#include "usb_plane.hpp"

#include "usb_protocol.h"

#include <chrono>
#include <thread>

namespace rocketbox {
namespace detail {
namespace {

unsigned listen_header_timeout_ms(bool stream) {
  // Stream keeps the USB handle open — short polls so send can turn around replies.
  return stream ? 50u : 300u;
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
  listen_thread_ = std::thread([this] { listen_loop(); });
}

void UsbPlane::stop_listen() {
  listen_stop_ = true;
  if (listen_thread_.joinable()) {
    listen_thread_.join();
  }
}

void UsbPlane::listen_loop() {
  while (!listen_stop_.load(std::memory_order_acquire)) {
    if (!controller_ || !connected_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    std::function<void(const std::vector<uint8_t>&)> cb;
    {
      std::lock_guard<std::mutex> lock(listen_mu_);
      cb = on_msg_;
    }
    if (!cb) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    std::vector<uint8_t> body;
    auto r = controller_->receive_buffer(port_index(), &body,
                                         listen_header_timeout_ms(stream_mode_),
                                         usb_protocol::kFrameKindPayload);
    if (listen_stop_ || !r.ok) {
      // Brief yield so switch/send can take usb_mutex_ (avoid "USB port busy").
      if (!stream_mode_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      continue;
    }
    if (!body.empty()) {
      cb(body);
    }
  }
}

}  // namespace detail
}  // namespace rocketbox
