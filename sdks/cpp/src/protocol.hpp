#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace rocketbox {
namespace detail {

constexpr uint8_t MSG_ATTACH = 0x01;
constexpr uint8_t MSG_LIST = 0x02;
constexpr uint8_t MSG_CONNECT = 0x03;
constexpr uint8_t MSG_DISCONNECT = 0x04;

constexpr uint8_t MSG_SYSTEMS = 0x82;
constexpr uint8_t MSG_ACK = 0x83;
constexpr uint8_t MSG_NAK = 0x84;
constexpr uint8_t MSG_CIRCUIT_UP = 0x85;
constexpr uint8_t MSG_CIRCUIT_DOWN = 0x86;

constexpr uint16_t USB_VID = 0x1772;
constexpr uint16_t USB_PID = 0x0006;
constexpr int USB_INTERFACE = 0;
constexpr unsigned USB_TIMEOUT_MS = 5000;
constexpr int USB_READ_SIZE = 16384;

// Default bulk addresses: EP1 OUT, EP2 IN, EP3 IN, EP4 OUT
constexpr uint8_t USB_EP1_OUT = 0x01;
constexpr uint8_t USB_EP2_IN = 0x82;
constexpr uint8_t USB_EP3_IN = 0x83;
constexpr uint8_t USB_EP4_OUT = 0x04;

struct ControlHeader {
    uint8_t ver = 0;
    uint8_t type = 0;
    uint16_t txn = 0;
    uint32_t arg = 0;
    uint32_t len = 0;
};

inline uint16_t read_u16_be(const uint8_t* buf) {
    return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
}

inline uint32_t read_u32_be(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) |
           buf[3];
}

inline void write_u16_be(uint8_t* buf, uint16_t val) {
    buf[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(val & 0xFF);
}

inline void write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(val & 0xFF);
}

inline ControlHeader parse_control_header(const uint8_t* buf) {
    ControlHeader h;
    h.ver = buf[0];
    h.type = buf[1];
    h.txn = read_u16_be(buf + 2);
    h.arg = read_u32_be(buf + 4);
    h.len = read_u32_be(buf + 8);
    return h;
}

inline std::string nak_reason(uint32_t arg) {
    if (arg == 0x02) return "offline";
    if (arg == 0x03) return "denied";
    if (arg == 0x04) return "invalid";
    if (arg == 0x05) return "timeout";
    return "busy";
}

}  // namespace detail
}  // namespace rocketbox
