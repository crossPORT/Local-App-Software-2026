#pragma once

#include <cstdint>
#include <string>

/** Fixed fabric LAN map: port N ↔ 10.64.0.N ↔ sys-port-N */
namespace rocketbox_lan {

constexpr const char* kSubnetPrefix = "10.64.0";
constexpr int kIdleDisconnectSec = 30;

inline std::string ip_for_port(int port) {
  return std::string(kSubnetPrefix) + "." + std::to_string(port);
}

inline std::string system_id_for_port(int port) {
  return "sys-port-" + std::to_string(port);
}

/** Returns fabric port 1..4 if bytes are 10.64.0.K, else 0. */
inline int port_from_ipv4_bytes(const uint8_t* b) {
  if (!b || b[0] != 10 || b[1] != 64 || b[2] != 0 || b[3] < 1 || b[3] > 4) {
    return 0;
  }
  return static_cast<int>(b[3]);
}

/** Dest port from raw IPv4 packet (IFF_NO_PI). Returns 0 if not fabric LAN. */
inline int dest_port_from_ip_packet(const uint8_t* pkt, size_t len) {
  if (!pkt || len < 20) {
    return 0;
  }
  if ((pkt[0] >> 4) != 4) {
    return 0;
  }
  return port_from_ipv4_bytes(pkt + 16);
}

/** Source fabric port from raw IPv4 packet. Returns 0 if not fabric LAN. */
inline int src_port_from_ip_packet(const uint8_t* pkt, size_t len) {
  if (!pkt || len < 20) {
    return 0;
  }
  if ((pkt[0] >> 4) != 4) {
    return 0;
  }
  return port_from_ipv4_bytes(pkt + 12);
}

}  // namespace rocketbox_lan
