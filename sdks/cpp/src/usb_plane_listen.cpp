#include "usb_plane.hpp"

#include "platform_util.h"
#include "usb_protocol.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

namespace rocketbox {
namespace detail {
namespace {

constexpr unsigned kListenHeaderTimeoutMs = 300;

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
    const std::string path = platform::create_empty_temp_file("rocketbox-sdk-listen-");
    if (path.empty()) {
        return;
    }
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

        // Blocks up to kListenHeaderTimeoutMs inside libusb — no busy-spin.
        auto r = controller_->receive_on_port(port_index(), path, nullptr, kListenHeaderTimeoutMs,
                                              usb_protocol::kFrameKindPayload);
        if (listen_stop_ || !r.ok) {
            continue;
        }
        std::ifstream in(path, std::ios::binary);
        std::vector<uint8_t> body((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        if (!body.empty()) {
            cb(body);
        }
    }
    std::remove(path.c_str());
}

}  // namespace detail
}  // namespace rocketbox
