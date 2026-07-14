#include "fabric_source.hpp"
#include "peer_map.hpp"

#include <vector>

namespace {

uint16_t ip_checksum(const uint8_t* hdr, size_t ihl_bytes) {
  uint32_t sum = 0;
  for (size_t i = 0; i + 1 < ihl_bytes; i += 2) {
    sum += static_cast<uint32_t>(hdr[i] << 8 | hdr[i + 1]);
  }
  if (ihl_bytes & 1) sum += static_cast<uint32_t>(hdr[ihl_bytes - 1] << 8);
  while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  return static_cast<uint16_t>(~sum);
}

}  // namespace

bool force_fabric_source(std::vector<uint8_t>& pkt, int local_port) {
  if (local_port < 1 || local_port > 4 || pkt.size() < 20) return false;
  if ((pkt[0] >> 4) != 4) return false;
  const int dest = rocketbox_lan::port_from_ipv4_bytes(pkt.data() + 16);
  if (dest < 1 || dest > 4 || dest == local_port) return false;
  const int src = rocketbox_lan::port_from_ipv4_bytes(pkt.data() + 12);
  if (src == local_port) return false;

  const size_t ihl = static_cast<size_t>(pkt[0] & 0x0f) * 4;
  if (ihl < 20 || pkt.size() < ihl) return false;

  pkt[12] = 10;
  pkt[13] = 64;
  pkt[14] = 0;
  pkt[15] = static_cast<uint8_t>(local_port);
  pkt[10] = 0;
  pkt[11] = 0;
  const uint16_t csum = ip_checksum(pkt.data(), ihl);
  pkt[10] = static_cast<uint8_t>(csum >> 8);
  pkt[11] = static_cast<uint8_t>(csum & 0xff);
  return true;
}
