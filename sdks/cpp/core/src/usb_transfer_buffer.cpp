#include "usb_transfer.h"
#include "usb_transfer_handle.h"
#include "usb_device_open.h"
#include "usb_diag.h"
#include "usb_frame.h"
#include "usb_protocol.h"

#include <libusb-1.0/libusb.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace {

bool bulk_write(libusb_device_handle* h, const uint8_t* data, size_t len, int timeout_ms) {
  size_t off = 0;
  while (off < len) {
    int xfer = 0;
    const size_t n = std::min(len - off, usb_protocol::kChunkSize);
    USB_DIAG("[USB-DIAG] buffer_write EP=0x%02x len=%zu timeout=%dms\n",
             usb_protocol::kEndpointDataOut, n, timeout_ms);
    const int rc = libusb_bulk_transfer(h, usb_protocol::kEndpointDataOut,
                                        const_cast<uint8_t*>(data + off), static_cast<int>(n), &xfer,
                                        timeout_ms);
    USB_DIAG("[USB-DIAG] buffer_write rc=%d (%s) xfer=%d\n", rc,
             libusb_strerror(static_cast<libusb_error>(rc)), xfer);
    if (rc != LIBUSB_SUCCESS) return false;
    off += static_cast<size_t>(xfer);
  }
  return true;
}

bool bulk_read(libusb_device_handle* h, uint8_t* data, size_t len, int timeout_ms) {
  size_t off = 0;
  while (off < len) {
    int xfer = 0;
    const size_t n = std::min(len - off, usb_protocol::kChunkSize);
    const int rc =
        libusb_bulk_transfer(h, usb_protocol::kEndpointDataIn, data + off, static_cast<int>(n),
                             &xfer, timeout_ms);
    if (rc != LIBUSB_SUCCESS) {
      USB_DIAG("[USB-DIAG] buffer_read EP=0x%02x want=%zu timeout=%dms rc=%d (%s)\n",
               usb_protocol::kEndpointDataIn, n, timeout_ms, rc,
               libusb_strerror(static_cast<libusb_error>(rc)));
      return false;
    }
    off += static_cast<size_t>(xfer);
  }
  return true;
}

bool discard_bytes(libusb_device_handle* h, uint64_t nbytes, int timeout_ms) {
  std::vector<uint8_t> scratch(std::min<size_t>(nbytes, usb_protocol::kChunkSize));
  uint64_t left = nbytes;
  while (left > 0) {
    const size_t n = std::min<uint64_t>(left, scratch.size());
    if (!bulk_read(h, scratch.data(), n, timeout_ms)) return false;
    left -= n;
  }
  return true;
}

}  // namespace

TransferResult send_buffer_on_handle(libusb_device_handle* handle, const uint8_t* data, size_t len,
                                     unsigned timeout_ms, uint8_t frame_kind, const char* filename) {
  TransferResult result{};
  result.expected_bytes = len;
  if (!handle) {
    result.error_message = "null handle";
    return result;
  }
  if (!data && len > 0) {
    result.error_message = "null buffer";
    return result;
  }
  RocketBxHeader hdr{};
  fill_rocketbx_header(&hdr, len, frame_kind, filename);
  const auto t0 = std::chrono::steady_clock::now();
  if (!bulk_write(handle, reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr),
                  static_cast<int>(timeout_ms))) {
    result.error_message = "Header send failed";
    return result;
  }
  if (len > 0 && !bulk_write(handle, data, len, static_cast<int>(timeout_ms))) {
    result.error_message = "Payload send failed";
    return result;
  }
  const double sec =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  result.ok = true;
  result.bytes_transferred = len;
  result.seconds = sec;
  result.mbps = sec > 0 ? (len / (1024.0 * 1024.0)) / sec : 0;
  return result;
}

TransferResult receive_buffer_on_handle(libusb_device_handle* handle, std::vector<uint8_t>* out,
                                        unsigned header_timeout_ms, uint8_t expected_frame_kind) {
  TransferResult result{};
  if (!handle || !out) {
    result.error_message = "null handle/out";
    return result;
  }
  out->clear();
  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + std::chrono::milliseconds(header_timeout_ms);
  RocketBxHeader hdr{};
  bool got = false;
  while (Clock::now() < deadline) {
    const int rem = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
    if (rem <= 0) break;
    if (!bulk_read(handle, reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr), rem)) break;
    if (std::memcmp(hdr.magic, usb_protocol::kHeaderMagic, 8) != 0) {
      USB_DIAG("[USB-DIAG] buffer_recv bad magic; clear_halt IN\n");
      (void)libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
      continue;
    }
    if (expected_frame_kind == usb_protocol::kFrameKindPayload &&
        hdr.frame_kind == usb_protocol::kFrameKindSession) {
      const int r2 = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
      if (!discard_bytes(handle, hdr.file_size, std::max(r2, 1))) {
        result.error_message = "Stray session frame read failed";
        return result;
      }
      continue;
    }
    if (expected_frame_kind != 0 && hdr.frame_kind != expected_frame_kind) {
      result.error_message = "Unexpected frame kind in header";
      return result;
    }
    got = true;
    break;
  }
  if (!got) {
    result.error_message = "Header read failed";
    return result;
  }
  result.expected_bytes = hdr.file_size;
  out->resize(static_cast<size_t>(hdr.file_size));
  if (hdr.file_size > 0 &&
      !bulk_read(handle, out->data(), out->size(), static_cast<int>(payload_timeout_ms()))) {
    result.error_message = "Payload read failed";
    out->clear();
    return result;
  }
  USB_DIAG("[USB-DIAG] buffer_recv ok kind=%u bytes=%llu\n",
           static_cast<unsigned>(hdr.frame_kind),
           static_cast<unsigned long long>(hdr.file_size));
  result.ok = true;
  result.bytes_transferred = hdr.file_size;
  return result;
}

TransferResult send_buffer_core(libusb_context* ctx, const uint8_t* data, size_t len, int port_index,
                                unsigned timeout_ms, uint8_t frame_kind, const char* filename,
                                bool reset_data_endpoints) {
  TransferResult result{};
  libusb_device_handle* handle =
      open_device_by_index(ctx, port_index, &result.error_message, 5, reset_data_endpoints);
  if (!handle) return result;
  result = send_buffer_on_handle(handle, data, len, timeout_ms, frame_kind, filename);
  close_device(handle);
  return result;
}

TransferResult receive_buffer_core(libusb_context* ctx, std::vector<uint8_t>* out, int port_index,
                                   unsigned header_timeout_ms, uint8_t expected_frame_kind,
                                   bool reset_data_endpoints) {
  TransferResult result{};
  if (!out) {
    result.error_message = "null out";
    return result;
  }
  libusb_device_handle* handle =
      open_device_by_index(ctx, port_index, &result.error_message, 5, reset_data_endpoints);
  if (!handle) return result;
  result = receive_buffer_on_handle(handle, out, header_timeout_ms, expected_frame_kind);
  close_device(handle);
  return result;
}
