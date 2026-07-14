#pragma once

#include <string>

struct libusb_context;
struct libusb_device_handle;

// Shared by rocketbox_usb_core TUs (not part of the public transfer API).
// reset_data_endpoints: clear IN/OUT halts after claim. Keep true for App file
// transfers; false for tunnel streaming (reopens often — halt clears desync).
libusb_device_handle* open_device_by_index(libusb_context* ctx,
                                           int index,
                                           std::string* error_out = nullptr,
                                           int max_open_attempts = 5,
                                           bool reset_data_endpoints = true);

void close_device(libusb_device_handle* handle);
