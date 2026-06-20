#include "test_util.h"

#include "usb_protocol.h"
#include "usb_transfer.h"

namespace {

unsigned chunk_mb() {
    return static_cast<unsigned>(usb_protocol::kChunkSize / (1024 * 1024));
}

}  // namespace

FABRIC_TEST(payload_timeout_default_and_override) {
    set_payload_timeout_ms(0);  // restore default
    CHECK_EQ(payload_timeout_ms(), usb_protocol::kFileTimeoutMs);

    set_payload_timeout_ms(5000);
    CHECK_EQ(payload_timeout_ms(), 5000u);

    set_payload_timeout_ms(0);  // 0 resets to compiled default
    CHECK_EQ(payload_timeout_ms(), usb_protocol::kFileTimeoutMs);
}

FABRIC_TEST(usbfs_limit_is_positive) {
    // Reads /sys; should always yield a sane non-zero value (>=16 fallback).
    CHECK(usbfs_limit_mb() >= 1u);
}

FABRIC_TEST(inflight_depth_from_explicit_budget) {
    const unsigned cmb = chunk_mb();
    CHECK(cmb >= 1u);

    set_inflight_budget_mb(cmb);  // exactly one chunk
    CHECK_EQ(inflight_queue_depth(), 1u);

    set_inflight_budget_mb(cmb * 2);  // two chunks
    CHECK_EQ(inflight_queue_depth(), 2u);

    set_inflight_budget_mb(cmb * 5);  // five chunks
    CHECK_EQ(inflight_queue_depth(), 5u);

    set_inflight_budget_mb(0);  // restore auto
}

FABRIC_TEST(inflight_depth_clamped_to_queue_max) {
    // A huge budget is capped at the engine's max queue depth.
    set_inflight_budget_mb(chunk_mb() * (usb_protocol::kQueueDepth + 100));
    CHECK_EQ(inflight_queue_depth(),
             static_cast<unsigned>(usb_protocol::kQueueDepth));
    set_inflight_budget_mb(0);
}

FABRIC_TEST(inflight_depth_floors_to_one) {
    // A budget smaller than a single chunk still allows one in-flight transfer.
    set_inflight_budget_mb(1);  // 1 MB < 4 MB chunk
    CHECK_EQ(inflight_queue_depth(), 1u);
    set_inflight_budget_mb(0);
}

FABRIC_TEST(inflight_depth_auto_is_sane) {
    set_inflight_budget_mb(0);  // auto-detect from usbfs limit
    const unsigned depth = inflight_queue_depth();
    CHECK(depth >= 1u);
    CHECK(depth <= static_cast<unsigned>(usb_protocol::kQueueDepth));
}
