#include "icmp_ping.hpp"

#include <cstring>

namespace rocketbox_icmp {
namespace {

uint16_t checksum(const uint8_t* data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i + 1 < len; i += 2) {
    sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
  }
  if (len & 1) {
    sum += static_cast<uint16_t>(data[len - 1] << 8);
  }
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  return static_cast<uint16_t>(~sum);
}

}  // namespace

std::vector<uint8_t> echo_request(int src_port, int dst_port, uint16_t id, uint16_t seq) {
  std::vector<uint8_t> pkt(20 + 8 + 32, 0);  // IP + ICMP hdr + payload
  // IPv4 header
  pkt[0] = 0x45;
  pkt[1] = 0;
  const uint16_t total = static_cast<uint16_t>(pkt.size());
  pkt[2] = static_cast<uint8_t>(total >> 8);
  pkt[3] = static_cast<uint8_t>(total & 0xff);
  pkt[8] = 64;   // TTL
  pkt[9] = 1;    // ICMP
  pkt[12] = 10;
  pkt[13] = 64;
  pkt[14] = 0;
  pkt[15] = static_cast<uint8_t>(src_port);
  pkt[16] = 10;
  pkt[17] = 64;
  pkt[18] = 0;
  pkt[19] = static_cast<uint8_t>(dst_port);
  const uint16_t ip_csum = checksum(pkt.data(), 20);
  pkt[10] = static_cast<uint8_t>(ip_csum >> 8);
  pkt[11] = static_cast<uint8_t>(ip_csum & 0xff);

  // ICMP echo request
  uint8_t* icmp = pkt.data() + 20;
  icmp[0] = 8;  // type echo request
  icmp[1] = 0;
  icmp[4] = static_cast<uint8_t>(id >> 8);
  icmp[5] = static_cast<uint8_t>(id & 0xff);
  icmp[6] = static_cast<uint8_t>(seq >> 8);
  icmp[7] = static_cast<uint8_t>(seq & 0xff);
  for (int i = 0; i < 32; ++i) {
    icmp[8 + i] = static_cast<uint8_t>('a' + (i % 26));
  }
  const uint16_t icmp_csum = checksum(icmp, 8 + 32);
  icmp[2] = static_cast<uint8_t>(icmp_csum >> 8);
  icmp[3] = static_cast<uint8_t>(icmp_csum & 0xff);
  return pkt;
}

bool is_echo_reply(const uint8_t* pkt, size_t len, uint16_t id) {
  if (!pkt || len < 28) {
    return false;
  }
  if ((pkt[0] >> 4) != 4 || pkt[9] != 1) {
    return false;
  }
  const size_t ihl = (pkt[0] & 0x0f) * 4;
  if (len < ihl + 8) {
    return false;
  }
  const uint8_t* icmp = pkt + ihl;
  if (icmp[0] != 0) {  // echo reply
    return false;
  }
  const uint16_t rid = static_cast<uint16_t>((icmp[4] << 8) | icmp[5]);
  return rid == id;
}

bool is_echo_request(const uint8_t* pkt, size_t len, int local_port) {
  if (!pkt || local_port < 1 || local_port > 4 || len < 28) {
    return false;
  }
  if ((pkt[0] >> 4) != 4 || pkt[9] != 1) {
    return false;
  }
  const size_t ihl = static_cast<size_t>(pkt[0] & 0x0f) * 4;
  if (ihl < 20 || len < ihl + 8) {
    return false;
  }
  if (pkt[16] != 10 || pkt[17] != 64 || pkt[18] != 0 || pkt[19] != static_cast<uint8_t>(local_port)) {
    return false;
  }
  return pkt[ihl] == 8;  // ICMP echo request
}

std::vector<uint8_t> make_echo_reply(const uint8_t* request, size_t len) {
  if (!request || len < 28 || (request[0] >> 4) != 4 || request[9] != 1) {
    return {};
  }
  const size_t ihl = static_cast<size_t>(request[0] & 0x0f) * 4;
  if (ihl < 20 || len < ihl + 8 || request[ihl] != 8) {
    return {};
  }
  std::vector<uint8_t> out(request, request + len);
  // Swap IPv4 src/dst.
  for (int i = 0; i < 4; ++i) {
    const uint8_t t = out[12 + i];
    out[12 + i] = out[16 + i];
    out[16 + i] = t;
  }
  out[8] = 64;  // TTL
  out[10] = 0;
  out[11] = 0;
  const uint16_t ip_csum = checksum(out.data(), ihl);
  out[10] = static_cast<uint8_t>(ip_csum >> 8);
  out[11] = static_cast<uint8_t>(ip_csum & 0xff);

  uint8_t* icmp = out.data() + ihl;
  icmp[0] = 0;  // echo reply
  icmp[2] = 0;
  icmp[3] = 0;
  const uint16_t icmp_csum = checksum(icmp, len - ihl);
  icmp[2] = static_cast<uint8_t>(icmp_csum >> 8);
  icmp[3] = static_cast<uint8_t>(icmp_csum & 0xff);
  return out;
}

}  // namespace rocketbox_icmp
