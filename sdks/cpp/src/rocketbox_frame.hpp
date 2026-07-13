#pragma once

#include "rocketbox/sdk.h"
#include "usb_protocol.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace rocketbox {
namespace detail {

inline void write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xff);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xff);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xff);
    buf[3] = static_cast<uint8_t>(val & 0xff);
}

inline uint32_t read_u32_be(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}

inline std::vector<uint8_t> build_rocketbx_header(uint64_t file_size, uint8_t frame_kind,
                                                  const std::string& filename = {}) {
    std::vector<uint8_t> hdr(usb_protocol::kHeaderSize, 0);
    const char* magic = "ROCKETBX";
    std::memcpy(hdr.data(), magic, 8);
    std::memcpy(hdr.data() + 8, &file_size, sizeof(file_size));
    hdr[usb_protocol::kFrameKindOffset] = frame_kind;
    const size_t n = std::min(filename.size(), static_cast<size_t>(15));
    if (n > 0) {
        std::memcpy(hdr.data() + 17, filename.data(), n);
    }
    return hdr;
}

inline ParsedHeader parse_rocketbx_header(const std::vector<uint8_t>& buf) {
    if (buf.size() < usb_protocol::kHeaderSize) {
        throw std::runtime_error("ROCKETBX header too short");
    }
    if (std::memcmp(buf.data(), "ROCKETBX", 8) != 0) {
        throw std::runtime_error("bad ROCKETBX magic");
    }
    ParsedHeader h;
    std::memcpy(&h.file_size, buf.data() + 8, sizeof(h.file_size));
    h.frame_kind = buf[usb_protocol::kFrameKindOffset];
    for (size_t i = 17; i < usb_protocol::kHeaderSize && buf[i]; ++i) {
        h.filename.push_back(static_cast<char>(buf[i]));
    }
    return h;
}

inline int peer_port_from_system_id(const std::string& id) {
    const std::string prefix = "sys-port-";
    if (id.rfind(prefix, 0) != 0) {
        return 0;
    }
    try {
        return std::stoi(id.substr(prefix.size()));
    } catch (...) {
        return 0;
    }
}

inline std::string system_id_for_display_port(int display_port) {
    return "sys-port-" + std::to_string(display_port);
}

}  // namespace detail
}  // namespace rocketbox
