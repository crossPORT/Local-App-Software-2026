#include "usb_transfer_handle.h"

#include "usb_buffer_bulk.h"
#include "usb_diag.h"
#include "usb_frame.h"
#include "usb_protocol.h"
#include "usb_transfer.h"

#include <libusb-1.0/libusb.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace {

bool magic_ok(const RocketBxHeader& hdr) {
  return std::memcmp(hdr.magic, usb_protocol::kHeaderMagic, 8) == 0;
}

/** Fast path: one 32-byte read. On bad magic, slide 1 byte at a time until sync or deadline. */
bool read_header_synced(libusb_device_handle* handle, RocketBxHeader* hdr,
                        std::chrono::steady_clock::time_point deadline) {
  using Clock = std::chrono::steady_clock;
  auto rem_ms = [&]() {
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
  };

  size_t got = 0;
  const int first = rem_ms();
  if (first <= 0) return false;
  if (!usb_bulk_read(handle, reinterpret_cast<uint8_t*>(hdr), sizeof(*hdr), first, &got)) {
    return false;
  }
  if (magic_ok(*hdr)) return true;

  USB_DIAG("[USB-DIAG] buffer_recv bad magic; sliding resync\n");
  uint8_t win[usb_protocol::kHeaderSize];
  std::memcpy(win, hdr, sizeof(win));
  size_t have = sizeof(win);
  while (Clock::now() < deadline) {
    std::memmove(win, win + 1, have - 1);
    --have;
    uint8_t b = 0;
    got = 0;
    const int rem = rem_ms();
    if (rem <= 0) break;
    if (!usb_bulk_read(handle, &b, 1, rem, &got) || got != 1) break;
    win[have++] = b;
    if (have < sizeof(win)) continue;
    std::memcpy(hdr, win, sizeof(*hdr));
    if (magic_ok(*hdr)) return true;
  }
  (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
  return false;
}

bool drain_payload_remainder(libusb_device_handle* handle, uint64_t left, int timeout_ms) {
  if (left == 0) return true;
  if (usb_bulk_discard(handle, left, timeout_ms)) return true;
  USB_DIAG("[USB-DIAG] buffer_recv drain failed; clear_halt IN\n");
  (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
  return false;
}

}  // namespace

TransferResult receive_buffer_on_handle(libusb_device_handle* handle, std::vector<uint8_t>* out,
                                        unsigned header_timeout_ms, uint8_t expected_frame_kind,
                                        unsigned payload_timeout_ms_arg) {
  TransferResult result{};
  if (!handle || !out) {
    result.error_message = "null handle/out";
    return result;
  }
  out->clear();
  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + std::chrono::milliseconds(header_timeout_ms);
  const unsigned pay_ms =
      payload_timeout_ms_arg != 0 ? payload_timeout_ms_arg : payload_timeout_ms();

  RocketBxHeader hdr{};
  for (;;) {
    if (!read_header_synced(handle, &hdr, deadline)) {
      result.error_message = "Header read failed";
      return result;
    }
    if (expected_frame_kind == usb_protocol::kFrameKindPayload &&
        hdr.frame_kind == usb_protocol::kFrameKindSession) {
      if (hdr.file_size > usb_protocol::kMaxBufferPayloadBytes) {
        USB_DIAG("[USB-DIAG] buffer_recv absurd session size=%llu; clear_halt\n",
                 static_cast<unsigned long long>(hdr.file_size));
        (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
        continue;
      }
      const int r2 = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
      if (!drain_payload_remainder(handle, hdr.file_size, std::max(r2, 1))) {
        result.error_message = "Stray session frame read failed";
        return result;
      }
      continue;
    }
    if (expected_frame_kind != 0 && hdr.frame_kind != expected_frame_kind) {
      if (hdr.file_size > usb_protocol::kMaxBufferPayloadBytes) {
        (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
      } else {
        (void)drain_payload_remainder(handle, hdr.file_size, static_cast<int>(pay_ms));
      }
      result.error_message = "Unexpected frame kind in header";
      return result;
    }
    break;
  }

  if (hdr.file_size > usb_protocol::kMaxBufferPayloadBytes) {
    USB_DIAG("[USB-DIAG] buffer_recv absurd size=%llu; clear_halt\n",
             static_cast<unsigned long long>(hdr.file_size));
    (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
    result.error_message = "Payload size exceeds limit";
    return result;
  }

  result.expected_bytes = hdr.file_size;
  out->resize(static_cast<size_t>(hdr.file_size));
  if (hdr.file_size > 0) {
    size_t got = 0;
    if (!usb_bulk_read(handle, out->data(), out->size(), static_cast<int>(pay_ms), &got)) {
      (void)drain_payload_remainder(handle, hdr.file_size - got, static_cast<int>(pay_ms));
      out->clear();
      result.error_message = "Payload read failed";
      return result;
    }
  }
  USB_DIAG("[USB-DIAG] buffer_recv ok kind=%u bytes=%llu\n",
           static_cast<unsigned>(hdr.frame_kind),
           static_cast<unsigned long long>(hdr.file_size));
  result.ok = true;
  result.bytes_transferred = hdr.file_size;
  return result;
}
