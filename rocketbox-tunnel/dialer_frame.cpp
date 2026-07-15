#include "dialer_frame.hpp"

#include "pkt_batch.hpp"

#include <cstring>

std::vector<uint8_t> frame_batch_body(const std::vector<uint8_t>& batch_body) {
  std::vector<uint8_t> framed(4 + batch_body.size());
  pkt_write_u32_be(framed.data(), static_cast<uint32_t>(batch_body.size()));
  if (!batch_body.empty()) {
    std::memcpy(framed.data() + 4, batch_body.data(), batch_body.size());
  }
  return framed;
}
