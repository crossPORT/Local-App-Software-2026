#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "usb_protocol.h"

struct libusb_context;

struct TransferResult {
    bool        ok;
    uint64_t    bytes_transferred;
    uint64_t    expected_bytes;
    double      seconds;
    double      mbps;
    std::string error_message;
};

using ProgressCallback = std::function<void(uint64_t done,
                                            uint64_t total,
                                            double   elapsed_secs)>;

// Per-chunk payload transfer timeout (data chunks; not session/header reads).
// Defaults to usb_protocol::kFileTimeoutMs. Passing 0 restores the default.
// Lets a deployment lower the stall backstop via config without recompiling.
void set_payload_timeout_ms(unsigned ms);
unsigned payload_timeout_ms();

/** When false (default), EP4 switch writes are no-ops. Settings / --ep4-switch override. */
void set_ep4_dynamic_switch_enabled(bool enabled);
bool ep4_dynamic_switch_enabled();

// In-flight buffer budget (per process) in MB. 0 = auto-detect from the kernel
// usbfs limit so submits never exceed the pool (LIBUSB_ERROR_NO_MEM). Raise it
// when usbfs_memory_mb has been increased, for higher throughput.
void set_inflight_budget_mb(unsigned mb);
// Resolved number of chunks kept in flight given the current budget / usbfs cap.
unsigned inflight_queue_depth();
// Kernel usbfs memory cap in MB (global, shared across processes).
unsigned usbfs_limit_mb();

TransferResult send_file_core(libusb_context* ctx,
                              const std::string& path,
                              int port_index,
                              ProgressCallback progress_cb = nullptr,
                              unsigned timeout_ms = usb_protocol::kFileTimeoutMs,
                              uint8_t frame_kind = usb_protocol::kFrameKindPayload,
                              const std::string& header_filename = {},
                              bool reset_data_endpoints = true);

TransferResult receive_file_core(libusb_context* ctx,
                                 const std::string& out_path,
                                 int port_index,
                                 ProgressCallback progress_cb = nullptr,
                                 unsigned header_timeout_ms = usb_protocol::kFileTimeoutMs,
                                 uint8_t expected_frame_kind = usb_protocol::kFrameKindPayload,
                                 bool reset_data_endpoints = true);

// In-memory ROCKETBX transfer (tunnel / datagram path). Same wire format as files.
TransferResult send_buffer_core(libusb_context* ctx, const uint8_t* data, size_t len,
                                int port_index, unsigned timeout_ms = usb_protocol::kFileTimeoutMs,
                                uint8_t frame_kind = usb_protocol::kFrameKindPayload,
                                const char* filename = nullptr,
                                bool reset_data_endpoints = true);

TransferResult receive_buffer_core(libusb_context* ctx, std::vector<uint8_t>* out, int port_index,
                                   unsigned header_timeout_ms = usb_protocol::kFileTimeoutMs,
                                   uint8_t expected_frame_kind = usb_protocol::kFrameKindPayload,
                                   bool reset_data_endpoints = true,
                                   unsigned payload_timeout_ms_arg = 0);

struct RocketBoxUsbDevice {
    uint8_t bus = 0;
    uint8_t addr = 0;
};

// All matching devices, sorted by bus then address (libusb index, not silkscreen).
std::vector<RocketBoxUsbDevice> list_rocketbox_devices(libusb_context* ctx);

// Returns how many 1772:0006 devices are connected (0, 1, 2, ...).
int count_rocketbox_devices(libusb_context* ctx);

// True when this port index can be opened and claimed right now (not just enumerated).
bool rocketbox_port_available(libusb_context* ctx, int port_index);

// Pick the fabric sort index for this process: 0 when one cable; otherwise the
// first index that can be opened now. Returns -1 when none are connected.
int resolve_rocketbox_port_index(libusb_context* ctx);

// Bus/address for the Nth matching device (port_index). False if unplugged.
bool rocketbox_device_bus_addr(libusb_context* ctx,
                            int port_index,
                            uint8_t* bus_out,
                            uint8_t* addr_out);

// USB serial string for the Nth matching RocketBox device (empty if unavailable).
std::string rocketbox_device_serial(libusb_context* ctx, int port_index);

// Port switch on EP4 (TS writeSwitch parity). dest_port 1–4 links, 0 clears.
// No EP3 reply. reset_data_endpoints: App true; tunnel stream mode false.
TransferResult switch_port_core(libusb_context* ctx, int port_index, int dest_port,
                                bool reset_data_endpoints = true);

// Send on send_port_index, receive on recv_port_index in parallel (same-PC loopback).
TransferResult loopback_transfer_core(libusb_context* ctx,
                                      const std::string& path,
                                      int send_port_index,
                                      int recv_port_index,
                                      ProgressCallback progress_cb = nullptr);
