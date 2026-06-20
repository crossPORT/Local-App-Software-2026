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
                              unsigned timeout_ms = usb_protocol::kFileTimeoutMs);

TransferResult receive_file_core(libusb_context* ctx,
                                 const std::string& out_path,
                                 int port_index,
                                 ProgressCallback progress_cb = nullptr,
                                 unsigned header_timeout_ms = usb_protocol::kFileTimeoutMs);

struct FabricUsbDevice {
    uint8_t bus = 0;
    uint8_t addr = 0;
};

// All matching devices, sorted by bus then address (libusb index, not silkscreen).
std::vector<FabricUsbDevice> list_fabric_devices(libusb_context* ctx);

// Returns how many 1772:0006 devices are connected (0, 1, 2, ...).
int count_fabric_devices(libusb_context* ctx);

// True when this port index can be opened and claimed right now (not just enumerated).
bool fabric_port_available(libusb_context* ctx, int port_index);

// Bus/address for the Nth matching device (port_index). False if unplugged.
bool fabric_device_bus_addr(libusb_context* ctx,
                            int port_index,
                            uint8_t* bus_out,
                            uint8_t* addr_out);

// Send on send_port_index, receive on recv_port_index in parallel (same-PC loopback).
TransferResult loopback_transfer_core(libusb_context* ctx,
                                      const std::string& path,
                                      int send_port_index,
                                      int recv_port_index,
                                      ProgressCallback progress_cb = nullptr);
