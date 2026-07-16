#include "usb_buffer_bulk.h"

#include "usb_diag.h"
#include "usb_protocol.h"

#include <algorithm>
#include <cstdio>
#include <libusb-1.0/libusb.h>
#include <vector>

namespace {

thread_local char g_bulk_status[192] = "";

void set_status(const char* dir, int rc, size_t want, int xfer, size_t off, int timeout_ms) {
  std::snprintf(g_bulk_status, sizeof(g_bulk_status),
                "%s rc=%d (%s) want=%zu xfer=%d off=%zu timeout_ms=%d", dir, rc,
                libusb_strerror(static_cast<libusb_error>(rc)), want, xfer, off, timeout_ms);
}

}  // namespace

const char* usb_last_bulk_status() { return g_bulk_status; }

bool usb_bulk_write(libusb_device_handle* h, const uint8_t* data, size_t len, int timeout_ms) {
  g_bulk_status[0] = '\0';
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
    if (rc != LIBUSB_SUCCESS) {
      set_status("OUT", rc, n, xfer, off, timeout_ms);
      return false;
    }
    off += static_cast<size_t>(xfer);
  }
  return true;
}

bool usb_bulk_read(libusb_device_handle* h, uint8_t* data, size_t len, int timeout_ms,
                   size_t* got) {
  g_bulk_status[0] = '\0';
  if (got) *got = 0;
  size_t off = 0;
  while (off < len) {
    int xfer = 0;
    const size_t n = std::min(len - off, usb_protocol::kChunkSize);
    const int rc =
        libusb_bulk_transfer(h, usb_protocol::kEndpointDataIn, data + off, static_cast<int>(n),
                             &xfer, timeout_ms);
    if (rc != LIBUSB_SUCCESS) {
      set_status("IN", rc, n, xfer, off, timeout_ms);
      USB_DIAG("[USB-DIAG] buffer_read %s\n", g_bulk_status);
      if (got) *got = off;
      return false;
    }
    off += static_cast<size_t>(xfer);
    if (got) *got = off;
  }
  return true;
}

bool usb_bulk_discard(libusb_device_handle* h, uint64_t nbytes, int timeout_ms) {
  std::vector<uint8_t> scratch(std::min<size_t>(nbytes, usb_protocol::kChunkSize));
  uint64_t left = nbytes;
  while (left > 0) {
    const size_t n = std::min<uint64_t>(left, scratch.size());
    size_t got = 0;
    if (!usb_bulk_read(h, scratch.data(), n, timeout_ms, &got)) {
      return false;
    }
    left -= got;
  }
  return true;
}
