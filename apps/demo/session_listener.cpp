#include "session_listener.h"

#include "booth_log.h"
#include "fabric_session_message.h"
#include "usb_protocol.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace {

constexpr unsigned kSessionSendTimeoutMs = 2500;

std::string temp_receive_path() {
    char path_template[] = "/tmp/slsfabric-sess-recv-XXXXXX";
    if (mkstemp(path_template) < 0) {
        return {};
    }
    return path_template;
}

}  // namespace

std::string make_session_id() {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream out;
    for (int i = 0; i < 16; ++i) {
        out << std::hex << dist(rng);
    }
    return out.str();
}

bool send_session_message(TransferController& controller,
                          int sender_port_index,
                          const FabricSessionMessage& message,
                          std::string* error_out,
                          unsigned timeout_ms) {
    if (controller.is_shutting_down()) {
        if (error_out) {
            *error_out = "Shutting down";
        }
        return false;
    }
    const std::string session_path = write_session_temp_file(message);
    if (session_path.empty()) {
        if (error_out) {
            *error_out = "Could not create session message file";
        }
        return false;
    }

    const unsigned effective_timeout = timeout_ms > 0 ? timeout_ms : kSessionSendTimeoutMs;
    TransferResult send =
        controller.send_on_port(sender_port_index, session_path, nullptr,
                              effective_timeout);
    std::remove(session_path.c_str());
    if (!send.ok) {
        if (error_out) {
            *error_out = send.error_message;
        }
        return false;
    }

    return true;
}

SessionListener::SessionListener(TransferController* controller,
                                 int port_index,
                                 MessageCallback on_message)
    : controller_(controller)
    , port_index_(port_index)
    , on_message_(std::move(on_message)) {}

SessionListener::~SessionListener() {
    stop();
}

void SessionListener::start() {
    stop_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    thread_ = std::thread([this] { listen_loop(); });
}

void SessionListener::stop() {
    stop_.store(true, std::memory_order_release);
    pause_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void SessionListener::pause() {
    paused_.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> lock(pause_mutex_);
    pause_cv_.wait_for(lock, std::chrono::milliseconds(1500), [this] {
        return stop_.load(std::memory_order_acquire)
            || !in_receive_.load(std::memory_order_acquire);
    });
}

void SessionListener::resume() {
    paused_.store(false, std::memory_order_release);
    pause_cv_.notify_all();
}

bool SessionListener::recently_finished_receive(std::chrono::milliseconds gap) const {
    const int64_t end_ms = last_receive_end_ms_.load(std::memory_order_acquire);
    if (end_ms == 0) {
        return false;
    }
    const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
    return (now_ms - end_ms) < gap.count();
}

void SessionListener::listen_loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        if (paused_.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(pause_mutex_);
            pause_cv_.wait_for(lock, std::chrono::milliseconds(200), [this] {
                return stop_.load(std::memory_order_acquire)
                    || !paused_.load(std::memory_order_acquire);
            });
            continue;
        }

        if (!controller_ || !controller_->fabric_port_available()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            continue;
        }

        const std::string recv_path = temp_receive_path();
        if (recv_path.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        unsigned header_timeout = usb_protocol::kSessionHeaderTimeoutMs;
        if (stop_.load(std::memory_order_acquire)) {
            header_timeout = 400;
        }

        in_receive_.store(true, std::memory_order_release);
        TransferResult result = controller_->receive_on_port(
            port_index_, recv_path, nullptr, header_timeout);
        in_receive_.store(false, std::memory_order_release);
        last_receive_end_ms_.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count(),
            std::memory_order_release);
        pause_cv_.notify_all();
        if (stop_.load(std::memory_order_acquire)) {
            std::remove(recv_path.c_str());
            break;
        }

        if (result.ok) {
            booth_log(port_index_, "usb_recv_ok", recv_path);
            FabricSessionMessage message{};
            if (read_session_file(recv_path, message) && on_message_) {
                on_message_(message);
            } else {
                booth_log(port_index_, "session_parse_fail", recv_path);
            }
        } else if (!result.error_message.empty()
                   && result.error_message != "Header read failed") {
            booth_log(port_index_, "usb_recv_fail", result.error_message);
            if (result.error_message.find("claim") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        std::remove(recv_path.c_str());

        if (!result.ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}
