#pragma once

#include <cstdint>
#include <vector>

/** Outer USB envelope: u32_be(len) + batch body. */
std::vector<uint8_t> frame_batch_body(const std::vector<uint8_t>& batch_body);
