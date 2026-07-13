#pragma once

#include "dialer.hpp"
#include "tun_device.hpp"

#include <atomic>
#include <mutex>

/** Bridges TUN ↔ dial-on-demand RocketBox circuits (message-framed IP). */
class TunnelBridge {
public:
    TunnelBridge(TunDevice& tun, CircuitDialer& dialer, int local_port);
    ~TunnelBridge();

    TunnelBridge(const TunnelBridge&) = delete;
    TunnelBridge& operator=(const TunnelBridge&) = delete;

    void run();
    void stop();

private:
    void on_tunnel_message(const std::vector<uint8_t>& msg);

    TunDevice& tun_;
    CircuitDialer& dialer_;
    int local_port_;
    std::mutex write_mu_;
    std::atomic<bool> stop_{false};
};
