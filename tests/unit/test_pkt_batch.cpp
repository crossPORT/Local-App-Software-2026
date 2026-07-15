#include "test_util.h"

#include "pkt_batch.hpp"

#include <vector>

RB_TEST(pkt_batch_round_trip_single) {
  const std::vector<uint8_t> ip{0x45, 0x00, 0x00, 0x14, 1, 2, 3, 4, 5, 6, 7, 8,
                                9, 10, 11, 12, 13, 14, 15, 16};
  const auto body = pack_ip_batch({ip});
  std::vector<std::vector<uint8_t>> out;
  CHECK(unpack_ip_batch(body.data(), body.size(), &out));
  CHECK_EQ(out.size(), static_cast<size_t>(1));
  CHECK(out[0] == ip);
}

RB_TEST(pkt_batch_round_trip_multi) {
  const std::vector<uint8_t> a{0x45, 1, 2, 3};
  const std::vector<uint8_t> b{0x45, 4, 5, 6, 7, 8};
  const auto body = pack_ip_batch({a, b});
  std::vector<std::vector<uint8_t>> out;
  CHECK(unpack_ip_batch(body.data(), body.size(), &out));
  CHECK_EQ(out.size(), static_cast<size_t>(2));
  CHECK(out[0] == a);
  CHECK(out[1] == b);
}

RB_TEST(pkt_batch_rejects_truncated) {
  const std::vector<uint8_t> bad{0x00, 0x00, 0x00, 0x10, 0x45};
  std::vector<std::vector<uint8_t>> out;
  CHECK(!unpack_ip_batch(bad.data(), bad.size(), &out));
  CHECK(out.empty());
}
