#include "dialer.hpp"
#include "peer_map.hpp"

#include <cstring>
#include <iostream>

CircuitDialer::CircuitDialer(rocketbox::RocketBoxTransport& transport, int local_port)
    : transport_(transport), local_port_(local_port) {
    last_activity_ = std::chrono::steady_clock::now();
    transport_.on_data_message([this](const std::vector<uint8_t>& body) {
        if (stop_ || body.size() < 4) {
            return;
        }
        const uint32_t len = read_u32_be(body.data());
        if (body.size() < 4u + len) {
            return;
        }
        std::vector<uint8_t> msg(body.begin() + 4, body.begin() + 4 + static_cast<std::ptrdiff_t>(len));
        MsgHandler h;
        {
            std::lock_guard<std::mutex> lock(mu_);
            h = on_msg_;
            last_activity_ = std::chrono::steady_clock::now();
        }
        if (h) {
            h(msg);
        }
    });
    transport_.ensure_listening();
}

CircuitDialer::~CircuitDialer() { shutdown(); }

void CircuitDialer::write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xff);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xff);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xff);
    buf[3] = static_cast<uint8_t>(val & 0xff);
}

uint32_t CircuitDialer::read_u32_be(const uint8_t* buf) const {
    return (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}

void CircuitDialer::shutdown() {
    stop_ = true;
    try {
        transport_.clear_circuit();
    } catch (...) {
    }
    std::lock_guard<std::mutex> lock(mu_);
    active_peer_port_ = 0;
}

bool CircuitDialer::ensure(int dest_port) {
    if (dest_port < 1 || dest_port > 4 || dest_port == local_port_) {
        return false;
    }
    // Must match EP4 switch — active_peer alone can lie after inbound-only traffic.
    if (transport_.switch_dest() == dest_port) {
        std::lock_guard<std::mutex> lock(mu_);
        active_peer_port_ = dest_port;
        last_activity_ = std::chrono::steady_clock::now();
        return true;
    }
    try {
        transport_.ensure_circuit(rocketbox_lan::system_id_for_port(dest_port));
    } catch (const std::exception& e) {
        std::cerr << "[rocketbox-tunnel] ensure_circuit failed: " << e.what() << std::endl;
        return false;
    }
    std::lock_guard<std::mutex> lock(mu_);
    active_peer_port_ = dest_port;
    last_activity_ = std::chrono::steady_clock::now();
    return true;
}

bool CircuitDialer::deliver(int dest_port, const std::vector<uint8_t>& msg) {
    if (dest_port < 1 || dest_port > 4 || dest_port == local_port_) {
        return false;
    }
    std::vector<uint8_t> framed(4 + msg.size());
    write_u32_be(framed.data(), static_cast<uint32_t>(msg.size()));
    if (!msg.empty()) {
        std::memcpy(framed.data() + 4, msg.data(), msg.size());
    }
    // Sticky EP4; retry once after cache invalidate if OUT fails (listen race / stale aim).
    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            if (attempt > 0 || transport_.switch_dest() != dest_port) {
                transport_.ensure_circuit(rocketbox_lan::system_id_for_port(dest_port));
            }
            {
                std::lock_guard<std::mutex> lock(mu_);
                active_peer_port_ = dest_port;
                last_activity_ = std::chrono::steady_clock::now();
            }
            transport_.send_bytes(framed);
            note_activity();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[rocketbox-tunnel] deliver failed dest=" << dest_port
                      << " attempt=" << (attempt + 1) << ": " << e.what() << std::endl;
            try {
                transport_.clear_circuit();
            } catch (...) {
            }
            {
                std::lock_guard<std::mutex> lock(mu_);
                active_peer_port_ = 0;
            }
        }
    }
    return false;
}

void CircuitDialer::send_message(const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> framed(4 + msg.size());
    write_u32_be(framed.data(), static_cast<uint32_t>(msg.size()));
    if (!msg.empty()) {
        std::memcpy(framed.data() + 4, msg.data(), msg.size());
    }
    try {
        transport_.send_bytes(framed);
    } catch (const std::exception& e) {
        std::cerr << "[rocketbox-tunnel] send_message failed: " << e.what() << std::endl;
        throw;
    }
    note_activity();
}

bool CircuitDialer::exchange_message(const std::vector<uint8_t>& msg, std::vector<uint8_t>* reply,
                                     unsigned timeout_ms) {
    if (!reply) return false;
    std::vector<uint8_t> framed(4 + msg.size());
    write_u32_be(framed.data(), static_cast<uint32_t>(msg.size()));
    if (!msg.empty()) {
        std::memcpy(framed.data() + 4, msg.data(), msg.size());
    }
    std::vector<uint8_t> raw;
    if (!transport_.exchange_bytes(framed, &raw, timeout_ms)) {
        return false;
    }
    if (raw.size() < 4) return false;
    const uint32_t len = read_u32_be(raw.data());
    if (raw.size() < 4u + len) return false;
    reply->assign(raw.begin() + 4, raw.begin() + 4 + static_cast<std::ptrdiff_t>(len));
    note_activity();
    return true;
}

void CircuitDialer::on_message(MsgHandler handler) {
    std::lock_guard<std::mutex> lock(mu_);
    on_msg_ = std::move(handler);
}

void CircuitDialer::note_activity() {
    std::lock_guard<std::mutex> lock(mu_);
    last_activity_ = std::chrono::steady_clock::now();
}

void CircuitDialer::note_inbound_peer(int peer_port) {
    // Do not set active_peer_port_ here — that skipped EP4 switch on the reply
    // path (Windows→Linux echo replies never left the Linux port).
    if (peer_port < 1 || peer_port > 4 || peer_port == local_port_) return;
    note_activity();
}

void CircuitDialer::tick_idle() {
    std::lock_guard<std::mutex> lock(mu_);
    if (active_peer_port_ == 0 || stop_) {
        return;
    }
    const auto idle = std::chrono::steady_clock::now() - last_activity_;
    if (idle < std::chrono::seconds(rocketbox_lan::kIdleDisconnectSec)) {
        return;
    }
    // Keep EP4 aimed at the last peer. Clearing forced a cold path where
    // Windows→Linux failed until Linux originated traffic (neighbor/circuit warm).
    std::cerr << "[rocketbox-tunnel] idle (keeping EP4 peer port " << active_peer_port_ << ")"
              << std::endl;
}

bool CircuitDialer::circuit_open() const {
    return transport_.switch_dest() != 0;
}
