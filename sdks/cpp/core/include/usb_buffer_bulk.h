#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct libusb_device_handle;

/** Last bulk IN/OUT failure detail (thread-local). Empty if last op succeeded. */
const char* usb_last_bulk_status();

/** Bulk OUT all of `len`. On failure, usb_last_bulk_status() is set. */
bool usb_bulk_write(libusb_device_handle* h, const uint8_t* data, size_t len, int timeout_ms);

/**
 * Bulk IN up to `len`. On success *got == len. On failure *got is bytes read
 * before the error (may be 0); usb_last_bulk_status() is set.
 */
bool usb_bulk_read(libusb_device_handle* h, uint8_t* data, size_t len, int timeout_ms,
                   size_t* got);

/** Discard exactly nbytes from IN (best-effort). */
bool usb_bulk_discard(libusb_device_handle* h, uint64_t nbytes, int timeout_ms);
