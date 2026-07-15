#include "pkt_batch.hpp"

#include <cstring>

void pkt_write_u32_be(uint8_t* buf, uint32_t val) {
  buf[0] = static_cast<uint8_t>((val >> 24) & 0xff);
  buf[1] = static_cast<uint8_t>((val >> 16) & 0xff);
  buf[2] = static_cast<uint8_t>((val >> 8) & 0xff);
  buf[3] = static_cast<uint8_t>(val & 0xff);
}

uint32_t pkt_read_u32_be(const uint8_t* buf) {
  return (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
         (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}

std::vector<uint8_t> pack_ip_batch(const std::vector<std::vector<uint8_t>>& pkts) {
  std::size_t total = 0;
  for (const auto& p : pkts) {
    total += 4 + p.size();
  }
  std::vector<uint8_t> out(total);
  std::size_t off = 0;
  for (const auto& p : pkts) {
    pkt_write_u32_be(out.data() + off, static_cast<uint32_t>(p.size()));
    off += 4;
    if (!p.empty()) {
      std::memcpy(out.data() + off, p.data(), p.size());
      off += p.size();
    }
  }
  return out;
}

bool unpack_ip_batch(const uint8_t* data, std::size_t len,
                     std::vector<std::vector<uint8_t>>* out) {
  if (!out || !data) {
    return false;
  }
  out->clear();
  std::size_t off = 0;
  while (off + 4 <= len) {
    const uint32_t plen = pkt_read_u32_be(data + off);
    off += 4;
    if (plen == 0 || off + plen > len) {
      out->clear();
      return false;
    }
    out->emplace_back(data + off, data + off + plen);
    off += plen;
  }
  if (off != len || out->empty()) {
    out->clear();
    return false;
  }
  return true;
}
