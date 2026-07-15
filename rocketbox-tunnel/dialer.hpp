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

    /** Ensure circuit to dest fabric port (1..4). Returns false on failure. */
    bool ensure(int dest_port);

    /**
     * EP4 connect if needed, then framed send. Keeps EP4 aimed (HW: one switch
     * to connect, dest=0 only on idle/shutdown).
     */
    bool deliver(int dest_port, const std::vector<uint8_t>& msg);

    void send_message(const std::vector<uint8_t>& msg);
    /** Send and wait for one framed reply (uses transport exchange when available). */
    bool exchange_message(const std::vector<uint8_t>& msg, std::vector<uint8_t>* reply,
                          unsigned timeout_ms);
    void on_message(MsgHandler handler);
    void note_activity();
    /** Remember peer from an inbound IP packet (avoids blank active_peer on reply path). */
    void note_inbound_peer(int peer_port);
    void tick_idle();
    void shutdown();
    bool circuit_open() const;

private:
    void write_u32_be(uint8_t* buf, uint32_t val);
    uint32_t read_u32_be(const uint8_t* buf) const;

    rocketbox::RocketBoxTransport& transport_;
    int local_port_;
    std::mutex mu_;
    int active_peer_port_ = 0;
    std::chrono::steady_clock::time_point last_activity_{};
    MsgHandler on_msg_;
    std::atomic<bool> stop_{false};
};
