#pragma once

#include "fabric_session_message.h"
#include "transfer_controller.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

class SessionListener {
public:
    using MessageCallback = std::function<void(const FabricSessionMessage&)>;

    using BeforeListenCallback = std::function<void()>;

    SessionListener(TransferController* controller,
                    int port_index,
                    MessageCallback on_message,
                    BeforeListenCallback on_before_listen = {});
    ~SessionListener();

    void set_session_header_timeout_ms(unsigned ms);
    void set_tight_poll(bool enabled);

    void start();
    void stop();
    void pause();
    void resume();
    bool is_paused() const;
    bool recently_finished_receive(std::chrono::milliseconds gap) const;

private:
    void listen_loop();

    TransferController* controller_ = nullptr;
    int port_index_ = 0;
    MessageCallback on_message_;
    BeforeListenCallback on_before_listen_;

    unsigned session_header_timeout_ms_ = 2000;
    unsigned handshake_poll_timeout_ms_ = 350;
    std::atomic<bool> tight_poll_{false};

    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> in_receive_{false};
    std::atomic<int64_t> last_receive_end_ms_{0};
    std::mutex pause_mutex_;
    std::condition_variable pause_cv_;
};

bool send_session_message(TransferController& controller,
                          int sender_port_index,
                          const FabricSessionMessage& message,
                          std::string* error_out = nullptr,
                          unsigned timeout_ms = 0);

std::string make_session_id();
