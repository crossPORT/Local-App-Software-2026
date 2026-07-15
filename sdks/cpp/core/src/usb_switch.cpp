#include "usb_transfer.h"
#include "usb_transfer_handle.h"
#include "usb_device_open.h"
#include "usb_protocol.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <libusb-1.0/libusb.h>
#include <string>

namespace {

std::atomic<bool> g_ep4_dynamic_switch{usb_protocol::kEp4DynamicSwitchDefault};

}  // namespace

void set_ep4_dynamic_switch_enabled(bool enabled) {
  g_ep4_dynamic_switch.store(enabled, std::memory_order_release);
}

bool ep4_dynamic_switch_enabled() {
  return g_ep4_dynamic_switch.load(std::memory_order_acquire);
}

TransferResult switch_port_on_handle(libusb_device_handle* handle, int dest_port) {
  TransferResult result{};
  if (!handle) {
    result.error_message = "null handle";
    return result;
  }
  // No EP4 traffic until settings / --ep4-switch enable dynamic routing.
  if (!ep4_dynamic_switch_enabled()) {
    (void)dest_port;
    result.ok = true;
    result.expected_bytes = usb_protocol::kSwitchPacketSize;
    return result;
  }
  libusb_clear_halt(handle, usb_protocol::kEndpointCtrlOut);

  std::array<uint8_t, usb_protocol::kSwitchPacketSize> pkt{};
  pkt[0] = static_cast<uint8_t>(dest_port & 0x0F);

  int transferred = 0;
  const auto t_start = std::chrono::steady_clock::now();
  const int r = libusb_bulk_transfer(
      handle, usb_protocol::kEndpointCtrlOut, pkt.data(), static_cast<int>(pkt.size()),
      &transferred, static_cast<unsigned>(usb_protocol::kSwitchTimeoutMs));
  result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
  result.bytes_transferred = static_cast<uint64_t>(transferred);
  result.expected_bytes = pkt.size();
  if (r != LIBUSB_SUCCESS) {
    result.error_message = std::string("EP4 write failed: ") + libusb_error_name(r);
  } else if (transferred != static_cast<int>(pkt.size())) {
    result.error_message =
        "Short write to EP4: " + std::to_string(transferred) + "/" + std::to_string(pkt.size());
  } else {
    result.ok = true;
  }
  return result;
}

TransferResult switch_port_core(libusb_context* ctx, int port_index, int dest_port,
                                bool reset_data_endpoints) {
  TransferResult result{};
  libusb_device_handle* handle =
      open_device_by_index(ctx, port_index, &result.error_message, 5, reset_data_endpoints);
  if (!handle) {
    if (result.error_message.empty()) {
      result.error_message =
          "Could not open USB device at port index " + std::to_string(port_index);
    }
    return result;
  }
  result = switch_port_on_handle(handle, dest_port);
  close_device(handle);
  return result;
}
