#include "sim_plane.hpp"

#include <cstring>
#include <stdexcept>
#include <unistd.h>

namespace rocketbox {
namespace detail {

namespace {

bool read_all(int fd, uint8_t* buf, size_t len, const std::atomic<bool>& stop) {
    size_t total = 0;
    while (total < len && !stop) {
        ssize_t n = ::read(fd, buf + total, len - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return total == len;
}

}  // namespace

void SimPlane::write_raw(const std::vector<uint8_t>& packet) {
    if (fd_ < 0) {
        throw std::runtime_error("sim disconnected");
    }
    if (::write(fd_, packet.data(), packet.size()) <= 0) {
        throw std::runtime_error("sim write failed");
    }
}

void SimPlane::write_ep4_switch(int dest_port) {
    std::vector<uint8_t> packet(1 + usb_protocol::kSwitchPacketSize, 0);
    packet[0] = 0x04;
    packet[1] = static_cast<uint8_t>(dest_port & 0x0F);
    write_raw(packet);
}

void SimPlane::write_ep1(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet(5 + data.size());
    packet[0] = 0x01;
    write_u32_be(packet.data() + 1, static_cast<uint32_t>(data.size()));
    if (!data.empty()) {
        std::memcpy(packet.data() + 5, data.data(), data.size());
    }
    write_raw(packet);
}

void SimPlane::push_inbound(std::vector<uint8_t> data) {
    std::function<void(const std::vector<uint8_t>&)> cb;
    std::vector<uint8_t> body;
    {
        std::lock_guard<std::mutex> lock(inbound_mu_);
        if (data.size() >= usb_protocol::kHeaderSize && on_msg_) {
            auto h = parse_rocketbx_header(data);
            if (h.frame_kind == usb_protocol::kFrameKindPayload) {
                body.assign(data.begin() + static_cast<std::ptrdiff_t>(usb_protocol::kHeaderSize),
                            data.end());
                cb = on_msg_;
            }
        }
        inbound_.push_back(std::move(data));
    }
    if (cb && !body.empty()) {
        cb(body);
    }
}

void SimPlane::listen_loop() {
    while (!stop_ && fd_ >= 0) {
        uint8_t ep = 0;
        if (::read(fd_, &ep, 1) <= 0) {
            break;
        }
        if (ep == 0x02) {
            uint8_t len_buf[4];
            if (!read_all(fd_, len_buf, 4, stop_)) {
                break;
            }
            uint32_t len = read_u32_be(len_buf);
            std::vector<uint8_t> payload(len);
            if (len > 0 && !read_all(fd_, payload.data(), len, stop_)) {
                break;
            }
            push_inbound(std::move(payload));
        } else if (ep == 0x03) {
            uint8_t hdr[12];
            if (!read_all(fd_, hdr, 12, stop_)) {
                break;
            }
            uint32_t plen = read_u32_be(hdr + 8);
            std::vector<uint8_t> payload(plen);
            if (plen > 0 && !read_all(fd_, payload.data(), plen, stop_)) {
                break;
            }
        } else {
            break;
        }
    }
    connected_ = false;
}

}  // namespace detail
}  // namespace rocketbox
