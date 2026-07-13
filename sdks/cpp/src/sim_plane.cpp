#include "sim_plane.hpp"

#include <chrono>
#include <stdexcept>

namespace rocketbox {
namespace detail {

SimPlane::SimPlane(int display_port) : display_port_(display_port) {
    if (display_port_ < 1 || display_port_ > 4) {
        throw std::runtime_error("display port must be 1–4");
    }
}

SimPlane::~SimPlane() { disconnect(); }

void SimPlane::connect() {
    if (connected_) {
        return;
    }
    fd_ = rb_connect_loopback(1772);
    if (fd_ == RB_SOCK_INVALID) {
        throw std::runtime_error("could not connect to simulated-hardware :1772");
    }
    connected_ = true;
    stop_ = false;
    thread_ = std::thread([this] { listen_loop(); });
}

void SimPlane::disconnect() {
    stop_ = true;
    if (fd_ != RB_SOCK_INVALID) {
        rb_sock_shutdown(fd_);
        rb_sock_close(fd_);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    connected_ = false;
    switch_dest_ = 0;
}

void SimPlane::reset_connection() {
    disconnect();
    connect();
}

void SimPlane::ensure_circuit(const std::string& peer_system_id) {
    const int dest = peer_port_from_system_id(peer_system_id);
    if (dest < 1 || dest > 4) {
        throw std::runtime_error("invalid peer for ensure_circuit: " + peer_system_id);
    }
    write_ep4_switch(dest);
    switch_dest_ = dest;
}

void SimPlane::clear_circuit() {
    write_ep4_switch(0);
    switch_dest_ = 0;
}

void SimPlane::sync_systems(std::function<void(const std::vector<SystemInfo>&)> handler) {
    std::vector<SystemInfo> systems;
    for (int p = 1; p <= 4; ++p) {
        if (p == display_port_) {
            continue;
        }
        systems.push_back({system_id_for_display_port(p), "Port " + std::to_string(p), "reachable"});
    }
    if (handler) {
        handler(systems);
    }
}

void SimPlane::send_bytes(const std::vector<uint8_t>& payload, const std::string& filename) {
    auto hdr = build_rocketbx_header(payload.size(), usb_protocol::kFrameKindPayload, filename);
    std::vector<uint8_t> frame;
    frame.reserve(hdr.size() + payload.size());
    frame.insert(frame.end(), hdr.begin(), hdr.end());
    frame.insert(frame.end(), payload.begin(), payload.end());
    write_ep1(frame);
}

void SimPlane::send_session_message(const std::vector<uint8_t>& message) {
    auto hdr = build_rocketbx_header(message.size(), usb_protocol::kFrameKindSession);
    std::vector<uint8_t> frame;
    frame.insert(frame.end(), hdr.begin(), hdr.end());
    frame.insert(frame.end(), message.begin(), message.end());
    write_ep1(frame);
}

void SimPlane::send_announce_presence(const std::vector<uint8_t>& message, AnnouncePresenceMode) {
    send_session_message(message);
}

void SimPlane::on_data_message(std::function<void(const std::vector<uint8_t>&)> cb) {
    std::lock_guard<std::mutex> lock(inbound_mu_);
    on_msg_ = std::move(cb);
}

ParsedHeader SimPlane::receive_header() {
    auto full = receive_bytes();
    pending_payload_.assign(full.begin() + static_cast<std::ptrdiff_t>(usb_protocol::kHeaderSize),
                            full.end());
    pending_header_ = {full.begin(), full.begin() + static_cast<std::ptrdiff_t>(usb_protocol::kHeaderSize)};
    return parse_rocketbx_header(pending_header_);
}

std::vector<uint8_t> SimPlane::receive_payload(uint64_t file_size) {
    if (pending_payload_.size() == file_size) {
        auto out = std::move(pending_payload_);
        pending_payload_.clear();
        return out;
    }
    auto full = receive_bytes();
    if (full.size() < usb_protocol::kHeaderSize) {
        throw std::runtime_error("short ROCKETBX frame");
    }
    return {full.begin() + static_cast<std::ptrdiff_t>(usb_protocol::kHeaderSize), full.end()};
}

std::vector<uint8_t> SimPlane::receive_bytes() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(inbound_mu_);
            if (!inbound_.empty()) {
                auto out = std::move(inbound_.front());
                inbound_.erase(inbound_.begin());
                return out;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("sim receive timeout");
}

}  // namespace detail
}  // namespace rocketbox
