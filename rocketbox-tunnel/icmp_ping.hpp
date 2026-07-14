#pragma once

#include <cstdint>
#include <vector>

namespace rocketbox_icmp {

/** Build IPv4 ICMP echo request: src/dst as 10.64.0.{ports}, id/seq set. */
std::vector<uint8_t> echo_request(int src_port, int dst_port, uint16_t id, uint16_t seq);

/** True if packet is ICMP echo reply to our id (any seq). */
bool is_echo_reply(const uint8_t* pkt, size_t len, uint16_t id);

/** True if IPv4 ICMP echo request (type 8) destined to 10.64.0.local_port. */
bool is_echo_request(const uint8_t* pkt, size_t len, int local_port);

/** Echo reply for a valid echo request (swap addrs, type 0, refresh checksums). Empty on failure. */
std::vector<uint8_t> make_echo_reply(const uint8_t* request, size_t len);

}  // namespace rocketbox_icmp
