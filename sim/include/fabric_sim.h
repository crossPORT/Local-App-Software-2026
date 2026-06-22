#pragma once

#include "usb_transfer.h"

#include <cstdint>
#include <string>
#include <vector>

// In-process fabric emulator: two linked ports (0 ↔ 1) with ROCKETBX framing.
// Enable with ROCKETBOX_SIM=1 or fabric_sim_set_enabled(true) (tests).
// Lives outside core/ — lib/tools link fabric_usb_sim when sim support is needed.

bool fabric_sim_enabled();
void fabric_sim_set_enabled(bool enabled);
void fabric_sim_reset();

TransferResult fabric_sim_send_file(const std::string& path,
                                    int port_index,
                                    ProgressCallback progress_cb,
                                    unsigned timeout_ms);

TransferResult fabric_sim_receive_file(const std::string& out_path,
                                       int port_index,
                                       ProgressCallback progress_cb,
                                       unsigned header_timeout_ms);

TransferResult fabric_sim_loopback(const std::string& path,
                                   int send_port_index,
                                   int recv_port_index,
                                   ProgressCallback progress_cb);

int fabric_sim_count_devices();
bool fabric_sim_port_available(int port_index);
bool fabric_sim_device_bus_addr(int port_index, uint8_t* bus_out, uint8_t* addr_out);
std::vector<FabricUsbDevice> fabric_sim_list_devices();
