#pragma once

#include <cstdio>
#include <cstdlib>

/** Verbose USB libusb traces. Off by default; set ROCKETBOX_USB_DIAG=1. */
inline bool usb_diag_enabled() {
  static const bool on = [] {
    const char* e = std::getenv("ROCKETBOX_USB_DIAG");
    return e && e[0] != '\0' && e[0] != '0';
  }();
  return on;
}

#define USB_DIAG(...)                 \
  do {                                \
    if (usb_diag_enabled()) {         \
      std::fprintf(stderr, __VA_ARGS__); \
    }                                 \
  } while (0)
