#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/** Shared tunnel TUN / coalesce constants. */
constexpr std::size_t kTunMtu = 16384;
/** Flush coalesced USB frames sooner — large OUTs + WinUSB listen-pause stall easily. */
constexpr std::size_t kBatchFlushBytes = 16 * 1024;
constexpr int kBatchFlushMs = 2;
constexpr std::size_t kBatchMaxBytes = 256 * 1024;

void pkt_write_u32_be(uint8_t* buf, uint32_t val);
uint32_t pkt_read_u32_be(const uint8_t* buf);

/** Pack IP packets: [u32_be len][ip]... (no outer envelope). */
std::vector<uint8_t> pack_ip_batch(const std::vector<std::vector<uint8_t>>& pkts);

/** Unpack batch body into IP packets. Returns false if malformed. */
bool unpack_ip_batch(const uint8_t* data, std::size_t len,
                     std::vector<std::vector<uint8_t>>* out);
