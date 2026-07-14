#pragma once

#include "usb_transfer.h"

struct libusb_device_handle;

// In-memory ROCKETBX on an already-claimed handle (tunnel stream mode).
TransferResult send_buffer_on_handle(libusb_device_handle* handle, const uint8_t* data, size_t len,
                                     unsigned timeout_ms = usb_protocol::kFileTimeoutMs,
                                     uint8_t frame_kind = usb_protocol::kFrameKindPayload,
                                     const char* filename = nullptr);

TransferResult receive_buffer_on_handle(libusb_device_handle* handle, std::vector<uint8_t>* out,
                                        unsigned header_timeout_ms = usb_protocol::kFileTimeoutMs,
                                        uint8_t expected_frame_kind = usb_protocol::kFrameKindPayload);

TransferResult switch_port_on_handle(libusb_device_handle* handle, int dest_port);
