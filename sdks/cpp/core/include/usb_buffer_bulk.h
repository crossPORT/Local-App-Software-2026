#pragma once

#include <cstddef>
#include <cstdint>

struct libusb_device_handle;

/** Bulk OUT all of `len`. */
bool usb_bulk_write(libusb_device_handle* h, const uint8_t* data, size_t len, int timeout_ms);

/**
 * Bulk IN up to `len`. On success *got == len. On failure *got is bytes read
 * before the error (may be 0).
 */
bool usb_bulk_read(libusb_device_handle* h, uint8_t* data, size_t len, int timeout_ms,
                   size_t* got);

/** Discard exactly nbytes from IN (best-effort). */
bool usb_bulk_discard(libusb_device_handle* h, uint64_t nbytes, int timeout_ms);
