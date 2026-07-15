#include "usb_transfer.h"
#include "usb_transfer_handle.h"
#include "usb_device_open.h"
#include "usb_buffer_bulk.h"
#include "usb_frame.h"
#include "usb_protocol.h"

#include <libusb-1.0/libusb.h>

#include <chrono>

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
  if (!usb_bulk_write(handle, reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr),
                      static_cast<int>(timeout_ms))) {
    result.error_message = "Header send failed";
    return result;
  }
  if (len > 0 && !usb_bulk_write(handle, data, len, static_cast<int>(timeout_ms))) {
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
