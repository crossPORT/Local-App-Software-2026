#pragma once

#include <cstdint>
#include <vector>

namespace rocketbox_icmp {

/** Build IPv4 ICMP echo request: src/dst as 10.64.0.{ports}, id/seq set. */
std::vector<uint8_t> echo_request(int src_port, int dst_port, uint16_t id, uint16_t seq);

/** True if packet is ICMP echo reply to our id (any seq). */
bool is_echo_reply(const uint8_t* pkt, size_t len, uint16_t id);

}  // namespace rocketbox_icmp
